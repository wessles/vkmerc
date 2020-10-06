#include "VulkanDescriptorSetLayout.h"

#include <vector>
#include <stdexcept>

#include <vulkan/vulkan.h>

#include "VulkanDevice.h"

namespace vku {
	VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice *device, std::vector<DescriptorLayout> descriptorLayouts) {
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		int bindingIndex = 0;
		for (auto& descriptorLayout : descriptorLayouts) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = bindingIndex++;
			binding.descriptorCount = 1;
			binding.pImmutableSamplers = nullptr;
			binding.descriptorType = descriptorLayout.type;
			binding.stageFlags = descriptorLayout.stageFlags;
			bindings.push_back(binding);
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		if (bindings.size() == 0) {
			layoutInfo.pBindings = nullptr;
		}
		else {
			layoutInfo.pBindings = &bindings[0];
		}
		if (vkCreateDescriptorSetLayout(*device, &layoutInfo, nullptr, &this->handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor set layout.");
		}

		this->device = device;
	}

	VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
		vkDestroyDescriptorSetLayout(*device, handle, nullptr);
	}

	VkDescriptorSet VulkanDescriptorSetLayout::createDescriptorSet() {
		VkDescriptorSet descriptorSet;

		VkDescriptorSetAllocateInfo info{};
		info.descriptorPool = device->descriptorPool;
		info.descriptorSetCount = 1;
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.pSetLayouts = &handle;

		vkAllocateDescriptorSets(*device, &info, &descriptorSet);

		return descriptorSet;
	}
}