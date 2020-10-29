/* This class is a stub for later implementation. */

#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "VulkanShader.h"

namespace vku {
	struct VulkanDevice;
	struct Scene;
	struct Pass;
	struct VulkanDescriptorSetLayout;
	struct VulkanDescriptorSet;
	struct VulkanMaterial;

	struct VulkanComputeInfo {
		std::string shaderFilename;
		std::vector<ShaderMacro> macros;
		VkShaderStageFlagBits stage = VK_SHADER_STAGE_COMPUTE_BIT;

		std::vector<VkPushConstantRange> pushConstRanges{};

		VkComputePipelineCreateInfo computeInfo{};

		VulkanDevice* device;

		VulkanComputeInfo(VulkanDevice* const device);
	};

	struct VulkanCompute {
		Scene* scene;

		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;

		VulkanDescriptorSetLayout* descriptorSetLayout;

		VulkanCompute(VulkanComputeInfo* const info, Scene* scene, Pass* pass);
		VulkanCompute() {}
		~VulkanCompute();
	};

	struct VulkanComputeInstance {
		VulkanCompute* compute;

		std::vector<VulkanDescriptorSet*> descriptorSets;

		VulkanComputeInstance(VulkanCompute* mat);

		~VulkanComputeInstance();
	};
}