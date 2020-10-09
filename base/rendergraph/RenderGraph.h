#pragma once

#include <vector>
#include <functional>
#include <array>

#include <vulkan/vulkan.h>


namespace vku {
	struct Scene;
	struct VulkanDevice;
	struct VulkanTexture;
	struct VulkanDescriptorSetLayout;

	struct PassInstance {
		VkDescriptorSet descriptorSet;
		VkFramebuffer framebuffer;
	};

	struct AttachmentInstance {
		VulkanTexture* texture;
	};

	struct Attachment;
	struct Pass {
		std::vector<Attachment*> in;
		std::vector<Attachment*> out;

		std::function<void(const uint32_t, const VkCommandBuffer&)> commands;

		VkRenderPass pass;
		VulkanDescriptorSetLayout *inputLayout;
		VkPipelineLayout pipelineLayout;

		std::vector<PassInstance> instances;
	};

	struct Attachment {
		Pass* from;
		std::vector<Pass*> to;
		VkFormat format;
		VkSampleCountFlagBits samples;
		bool transient;
		bool isSwapchain;

		std::vector<AttachmentInstance> instances;
	};

	class RenderGraph {
		std::vector<Pass*> nodes;
		std::vector<Attachment*> edges;
		Pass* start;
		Pass* end;

		Scene* scene;
		VulkanDevice* device;

		// multiple instances for swap synchronization purposes
		uint32_t numInstances;

	public:
		RenderGraph(Scene* scene, uint32_t numInstances);
		~RenderGraph();

		Pass* pass(std::function<void(const uint32_t, const VkCommandBuffer&)> commands);
		Attachment* attachment(Pass* from, std::vector<Pass*> to);

		void begin(Pass* node);
		void terminate(Pass* node);

		void render(VkCommandBuffer cmdbuf, uint32_t i);

		void createLayouts();
		void destroyLayouts();

		void createInstances();
		void destroyInstances();
	};
}