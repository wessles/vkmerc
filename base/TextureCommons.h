#pragma once

namespace vku {
	struct VulkanDevice;
	struct VulkanTexture;

	struct TextureCommons {
		VulkanTexture* white1x1, *black1x1, *zNorm;
		TextureCommons(VulkanDevice* device);
		~TextureCommons();
	};
}