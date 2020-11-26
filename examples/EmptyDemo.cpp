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
#include <shader/ShaderVariant.h>

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
		this->windowTitle = "Empty Demo";
		this->width = 900;
		this->height = 900;
	}

	OrbitCam* flycam;
	Scene* scene;

	RenderGraphSchema* graphSchema;
	RenderGraph* graph;
	Pass* mainPass;

	std::vector<VkCommandBuffer> cmdBufs;

	// Inherited via BaseEngine
	void postInit()
	{
		SceneInfo sInfo{};
		scene = new Scene(context->device, sInfo);

		flycam = new OrbitCam(context->windowHandle);

		graphSchema = new RenderGraphSchema();
		{
			VkSampleCountFlagBits msaaCount = VK_SAMPLE_COUNT_8_BIT;

			AttachmentSchema* color = graphSchema->attachment("color");
			color->format = context->device->swapchain->screenFormat;
			color->isSwapchain = true;

			PassSchema* main = graphSchema->pass("main");

			main->write(0, color);


		}

		graph = new RenderGraph(graphSchema, scene, context->device->swapchain->swapChainLength);
		graph->createLayouts();

		mainPass = graph->getPass("main");

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

		scene->updateUniforms(i, 0, &global);
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

		delete flycam;

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
