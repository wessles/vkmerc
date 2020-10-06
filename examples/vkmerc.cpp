#include <iostream>

#include <VulkanContext.h>
#include <VulkanDevice.h>
#include <VulkanSwapchain.h>
#include <VulkanImage.h>
#include <VulkanDescriptorSetLayout.h>
#include <rendergraph/RenderGraph.h>
#include <scene/Scene.h>
#include <Mesh.h>
#include <BaseEngine.h>

using namespace std;
using namespace vku;

class Engine : public BaseEngine {
	Scene* scene;
	RenderGraph* graph;
	VulkanImage *image;

	std::vector<VkCommandBuffer> cmdBufs;

	// Inherited via BaseEngine
	void postInit()
	{
		VulkanContext &context = *this->context;

		SceneInfo sInfo{};
		sInfo.globalDescriptors = { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT} };
		scene = new Scene(context.device, sInfo);

		graph = new RenderGraph(scene, context.device->swapchain->swapChainLength);
		{
			Pass* main = graph->pass([](uint32_t i, const VkCommandBuffer&) {});

			graph->begin(main);
			graph->terminate(main);

			Attachment* edge = graph->attachment(main, {});
			edge->format = context.device->swapchain->screenFormat;
			edge->samples = VK_SAMPLE_COUNT_1_BIT;
		}
		graph->createLayouts();

		VulkanImageInfo imageInfo{};
		image = new VulkanImage(context.device);

		buildSwapchainDependants();
	}
	VkCommandBuffer draw(uint32_t swapIdx)
	{
		return cmdBufs[swapIdx];
	}
	void buildSwapchainDependants()
	{
		graph->createInstances();

		vkFreeCommandBuffers(*context->device, context->device->commandPool, cmdBufs.size(), cmdBufs.data());
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

		graph->destroyLayouts();
		delete scene;
	}
};

int main()
{
	Engine *demo = new Engine;
	demo->run();
	return 0;
}
