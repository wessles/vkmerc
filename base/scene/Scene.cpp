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
		std::vector<DescriptorLayout> descriptorLayouts{};
		for (uint32_t i = 0; i < info.uniformAllocSizes.size(); i++) {
			descriptorLayouts.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS });
		}

		this->globalDescriptorSetLayout = new VulkanDescriptorSetLayout(device, descriptorLayouts);

		{
			VkPushConstantRange range{};
			range.offset = 0;
			range.size = 128;
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
			globalDescriptorSets[i] = new VulkanDescriptorSet(globalDescriptorSetLayout);
			globalUniforms[i].resize(info.uniformAllocSizes.size());
			for (uint32_t k = 0; k < globalUniforms[i].size(); k++) {
				globalUniforms[i][k] = new VulkanUniform(device, info.uniformAllocSizes[k]);
				globalDescriptorSets[i]->write(k, globalUniforms[i][k]);
			}
		}
	}

	Scene::~Scene() {
		for (Object* obj : objects) {
			delete obj;
		}

		vkDestroyPipelineLayout(*device, globalPipelineLayout, nullptr);
		delete globalDescriptorSetLayout;
		for (auto uniforms : globalUniforms) {
			for (VulkanUniform *uniform : uniforms) {
				delete uniform;
			}
		}
	}

	void Scene::addObject(Object* object) {
		objects.push_back(object);
	}

	void Scene::render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial, uint32_t layerMask) {
		for (Object* obj : objects) {
			if ((obj->layer & layerMask) != 0) {
				obj->render(cmdBuf, swapIdx, noMaterial);
			}
		}
	}

	void Scene::updateUniforms(uint32_t swapIdx, uint32_t uniformIdx, void* data) {
		globalUniforms[swapIdx][uniformIdx]->write(data);
	}

	glm::mat4 Scene::getAABBTransform() {
		glm::vec3 min = glm::vec3(std::numeric_limits<float>::infinity());
		glm::vec3 max = glm::vec3(-std::numeric_limits<float>::infinity());

		for (auto* obj : objects) {
			glm::mat4 aabb = obj->getAABBTransform();
			for (uint32_t i = 0; i < 2; i++) {
				for (uint32_t j = 0; j < 2; j++) {
					for (uint32_t k = 0; k < 2; k++) {
						glm::vec3 v = glm::vec3(aabb * glm::vec4(i,j,k,1));
						min = glm::min(v, min);
						max = glm::max(v, max);
					}
				}
			}
		}

		return glm::translate(glm::mat4(1.0f), min) * glm::scale(glm::mat4(1.0f), max - min);
	}
}