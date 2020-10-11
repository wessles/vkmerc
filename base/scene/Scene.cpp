#include "Scene.h"

#include <stdexcept>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "../VulkanDevice.h";
#include "../VulkanSwapchain.h"
#include "../VulkanUniform.h"
#include "../VulkanDescriptorSet.h"

#include "Object.h"


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

		this->globalUniforms.resize(n);
		this->globalDescriptorSets.resize(n);
		for (uint32_t i = 0; i < n; i++) {
			globalUniforms[i] = new VulkanUniform(device, sizeof(SceneGlobalUniform));
			globalDescriptorSets[i] = new VulkanDescriptorSet(globalDescriptorSetLayout);
			globalDescriptorSets[i]->write(0, globalUniforms[i]);
		}
	}

	Scene::~Scene() {
		for (Object* obj : objects) {
			delete obj;
		}

		vkDestroyPipelineLayout(*device, globalPipelineLayout, nullptr);
		delete globalDescriptorSetLayout;
		for (VulkanUniform* uni : globalUniforms)
			delete uni;
	}

	void Scene::addObject(Object* object) {
		objects.push_back(object);
	}

	void Scene::render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial) {
		for (Object* obj : objects) {
			obj->render(cmdBuf, swapIdx, noMaterial);
		}
	}

	void Scene::updateUniforms(uint32_t i, SceneGlobalUniform* uniform) {
		globalUniforms[i]->write(uniform);
	}
}