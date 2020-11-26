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
#include <shader/ShaderCache.h>

#include <util/CascadedShadowmap.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <pbr/VulkanGltfModel.hpp>
#include <pbr/VulkanObjModel.h>
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
#include <imgui\VulkanImguiInstance.hpp>

using namespace std;
using namespace vku;

class Engine : public BaseEngine {
public:
	Engine() {
		this->windowTitle = "Bloom Demo";
		this->width = 900;
		this->height = 900;
	}

	OrbitCam* flycam;
	Scene* scene;

	RenderGraphSchema* graphSchema;
	RenderGraph* graph;
	Pass* mainPass;
	Pass* blitPass;

	VulkanObjModel* platform;
	VulkanGltfModel* gltf;
	VulkanTexture* brdf;
	VulkanTexture* irradiancemap;
	VulkanTexture* specmap;

	VulkanTexture* skybox;
	VulkanMeshBuffer* boxMeshBuf;
	VulkanMaterial* mat;
	VulkanMaterialInstance* matInst;

	VulkanImguiInstance* gui;

	std::vector<VkCommandBuffer> cmdBufs;

	struct BlurUniform {
		glm::vec2 dir;
		int pixelStep;
	};

	glm::vec4 highpassCutoff = {0.8, 0.9, 0.0, 0.0};
	float bloomIntensity = 0.35f;
	BlurUniform blurX{ {1.0, 0.0}, 6 }, blurY{ {0.0, 1.0}, 6 };

	VulkanUniform *blurXUniform, *blurYUniform, *highpassUniform, *mergePassUniform;

	// Inherited via BaseEngine
	void postInit()
	{
		boxMeshBuf = new VulkanMeshBuffer(context->device, vku::box);

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

			PassSchema* downscale = graphSchema->blitPass("highpass", "bloom/highpass.frag");
			PassSchema* blurX = graphSchema->blitPass("blur_x", "bloom/blur.frag");
			PassSchema* blurY = graphSchema->blitPass("blur_y", "bloom/blur.frag");
			PassSchema* blit = graphSchema->blitPass("blit", "bloom/merge.frag", [&](uint32_t i, const VkCommandBuffer& cb) {
				gui->Render(cb);
				});

			main->samples = msaaCount;

			AttachmentSchema* edge = graphSchema->attachment("color", main, { blit, downscale });
			edge->format = context->device->swapchain->screenFormat;
			edge->isSampled = true;
			edge->samples = msaaCount;
			edge->resolve = true;

			AttachmentSchema* depth = graphSchema->attachment("depth", main, {});
			depth->format = context->device->swapchain->depthFormat;
			depth->samples = msaaCount;
			depth->isTransient = true;
			depth->isDepth = true;

			AttachmentSchema* downscaled = graphSchema->attachment("downscaled", downscale, { blurX });
			downscaled->format = context->device->swapchain->screenFormat;
			// half screen resolution
			downscaled->width = -2;
			downscaled->height = -2;
			downscaled->isSampled = true;

			AttachmentSchema* blurredX = graphSchema->attachment("blurredX", blurX, { blurY });
			blurredX->format = context->device->swapchain->screenFormat;
			// quarter screen resolution
			blurredX->width = -4;
			blurredX->height = -4;
			blurredX->isSampled = true;

			AttachmentSchema* blurredY = graphSchema->attachment("blurredY", blurY, { blit });
			blurredY->format = context->device->swapchain->screenFormat;
			// quarter screen resolution
			blurredY->width = -4;
			blurredY->height = -4;
			blurredY->isSampled = true;

			AttachmentSchema* swap = graphSchema->attachment("swap", blit, {});
			swap->format = context->device->swapchain->screenFormat;
			swap->isSwapchain = true;
		}

		graph = new RenderGraph(graphSchema, scene, context->device->swapchain->swapChainLength);
		graph->createLayouts();

		mainPass = graph->getPass("main");
		Pass* blurXPass = graph->getPass("blur_x");
		Pass* blurYPass = graph->getPass("blur_y");
		Pass* highPass = graph->getPass("highpass");
		Pass* mergePass = graph->getPass("blit");

		blurXUniform = new VulkanUniform(context->device, sizeof(BlurUniform));
		blurYUniform = new VulkanUniform(context->device, sizeof(BlurUniform));
		highpassUniform = new VulkanUniform(context->device, sizeof(glm::vec4));
		mergePassUniform = new VulkanUniform(context->device, sizeof(float));
		for (VulkanDescriptorSet* set : blurXPass->blitPassMaterialInstance->descriptorSets) { set->write(0, blurXUniform); }
		for (VulkanDescriptorSet* set : blurYPass->blitPassMaterialInstance->descriptorSets) { set->write(0, blurYUniform); }
		for (VulkanDescriptorSet* set : highPass->blitPassMaterialInstance->descriptorSets) { set->write(0, highpassUniform); }
		for (VulkanDescriptorSet* set : mergePass->blitPassMaterialInstance->descriptorSets) { set->write(0, mergePassUniform); }



		gui = new VulkanImguiInstance(context, mergePass->pass);

		brdf = generateBRDFLUT(context->device);
		irradiancemap = generateIrradianceCube(context->device, skybox);
		specmap = generatePrefilteredCube(context->device, skybox);
		gltf = new VulkanGltfModel("res/models/DamagedHelmet.glb", scene, mainPass, brdf, irradiancemap, specmap);
		scene->addObject(gltf);

		platform = new VulkanObjModel("res/models/stand.obj", scene, mainPass, specmap, irradiancemap, brdf);
		platform->localTransform *= glm::translate(glm::vec3(0, -2, 0));
		scene->addObject(platform);

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

		cmdBufs.resize(context->device->swapchain->swapChainLength);
		for (uint32_t i = 0; i < cmdBufs.size(); i++) {
			cmdBufs[i] = context->device->createCommandBuffer();
		}

		buildSwapchainDependants();
	}

	glm::mat4 cascade = glm::mat4(1.0f);

	void updateUniforms(uint32_t i) {
		flycam->update();

		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		float n = 0.01f;
		float f = 20.0f;

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

		blurXUniform->write(&blurX);
		blurYUniform->write(&blurY);

		highpassUniform->write(&highpassCutoff);
		mergePassUniform->write(&bloomIntensity);
	}

	int timesDrawn = 0;
	VkCommandBuffer draw(uint32_t i)
	{
		updateUniforms(i);

		gui->NextFrame();
		ImGui::Begin("Bloom Demo");
		ImGui::InputInt("Blur Amount", &blurX.pixelStep);
		blurY.pixelStep = blurX.pixelStep;
		ImGui::SliderFloat("Bloom Intensity", &bloomIntensity, 0, 1);
		ImGui::SliderFloat("Bloom Cutoff A", &highpassCutoff.x, 0, 1);
		ImGui::SliderFloat("Bloom Cutoff B", &highpassCutoff.y, 0, 1);
		ImGui::End();
		gui->InternalRender();

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
		destroySwapchainDependents();

		delete gui;

		delete skybox;
		delete boxMeshBuf;
		delete matInst;
		delete mat;

		delete blurXUniform;
		delete blurYUniform;
		delete mergePassUniform;
		delete highpassUniform;

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
