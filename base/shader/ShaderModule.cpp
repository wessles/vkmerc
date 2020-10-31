#include "ShaderModule.h"

#include <vector>
#include <stdexcept>

#include "../VulkanDevice.h"

namespace vku {
	ShaderModule::ShaderModule(VulkanDevice* device, std::vector<uint32_t> data, VkShaderStageFlagBits shaderStageFlag) {
		this->device = device;
		this->spirvData = data;

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = data.size() * sizeof(uint32_t);
		createInfo.pCode = data.data();

		if (vkCreateShaderModule(*device, &createInfo, nullptr, &handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create shader module!");
		}

		this->shaderStageFlag = shaderStageFlag;
	}

	void ShaderModule::registerHotReloadCallback(ShaderCacheHotReloadCallback callback) {
		hotReloadCallbacks.push_back(callback);
	}

	VkPipelineShaderStageCreateInfo ShaderModule::getStageInfo() {
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = this->shaderStageFlag;
		shaderStageInfo.module = this->handle;
		shaderStageInfo.pName = "main";

		return shaderStageInfo;
	}

	ShaderModule::~ShaderModule() {
		vkDestroyShaderModule(*device, *this, nullptr);
	}
}