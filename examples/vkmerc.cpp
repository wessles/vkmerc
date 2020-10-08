#include <iostream>
#include <filesystem>

#include <VulkanContext.h>
#include <VulkanDevice.h>
#include <VulkanSwapchain.h>
#include <VulkanImage.h>
#include <VulkanDescriptorSetLayout.h>
#include <VulkanMesh.h>
#include <VulkanMaterial.h>
#include <rendergraph/RenderGraph.h>
#include <scene/Scene.h>
#include <BaseEngine.h>

#include <FlyCam.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

using namespace std;
using namespace vku;

class Engine : public BaseEngine {
public:
	Engine() {
		this->debugEnabled = false;
	}

	FlyCam* flycam;
	Scene* scene;
	
	RenderGraph* graph;
	Pass* main;

	VulkanTexture *skybox;
	VulkanMeshBuffer* boxMeshBuf;
	Material* mat;
	MaterialInstance *matInst;

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
			"res/cubemap/negz.jpg",
			});
		VulkanImageViewInfo viewInfo{};
		skybox->image->writeImageViewInfo(&viewInfo);
		viewInfo.imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
		skybox->view = new VulkanImageView(context->device, viewInfo);
		skybox->sampler = new VulkanSampler(context->device, {});

		flycam = new FlyCam(context->windowHandle);



		graph = new RenderGraph(scene, context->device->swapchain->swapChainLength);
		{
			main = graph->pass([&](uint32_t i, const VkCommandBuffer& cb) {
				matInst->bind(cb, i);
				matInst->material->bind(cb);
				boxMeshBuf->draw(cb);
			});

			graph->begin(main);
			graph->terminate(main);

			Attachment* edge = graph->attachment(main, {});
			edge->format = context->device->swapchain->screenFormat;
			edge->samples = VK_SAMPLE_COUNT_1_BIT;
		}
		graph->createLayouts();

		mat = new Material(scene, main, { {"res/shaders/skybox/skybox.frag", VK_SHADER_STAGE_FRAGMENT_BIT} , {"res/shaders/skybox/skybox.vert", VK_SHADER_STAGE_VERTEX_BIT} });
		// skyboxes don't care about depth testing / writing
		mat->pipelineBuilder->depthStencil.depthWriteEnable = false;
		mat->pipelineBuilder->depthStencil.depthTestEnable = false;
		mat->pipelineBuilder->rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		mat->createPipeline();
		matInst = new MaterialInstance(mat);
		matInst->write(0, skybox);

		buildSwapchainDependants();
	}

	void updateUniforms(uint32_t i) {
		flycam->update();

		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		SceneGlobalUniform global{};
		glm::mat4 transform = flycam->getTransform();
		global.view = glm::inverse(transform);
		VkExtent2D swapchainExtent = context->device->swapchain->swapChainExtent;
		global.proj = flycam->getProjMatrix(static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), 0.01f, 10.0f);
		global.camPos = transform * glm::vec4(0.0, 0.0, 0.0, 1.0);
		global.screenRes = { swapchainExtent.width, swapchainExtent.height };
		global.time = time;
		global.directionalLight = glm::rotate(glm::mat4(1.0), time, glm::vec3(0.0, 1.0, 0.0)) * glm::vec4(1.0, -1.0, 0.0, 0.0);

		scene->updateUniforms(i, &global);
	}

	VkCommandBuffer draw(uint32_t swapIdx)
	{
		updateUniforms(swapIdx);
		return cmdBufs[swapIdx];
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
		delete flycam;

		graph->destroyLayouts();
		delete scene;
	}
};

int main()
{
	Engine* demo = new Engine;
	demo->run();
	return 0;
}
