#include "VulkanTexture.h"

#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <array>

#include "VulkanDevice.h"

namespace vku {

	// VulkanImage

	VulkanImage::VulkanImage(VulkanDevice* device, VulkanImageInfo info) {
		this->device = device;
		init(info);
	}

	VulkanImage::~VulkanImage() {
		vkDestroyImage(*device, handle, nullptr);
		vkFreeMemory(*device, memory, nullptr);
	}

	void VulkanImage::writeImageViewInfo(VulkanImageViewInfo* viewInfo) {
		viewInfo->image = *this;
		viewInfo->format = info.format;
		viewInfo->mipLevels = info.mipLevels;
		viewInfo->layerCount = info.arrayLayers;
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
		samplerInfo.compareEnable = info.compareEnable;
		samplerInfo.compareOp = info.compareOp;
		samplerInfo.minLod = info.minLod;
		samplerInfo.maxLod = info.maxLod;

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
		image->writeImageViewInfo(&info.imageViewInfo);
		view = new VulkanImageView(device, info.imageViewInfo);
		sampler = new VulkanSampler(device, info.samplerInfo);
	}

	VulkanTexture::~VulkanTexture() {
		delete image;
		delete view;
		delete sampler;
	}

	void VulkanImage::transitionImageLayout(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout) {
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

			if (info.format == VK_FORMAT_D32_SFLOAT_S8_UINT || info.format == VK_FORMAT_D24_UNORM_S8_UINT) {
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

	void VulkanImage::transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkCommandBuffer commandBuffer = device->beginCommandBuffer(true);
		transitionImageLayout(commandBuffer, oldLayout, newLayout);
		device->submitCommandBuffer(commandBuffer, device->graphicsQueue);
	}

	void VulkanImage::generateMipmaps() {
		// check for linear blitting support
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(device->physicalDevice, info.format, &formatProperties);
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("Texture image format does not support linear blitting.");
		}

		VkCommandBuffer commandBuffer = device->beginCommandBuffer(true);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = *this;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = info.width;
		int32_t mipHeight = info.height;

		for (uint32_t i = 1; i < info.mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			// blit from mip level i-1 to i, at half the resolution
			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				*this, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				*this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit, VK_FILTER_LINEAR);

			// move mip level i-1 from SRC to SHADER_READ_ONLY layout, since we're done blitting
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			// reduce resolution for next mip blit (fun word)
			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		// before we're done, just transition that last mip level from DST to READ_ONLY
		barrier.subresourceRange.baseMipLevel = info.mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		device->submitCommandBuffer(commandBuffer, device->graphicsQueue);
	}

	void VulkanImage::copyBufferToImage(VkBuffer buffer) {
		VkCommandBuffer commandBuffer = device->beginCommandBuffer(true);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = info.arrayLayers;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { info.width, info.height, 1 };

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			handle,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		device->submitCommandBuffer(commandBuffer, device->graphicsQueue);
	}

	void VulkanImage::loadFromBuffer(VkBuffer buffer) {
		// transition layout to transfer destination optimized type
		transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		// copy data to image
		copyBufferToImage(buffer);
		// transition layout to readonly shader data, and generate mip maps (even if it's just one)
		generateMipmaps();
	}

	VulkanImage::VulkanImage(VulkanDevice* device, const std::string& path) {
		this->device = device;

		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

		if (!pixels) {
			throw std::runtime_error("Failed to load texture image!");
		}

		VulkanImageInfo imageInfo{};
		imageInfo.width = texWidth;
		imageInfo.height = texHeight;
		imageInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;

		VkDeviceSize imageSize = texWidth * texHeight * 4;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		device->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(*device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(*device, stagingBufferMemory);
		stbi_image_free(pixels);

		// create image and load from buffer
		init(imageInfo);
		loadFromBuffer(stagingBuffer);

		vkDestroyBuffer(*device, stagingBuffer, nullptr);
		vkFreeMemory(*device, stagingBufferMemory, nullptr);
	}

	// generates a cubemap
	VulkanImage::VulkanImage(VulkanDevice* device, const std::array<std::string, 6>& paths) {
		this->device = device;

		std::array<stbi_uc*, 6> faceData;
		int texWidth, texHeight, texChannels;

		for (int i = 0; i < paths.size(); i++) {
			faceData[i] = stbi_load(paths[i].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
			if (!faceData[i]) {
				throw std::runtime_error("Failed to load cubemap image '" + paths[i] + "'!");
			}
		}

		VkDeviceSize layerSize = texWidth * texHeight * 4;
		VkDeviceSize imageSize = layerSize * paths.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		device->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(*device, stagingBufferMemory, 0, imageSize, 0, &data);
		for (int i = 0; i < paths.size(); i++) {
			memcpy(static_cast<char*>(data) + (layerSize * i), faceData[i], static_cast<size_t>(layerSize));
		}
		vkUnmapMemory(*device, stagingBufferMemory);

		for (int i = 0; i < paths.size(); i++) {
			stbi_image_free(faceData[i]);
		}

		VulkanImageInfo imageInfo{};
		imageInfo.width = texWidth;
		imageInfo.height = texHeight;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 6;
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageInfo.imageCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		init(imageInfo);

		// transition layout to transfer destination optimized type
		transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		// copy data to image
		copyBufferToImage(stagingBuffer);
		// transition layout to readonly shader data, and generate mip maps (even if it's just one)
		transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(*device, stagingBuffer, nullptr);
		vkFreeMemory(*device, stagingBufferMemory, nullptr);
	}

	void VulkanImage::init(VulkanImageInfo info) {
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
		imageInfo.usage = info.usage;

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
	}
}