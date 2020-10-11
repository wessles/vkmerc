#include "VulkanDescriptorSet.h"

#include <vector>
#include <stdexcept>

#include <vulkan/vulkan.h>

#include "VulkanDevice.h"
#include "VulkanUniform.h"
#include "VulkanTexture.h"

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

	VulkanDescriptorSet::VulkanDescriptorSet(VulkanDescriptorSetLayout* layout) {
		this->device = layout->device;

		VkDescriptorSetAllocateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorPool = device->descriptorPool;
		info.descriptorSetCount = 1;
		info.pSetLayouts = &layout->handle;

		vkAllocateDescriptorSets(*device, &info, &handle);
	}
	VulkanDescriptorSet::~VulkanDescriptorSet() {
		vkFreeDescriptorSets(*device, device->descriptorPool, 1, &handle);
	}

	void VulkanDescriptorSet::write(uint32_t binding, VulkanUniform* uniform) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniform->buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = uniform->dataSize;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = handle;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(*device, 1, &descriptorWrite, 0, nullptr);
	}

	void VulkanDescriptorSet::write(uint32_t binding, VulkanTexture* texture) {
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = *texture->view;
		imageInfo.sampler = *texture->sampler;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = handle;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(*device, 1, &descriptorWrite, 0, nullptr);
	}
}