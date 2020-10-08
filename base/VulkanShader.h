#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace vku {
	struct VulkanDevice;

	struct VulkanShader {
		VulkanDevice* device;
		VkShaderModule handle;

		VulkanShader(VulkanDevice* device, std::vector<uint32_t> data);
		VkPipelineShaderStageCreateInfo VulkanShader::getShaderStage(VkShaderStageFlagBits stage);
		~VulkanShader();

		operator VkShaderModule () const { return handle; }
	};

	/*
	Loads spirv shader binary. Before doing so, compiles glsl file
	at path IF spv is nonexistent or out of date.
	*/
	std::vector<uint32_t> lazyLoadSpirv(const std::string& path);
}