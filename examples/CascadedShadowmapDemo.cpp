#include <iostream>
#include <filesystem>

#include <VulkanContext.h>
#include <VulkanDevice.h>
#include <VulkanSwapchain.h>
#include <VulkanTexture.h>
#include <VulkanDescriptorSet.h>
#include <VulkanMesh.h>
#include <VulkanMaterial.h>
#include <VulkanUniform.h>

#include <util/CascadedShadowmap.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <pbr/VulkanGltfModel.hpp>
#include <pbr/Ue4BrdfLut.hpp>
#include <pbr/CubemapFiltering.hpp>

#include <rendergraph/RenderGraph.h>
#include <scene/Scene.h>
#include <BaseEngine.h>

#include <util/OrbitCam.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace std;
using namespace vku;

struct Shadowmap1CascadeUniform {
	glm::mat4 cascade0;
};
struct Shadowmap2CascadeUniform {
	glm::mat4 cascade0, cascade1;
};

class Engine : public BaseEngine {
	const uint32_t shadowmapDimensions = 256;

public:
	Engine() {
		this->windowTitle = "Cascaded Shadowmap Demo";
		//this->debugEnabled = false;
		this->width = 900;
		this->height = 900;
	}

	OrbitCam* flycam;
	Scene* scene;

	RenderGraph* graph;
	Pass* cascades;
	Pass* main;
	Pass* blit;

	VulkanGltfModel* gltf;
	VulkanTexture* brdf;
	VulkanTexture* irradiancemap;
	VulkanTexture* specmap;

	VulkanTexture* skybox;
	VulkanMeshBuffer* boxMeshBuf;
	VulkanMaterial* mat;
	VulkanMaterialInstance* matInst;

	std::vector<VulkanUniform*> cascadeUniform;
	std::vector<VulkanMaterial*> cascadeMats;
	std::vector<VulkanMaterialInstance*> cascadeMatInsts;

	std::vector<VkCommandBuffer> cmdBufs;

	// Inherited via BaseEngine
	void postInit()
	{
		boxMeshBuf = new VulkanMeshBuffer(context->device, vku::box);

		SceneInfo sInfo{};
		scene = new Scene(context->device, sInfo);

		// load skybox texture
		skybox = new VulkanTexture;
		skybox->image = new VulkanImage(context->device, {
			"res/cubemap/posx.jpg",
			"res/cubemap/negx.jpg",
			"res/cubemap/posy.jpg",
			"res/cubemap/negy.jpg",
			"res/cubemap/posz.jpg",
			"res/cubemap/negz.jpg"
			});
		VulkanImageViewInfo viewInfo{};
		skybox->image->writeImageViewInfo(&viewInfo);
		viewInfo.imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
		skybox->view = new VulkanImageView(context->device, viewInfo);
		skybox->sampler = new VulkanSampler(context->device, {});

		flycam = new OrbitCam(context->windowHandle);

		cascadeUniform.resize(context->device->swapchain->swapChainLength);
		for (uint32_t i = 0; i < cascadeUniform.size(); i++) {
			cascadeUniform[i] = new VulkanUniform(context->device, sizeof(Shadowmap1CascadeUniform));
		}

		graph = new RenderGraph(scene, context->device->swapchain->swapChainLength);
		{
			cascades = graph->pass([&](uint32_t i, const VkCommandBuffer& cb) {
				cascadeMatInsts[0]->bind(cb, i);
				cascadeMatInsts[0]->material->bind(cb);

				scene->render(cb, i, true);
			});

			main = graph->pass([&](uint32_t i, const VkCommandBuffer& cb) {
				matInst->bind(cb, i);
				matInst->material->bind(cb);
				boxMeshBuf->draw(cb);

				scene->render(cb, i, false);
			});

			blit = graph->pass([&](uint32_t i, const VkCommandBuffer& cb) {
				matInst->bind(cb, i);
				matInst->material->bind(cb);
				boxMeshBuf->draw(cb);

				scene->render(cb, i, false);
			});

			graph->begin(main);
			graph->terminate(main);

			Attachment* shadowmap = graph->attachment(cascades, { main });
			shadowmap->format = context->device->swapchain->depthFormat;
			shadowmap->samples = VK_SAMPLE_COUNT_1_BIT;
			shadowmap->sizeToSwapchain = false;
			shadowmap->width = shadowmapDimensions;
			shadowmap->height = shadowmapDimensions;

			Attachment* edge = graph->attachment(main, {});
			edge->format = context->device->swapchain->screenFormat;
			edge->samples = VK_SAMPLE_COUNT_1_BIT;
			edge->isSwapchain = true;

			Attachment* depth = graph->attachment(main, {});
			depth->format = context->device->swapchain->depthFormat;
			depth->samples = VK_SAMPLE_COUNT_1_BIT;
			depth->transient = true;
		}
		graph->createLayouts();

		brdf = generateBRDFLUT(context->device);
		irradiancemap = generateIrradianceCube(context->device, skybox);
		specmap = generatePrefilteredCube(context->device, skybox);
		gltf = new VulkanGltfModel("res/demo/DamagedHelmet.glb", scene, main, brdf, irradiancemap, specmap);
		scene->addObject(gltf);

		// skyboxes don't care about depth testing / writing
		VulkanMaterialInfo matInfo(context->device);
		matInfo.depthStencil.depthWriteEnable = false;
		matInfo.depthStencil.depthTestEnable = false;
		matInfo.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		matInfo.shaderStages.push_back({ "res/shaders/skybox/skybox.frag", VK_SHADER_STAGE_FRAGMENT_BIT, {} });
		matInfo.shaderStages.push_back({ "res/shaders/skybox/skybox.vert", VK_SHADER_STAGE_VERTEX_BIT, {} });
		mat = new VulkanMaterial(&matInfo, scene, main);
		matInst = new VulkanMaterialInstance(mat);
		for (VulkanDescriptorSet* set : matInst->descriptorSets) { set->write(0, skybox); }

		// depth only pipeline
		VulkanMaterialInfo cascadeMatInfo(context->device);
		cascadeMatInfo.shaderStages.push_back( { "res/shaders/shadowpass/shadowpass.vert", VK_SHADER_STAGE_VERTEX_BIT, {{"CASCADE_0", ""}} } );
		cascadeMats.resize(1);
		cascadeMatInsts.resize(1);
		cascadeMats[0] = new VulkanMaterial(&cascadeMatInfo, scene, cascades);
		cascadeMatInsts[0] = new VulkanMaterialInstance(cascadeMats[0]);

		buildSwapchainDependants();
	}

	glm::mat4 cascade = glm::mat4(1.0f);

	void updateUniforms(uint32_t i) {
		flycam->update();

		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		float n = 0.01f;
		float f = 5.0f;
		float half = (n + f) / 2.0f;
		float quarter = (n + half) / 2.0f;

		SceneGlobalUniform global{};
		glm::mat4 transform = flycam->getTransform();
		global.view = glm::inverse(transform);
		VkExtent2D swapchainExtent = context->device->swapchain->swapChainExtent;
		global.proj = flycam->getProjMatrix(static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), n, f);
		global.camPos = transform * glm::vec4(0.0, 0.0, 0.0, 1.0);
		global.screenRes = { swapchainExtent.width, swapchainExtent.height };
		global.time = time;
		global.directionalLight = glm::rotate(glm::mat4(1.0), time, glm::vec3(0.0, 1.0, 0.0)) * glm::vec4(1.0, -1.0, 0.0, 0.0);

		//glm::mat4 subfrust0 = glm::inverse(flycam->getProjMatrix(static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), n, quarter));
		//glm::mat4 subfrust1 = glm::inverse(flycam->getProjMatrix(static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), quarter, half));
		//glm::mat4 subfrust2 = glm::inverse(flycam->getProjMatrix(static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), half, f));
		//global.cascade0 = fitLightProjMatToCameraFrustum(subfrust0, global.directionalLight, shadowmapDimensions);
		//global.cascade1 = fitLightProjMatToCameraFrustum(subfrust1, global.directionalLight, shadowmapDimensions);
		//global.cascade2 = fitLightProjMatToCameraFrustum(subfrust2, global.directionalLight, shadowmapDimensions);

		scene->updateUniforms(i, &global);
	}

	VkCommandBuffer draw(uint32_t i)
	{
		updateUniforms(i);

		return cmdBufs[i];
	}
	void buildSwapchainDependants()
	{
		graph->createInstances();

		vkFreeCommandBuffers(*context->device, context->device->commandPool, static_cast<uint32_t>(cmdBufs.size()), cmdBufs.data());
		cmdBufs.resize(context->device->swapchain->swapChainLength);
		for (uint32_t i = 0; i < cmdBufs.size(); i++) {
			cmdBufs[i] = context->device->beginCommandBuffer();
			graph->render(cmdBufs[i], i);
			vkEndCommandBuffer(cmdBufs[i]);
		}
	}
	void destroySwapchainDependents()
	{
		graph->destroyInstances();
	}
	void preCleanup()
	{
		destroySwapchainDependents();

		delete skybox;
		delete boxMeshBuf;
		delete matInst;
		delete mat;

		for (auto* mat : cascadeMats) {
			delete mat;
		}
		for (auto* matInst : cascadeMatInsts) {
			delete matInst;
		}

		delete flycam;

		delete brdf;
		delete irradiancemap;
		delete specmap;

		graph->destroyLayouts();
		delete scene;
	}
};

int main()
{
	Engine* demo = new Engine();
	demo->run();
	return 0;
}
