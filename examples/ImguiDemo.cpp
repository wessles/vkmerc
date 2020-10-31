#include <iostream>
#include <filesystem>

#include <imgui/imgui.h>
#include <imgui/VulkanImguiInstance.hpp>

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

class Engine : public BaseEngine {
public:
	Engine() {
		this->windowTitle = "ImGui Demo";
		this->width = 900;
		this->height = 900;
	}

	OrbitCam* flycam;
	Scene* scene;

	RenderGraphSchema* graphSchema;
	RenderGraph* graph;
	Pass* mainPass;
	Pass* blitPass;

	VulkanImguiInstance* guiInstance;

	VulkanMeshBuffer* blitMeshBuf;
	VulkanMaterial* blitMat;
	VulkanMaterialInstance* blitMatInst;

	VulkanGltfModel* gltf;
	VulkanTexture* brdf;
	VulkanTexture* irradiancemap;
	VulkanTexture* specmap;

	VulkanTexture* skybox;
	VulkanMeshBuffer* boxMeshBuf;
	VulkanMaterial* mat;
	VulkanMaterialInstance* matInst;

	std::vector<VkCommandBuffer> cmdBufs;

	// Inherited via BaseEngine
	void postInit()
	{
		boxMeshBuf = new VulkanMeshBuffer(context->device, vku::box);
		blitMeshBuf = new VulkanMeshBuffer(context->device, vku::blit);

		SceneInfo sInfo{};
		scene = new Scene(context->device, sInfo);

		// load skybox texture
		skybox = new VulkanTexture;
		skybox->image = new VulkanImage(context->device, {
			"res/textures/cubemap_day/posx.jpg",
			"res/textures/cubemap_day/negx.jpg",
			"res/textures/cubemap_day/posy.jpg",
			"res/textures/cubemap_day/negy.jpg",
			"res/textures/cubemap_day/posz.jpg",
			"res/textures/cubemap_day/negz.jpg"
			});
		VulkanImageViewInfo viewInfo{};
		skybox->image->writeImageViewInfo(&viewInfo);
		viewInfo.imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
		skybox->view = new VulkanImageView(context->device, viewInfo);
		skybox->sampler = new VulkanSampler(context->device, {});

		flycam = new OrbitCam(context->windowHandle);

		graphSchema = new RenderGraphSchema();
		{
			VkSampleCountFlagBits msaaCount = VK_SAMPLE_COUNT_8_BIT;

			PassSchema* main = graphSchema->pass("main", [&](uint32_t i, const VkCommandBuffer& cb) {
				matInst->bind(cb, i);
				matInst->material->bind(cb);
				boxMeshBuf->draw(cb);

				scene->render(cb, i, false);
				});

			PassSchema* blit = graphSchema->pass("blit", [&](uint32_t i, const VkCommandBuffer& cb) {
				blitMatInst->bind(cb, i);
				blitMatInst->material->bind(cb);
				blitMeshBuf->draw(cb);

				guiInstance->Render(cb);
				});

			main->samples = msaaCount;

			AttachmentSchema* edge = graphSchema->attachment("color", main, { blit });
			edge->format = context->device->swapchain->screenFormat;
			edge->isSampled = true;
			edge->samples = msaaCount;
			edge->resolve = true;

			AttachmentSchema* depth = graphSchema->attachment("depth", main, {});
			depth->format = context->device->swapchain->depthFormat;
			depth->samples = msaaCount;
			depth->isTransient = true;
			depth->isDepth = true;

			AttachmentSchema* swap = graphSchema->attachment("swap", blit, {});
			swap->format = context->device->swapchain->screenFormat;
			swap->isSwapchain = true;
		}

		graph = new RenderGraph(graphSchema, scene, context->device->swapchain->swapChainLength);
		graph->createLayouts();

		mainPass = graph->getPass("main");
		blitPass = graph->getPass("blit");

		brdf = generateBRDFLUT(context->device);
		irradiancemap = generateIrradianceCube(context->device, skybox);
		specmap = generatePrefilteredCube(context->device, skybox);
		gltf = new VulkanGltfModel("res/models/DamagedHelmet.glb", scene, mainPass, brdf, irradiancemap, specmap);
		scene->addObject(gltf);

		// skyboxes don't care about depth testing / writing
		VulkanMaterialInfo matInfo(context->device);
		matInfo.depthStencil.depthWriteEnable = false;
		matInfo.depthStencil.depthTestEnable = false;
		matInfo.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		matInfo.shaderStages.push_back({ "skybox/skybox.frag", {} });
		matInfo.shaderStages.push_back({ "skybox/skybox.vert", {} });
		mat = new VulkanMaterial(&matInfo, scene, mainPass);
		matInst = new VulkanMaterialInstance(mat);
		for (VulkanDescriptorSet* set : matInst->descriptorSets) { set->write(0, skybox); }

		VulkanMaterialInfo blitMatInfo(context->device);
		blitMatInfo.shaderStages.push_back({ "blit/blit.frag", {} });
		blitMatInfo.shaderStages.push_back({ "blit/blit.vert", {} });
		blitMat = new VulkanMaterial(&blitMatInfo, scene, blitPass);
		blitMatInst = new VulkanMaterialInstance(blitMat);

		cmdBufs.resize(context->device->swapchain->swapChainLength);
		for (uint32_t i = 0; i < cmdBufs.size(); i++) {
			cmdBufs[i] = context->device->createCommandBuffer();
		}

		guiInstance = new VulkanImguiInstance(context, blitPass->pass);

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

		SceneGlobalUniform global{};
		glm::mat4 transform = flycam->getTransform();
		global.view = glm::inverse(transform);
		VkExtent2D swapchainExtent = context->device->swapchain->swapChainExtent;
		global.proj = flycam->getProjMatrix(static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), n, f);
		global.camPos = transform * glm::vec4(0.0, 0.0, 0.0, 1.0);
		global.screenRes = { swapchainExtent.width, swapchainExtent.height };
		global.time = time;
		global.directionalLight = glm::rotate(glm::mat4(1.0), time, glm::vec3(0.0, 1.0, 0.0)) * glm::vec4(1.0, -1.0, 0.0, 0.0);

		scene->updateUniforms(i, 0, &global);
	}

	int timesDrawn = 0;
	VkCommandBuffer draw(uint32_t i)
	{
		updateUniforms(i);

		guiInstance->NextFrame();
		ImGui::ShowDemoWindow();
		guiInstance->InternalRender();

		if (timesDrawn >= cmdBufs.size())
			vkResetCommandBuffer(cmdBufs[i], 0);
		else
			timesDrawn++;
		cmdBufs[i] = context->device->beginCommandBuffer();
		graph->render(cmdBufs[i], i);
		vkEndCommandBuffer(cmdBufs[i]);

		return cmdBufs[i];
	}

	void buildSwapchainDependants()
	{
		graph->createInstances();
	}
	void destroySwapchainDependents()
	{
		graph->destroyInstances();
	}
	void preCleanup()
	{
		delete guiInstance;

		destroySwapchainDependents();

		delete skybox;
		delete boxMeshBuf;
		delete matInst;
		delete mat;

		delete blitMatInst;
		delete blitMat;
		delete blitMeshBuf;

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
