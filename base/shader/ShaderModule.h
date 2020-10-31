#pragma once

#include <vector>
#include <functional>

#include <vulkan/vulkan.h>

#include "ShaderCache.h"
#include "ShaderVariant.h"

namespace vku {
	struct VulkanDevice;

	struct ShaderModule {
		VulkanDevice* device;
		VkShaderModule handle;
		VkShaderStageFlagBits shaderStageFlag;
		ShaderVariant info;
		std::vector<uint32_t> spirvData;

		std::vector<ShaderCacheHotReloadCallback> hotReloadCallbacks;

		ShaderModule(VulkanDevice* device, std::vector<uint32_t> data, VkShaderStageFlagBits shaderStageFlag);
		void registerHotReloadCallback(ShaderCacheHotReloadCallback callback);
		VkPipelineShaderStageCreateInfo getStageInfo();
		~ShaderModule();

		operator VkShaderModule () const { return handle; }
	};
}