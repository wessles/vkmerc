#include "Scene.h"

#include <stdexcept>

#include <glm/glm.hpp>

#include "../VulkanDevice.h";
#include "../VulkanSwapchain.h"
#include "../VulkanDescriptorSetLayout.h"


namespace vku {
	Scene::Scene(VulkanDevice* device, SceneInfo info) {
		this->device = device;
		this->globalDescriptorSetLayout = new VulkanDescriptorSetLayout(device, info.globalDescriptors);

		{
			VkPushConstantRange range{};
			range.offset = 0;
			range.size = sizeof(glm::mat4);
			range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

			VkPipelineLayoutCreateInfo pipelineLayoutCI{};
			pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCI.setLayoutCount = 1;
			VkDescriptorSetLayout layout = *globalDescriptorSetLayout;
			pipelineLayoutCI.pSetLayouts = &layout;
			pipelineLayoutCI.pushConstantRangeCount = 1;
			pipelineLayoutCI.pPushConstantRanges = &range;

			if (vkCreatePipelineLayout(*device, &pipelineLayoutCI, nullptr, &globalPipelineLayout) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create pipeline layout.");
			}
		}

		uint32_t n = this->device->swapchain->swapChainLength;
		this->globalDescriptorSets.resize(n);
		for (uint32_t i = 0; i < n; i++) {
			this->globalDescriptorSets[i] = globalDescriptorSetLayout->createDescriptorSet();
		}
	}

	Scene::~Scene() {
		vkDestroyPipelineLayout(*device, globalPipelineLayout, nullptr);
		delete globalDescriptorSetLayout;
	}
}