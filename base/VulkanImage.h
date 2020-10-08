#pragma once

#include <string>

#include <vulkan/vulkan.h>

namespace vku {
	struct VulkanDevice;

	struct VulkanImageViewInfo;

	struct VulkanImageInfo {
		uint32_t width = 0, height = 0;
		uint32_t mipLevels = 1;
		VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
		VkImageUsageFlags usage = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
		VkMemoryPropertyFlags properties;
		VkImageCreateFlags imageCreateFlags = 0;
		uint32_t arrayLayers = 1;
	};

	struct VulkanImage {
		VkImage handle;
		VkDeviceMemory memory;

	private:
		VulkanDevice* device;
		VulkanImageInfo info;

	public:
		VulkanImage() {}
		VulkanImage(VulkanDevice* device, VulkanImageInfo info);
		VulkanImage(VulkanDevice* device, const std::string& path);
		VulkanImage(VulkanDevice* device, const std::array<std::string, 6>& paths);
		~VulkanImage();

		void writeImageViewInfo(VulkanImageViewInfo* viewInfo);

		void transitionImageLayout(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout);
		void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);

		void copyBufferToImage(VkBuffer buffer);
		void loadFromBuffer(VkBuffer buffer);

		// allow convenient casting
		operator VkImage() const { return handle; }

	private:
		void init(VulkanImageInfo info);
		void generateMipmaps();
	};



	struct VulkanImageViewInfo {
		VkImage image;

		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		uint32_t mipLevels = 1;
		VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D;
		uint32_t layerCount = 1;
	};

	struct VulkanImageView {
		VkImageView handle;

		VulkanDevice* device;

		VulkanImageView() {}
		VulkanImageView(VulkanDevice* device, VulkanImageViewInfo info);
		~VulkanImageView();

		// allow convenient casting
		operator VkImageView() const { return handle; }
	};



	struct VulkanSamplerInfo {
		VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		VkFilter minFilter = VK_FILTER_LINEAR;
		VkFilter magFilter = VK_FILTER_LINEAR;
	};

	struct VulkanSampler {
		VkSampler handle;

		VulkanDevice* device;

		VulkanSampler() {}
		VulkanSampler(VulkanDevice* device, VulkanSamplerInfo info);
		~VulkanSampler();

		// allow convenient casting
		operator VkSampler() const { return handle; }
	};



	struct VulkanTextureInfo {
		VulkanImageInfo imageInfo;
		VulkanImageViewInfo imageViewInfo;
		VulkanSamplerInfo samplerInfo;
	};

	struct VulkanTexture {
		VulkanImage* image;
		VulkanImageView* view;
		VulkanSampler* sampler;

		VulkanTexture() {}
		VulkanTexture(VulkanDevice* device, VulkanTextureInfo info);
		~VulkanTexture();

		VkDescriptorImageInfo getImageInfo(VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		VkWriteDescriptorSet getDescriptorWrite(uint32_t binding, VkDescriptorSet descriptorSet, VkDescriptorImageInfo* imageInfo);

		// allow convenient casting
		operator VkImage() const { return *image; }
	};
}