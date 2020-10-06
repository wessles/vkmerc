#include "VulkanImage.h"

#include <stdexcept>

#include "VulkanDevice.h"

namespace vku {

	// VulkanImage

	VulkanImage::VulkanImage(VulkanDevice* device, VulkanImageInfo info) {
		// set up image create, as a dst for the staging buffer we just made
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = info.width;
		imageInfo.extent.height = info.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = info.mipLevels;
		imageInfo.arrayLayers = info.arrayLayers;
		imageInfo.format = info.format;
		imageInfo.tiling = info.tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = info.usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = info.numSamples;
		imageInfo.flags = info.imageCreateFlags;

		if (vkCreateImage(*device, &imageInfo, nullptr, &handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(*device, *this, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = device->findSupportedMemoryType(memRequirements.memoryTypeBits, info.properties);

		if (vkAllocateMemory(*device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate image memory!");
		}

		vkBindImageMemory(*device, handle, memory, 0);

		this->info = info;
		this->device = device;
	}

	VulkanImage::~VulkanImage() {
		vkDestroyImage(*device, handle, nullptr);
		vkFreeMemory(*device, memory, nullptr);
	}

	void VulkanImage::writeImageViewInfo(VulkanImageViewInfo& viewInfo) {
		viewInfo.image = *this;
		viewInfo.format = info.format;
		viewInfo.mipLevels = info.mipLevels;
		viewInfo.layerCount = info.arrayLayers;
	}

	// VulkanImageView

	VulkanImageView::VulkanImageView(VulkanDevice* device, VulkanImageViewInfo info) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = info.image;
		viewInfo.viewType = info.imageViewType;
		viewInfo.format = info.format;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = info.mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = info.layerCount;
		viewInfo.subresourceRange.aspectMask = info.aspectFlags;

		if (vkCreateImageView(*device, &viewInfo, nullptr, &handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create image view!");
		}

		this->device = device;
	}

	VulkanImageView::~VulkanImageView() {
		vkDestroyImageView(*device, handle, nullptr);
	}

	// VulkanSampler

	VulkanSampler::VulkanSampler(VulkanDevice* device, VulkanSamplerInfo info) {
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.addressModeU = info.addressModeU;
		samplerInfo.addressModeV = info.addressModeV;
		samplerInfo.addressModeW = info.addressModeW;
		samplerInfo.minFilter = info.minFilter;
		samplerInfo.magFilter = info.magFilter;
		samplerInfo.borderColor = info.borderColor;

		if (vkCreateSampler(*device, &samplerInfo, nullptr, &handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create sampler!");
		}

		this->device = device;
	}

	VulkanSampler::~VulkanSampler() {
		vkDestroySampler(*device, handle, nullptr);
	}

	// VulkanTexture

	VulkanTexture::VulkanTexture(VulkanDevice* device, VulkanTextureInfo info) {
		image = new VulkanImage(device, info.imageInfo);
		view = new VulkanImageView(device, info.imageViewInfo);
		sampler = new VulkanSampler(device, info.samplerInfo);
	}

	VkDescriptorImageInfo VulkanTexture::getImageInfo(VkImageLayout layout) {
		VkDescriptorImageInfo info{};
		info.imageLayout = layout;
		info.imageView = *view;
		info.sampler = *sampler;

		return info;
	}

	VkWriteDescriptorSet VulkanTexture::getDescriptorWrite(uint32_t binding, VkDescriptorSet descriptorSet, VkDescriptorImageInfo* imageInfo) {
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = imageInfo;

		return descriptorWrite;
	}

	void VulkanImage::transitionImageLayout(VkCommandBuffer commandBuffer, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;

		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.image = *this;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = this->info.mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = this->info.arrayLayers;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		// special stuff for depth and stencil attachment
		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}
		}

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else {
			throw std::invalid_argument("Unsupported layout transition!");
		}

		vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	void VulkanImage::transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkCommandBuffer commandBuffer = device->beginCommandBuffer(true);
		transitionImageLayout(commandBuffer, format, oldLayout, newLayout);
		device->submitCommandBuffer(commandBuffer, device->graphicsQueue);
	}
}