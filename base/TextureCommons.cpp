#include "TextureCommons.h"

#include "VulkanTexture.h"
#include "VulkanDevice.h"

namespace vku {

	void createSinglePixelTexture(VulkanDevice* device, unsigned char *pixels, VulkanTexture **texture) {
		*texture = new VulkanTexture();
		VulkanTexture* pixTex = *texture;
		pixTex->image = new VulkanImage(device, VulkanImageInfo{
			.width = 1,
			.height = 1,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
			});

		VkDeviceSize imageSize = 1 * 1 * 4;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		device->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(*device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(*device, stagingBufferMemory);

		// create image and load from buffer
		pixTex->image->loadFromBuffer(stagingBuffer);

		vkDestroyBuffer(*device, stagingBuffer, nullptr);
		vkFreeMemory(*device, stagingBufferMemory, nullptr);

		VulkanImageViewInfo viewInfo{};
		pixTex->image->writeImageViewInfo(&viewInfo);
		pixTex->view = new VulkanImageView(device, viewInfo);

		pixTex->sampler = new VulkanSampler(device, VulkanSamplerInfo{});

	}

	TextureCommons::TextureCommons(VulkanDevice* device) {
		// make 1x1 white texture
		unsigned char white[] = { 0xff,0xff,0xff,0xff };
		unsigned char black[] = { 0x00,0x00,0x00,0x00 };
		unsigned char norm[] = { 0x00,0x00,0xff,0x00 };
		createSinglePixelTexture(device, white, &white1x1);
		createSinglePixelTexture(device, black, &black1x1);
		createSinglePixelTexture(device, norm, &zNorm);
		
	}

	TextureCommons::~TextureCommons() {
		delete white1x1;
		delete black1x1;
		delete zNorm;
	}
}