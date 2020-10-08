#include "Scene.h"

#include <stdexcept>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "../VulkanDevice.h";
#include "../VulkanSwapchain.h"
#include "../VulkanUniform.h"
#include "../VulkanDescriptorSetLayout.h"


namespace vku {
	Scene::Scene(VulkanDevice* device, SceneInfo info) {
		this->device = device;
		this->globalDescriptorSetLayout = new VulkanDescriptorSetLayout(device, { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS} });

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

		this->globalUniforms = new VulkanUniform(device, n, sizeof(SceneGlobalUniform));
		
		this->globalDescriptorSets.resize(n);
		for (uint32_t i = 0; i < n; i++) {
			globalDescriptorSets[i] = globalDescriptorSetLayout->createDescriptorSet();
			VkWriteDescriptorSet descriptorSetWrite = globalUniforms->getDescriptorWrite(i, globalDescriptorSets[i]);
			vkUpdateDescriptorSets(*device, 1, &descriptorSetWrite, 0, nullptr);
		}
	}

	Scene::~Scene() {
		vkDestroyPipelineLayout(*device, globalPipelineLayout, nullptr);
		delete globalDescriptorSetLayout;
		delete globalUniforms;
	}

	void Scene::updateUniforms(uint32_t i, SceneGlobalUniform *uniform) {
		globalUniforms->write(i, uniform);
	}
}