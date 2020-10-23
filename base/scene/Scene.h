#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "../VulkanDevice.h"

namespace vku {
	struct Object;

	struct DescriptorLayout;
	struct VulkanDescriptorSetLayout;
	struct VulkanDescriptorSet;
	struct VulkanUniform;

	struct SceneGlobalUniform
	{
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec4 camPos;
		glm::vec4 directionalLight;
		glm::vec2 screenRes;
		float time;
	};

	struct SceneInfo {
		// list of sizes of uniform buffers to install at binding. Default SceneGlobalUniform
		std::vector<size_t> uniformAllocSizes{ sizeof(SceneGlobalUniform) };
	};

	struct Scene {
		VulkanDevice* device;
		VulkanDescriptorSetLayout* globalDescriptorSetLayout;
		VkPipelineLayout globalPipelineLayout;
		std::vector<VulkanDescriptorSet*> globalDescriptorSets;
		std::vector<std::vector<VulkanUniform*>> globalUniforms;

		std::vector<Object*> objects{};

		Scene(VulkanDevice* device, SceneInfo info);
		~Scene();

		void addObject(Object* object);
		void render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial);

		void updateUniforms(uint32_t swapIdx, uint32_t uniformIdx, void* data);

		glm::mat4 getAABBTransform();
	};
}