#pragma once

#include <vector>
#include <functional>
#include <array>

#include <vulkan/vulkan.h>


namespace vku {

	// RenderGraph Schema
	///

	struct AttachmentSchema;

	struct PassSchema {
		std::string name;

		std::vector<AttachmentSchema*> in;
		std::vector<AttachmentSchema*> out;

		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

		std::function<void(const uint32_t, const VkCommandBuffer&)> commands;
		
		PassSchema(const std::string name) {
			this->name = name;
		}
	};

	struct AttachmentSchema {
		std::string name;

		PassSchema* from;
		std::vector<PassSchema*> to;

		VkFormat format;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

		// if so, no image will be created for this attachment, and the corresponding swapchain will be assumed as its image
		bool isSwapchain = false;

		// attachment does not get read later, it's only read by the pipeline internally
		bool isTransient = false;

		// this is a very specific term referring to the vulkan spec. An "incoming attachment" is usually *not* an input attachment.
		bool isInputAttachment = false;

		bool isSampled = false;

		// resolve MSAA to single sample IF this is the swapchain
		bool resolve = false;

		// by setting to a negative value, you are setting the dimension to equal floor(-swapchain.width / -1). You can do half resolution by setting width = -2
		int width = -1, height = -1;

		AttachmentSchema(const std::string name) {
			this->name = name;
		}
	};

	struct RenderGraphSchema {
		std::vector<PassSchema*> nodes;
		std::vector<AttachmentSchema*> edges;

		RenderGraphSchema();
		~RenderGraphSchema();

		PassSchema* pass(const std::string& name, std::function<void(const uint32_t, const VkCommandBuffer&)> commands);
		AttachmentSchema* attachment(const std::string& name, PassSchema* from, std::vector<PassSchema*> to);
	};

	// RenderGraph
	///

	struct Scene;
	struct VulkanDevice;
	struct VulkanTexture;
	struct VulkanDescriptorSet;
	struct VulkanDescriptorSetLayout;

	struct PassInstance {
		VulkanDescriptorSet* descriptorSet;
		VkFramebuffer framebuffer;
	};

	struct AttachmentInstance {
		VulkanTexture* texture;
	};

	struct RenderGraph;

	struct Attachment;
	struct Pass {
		std::vector<Attachment*> in;
		std::vector<Attachment*> out;

		// this will be set during the createInstances function, based on outgoing edge dimensions (which must be equal)
		uint32_t width, height;

		const PassSchema* schema;

		VkRenderPass pass;
		VulkanDescriptorSetLayout* inputLayout;
		VkPipelineLayout pipelineLayout;

		std::vector<PassInstance> instances;
	};

	struct Attachment {
		std::vector<Pass*> to;

		// this will be set during the createInstances function, based on schema and (optionally) swapchain dimensions
		uint32_t width, height;

		const AttachmentSchema* schema;

		std::vector<AttachmentInstance> instances;
	};

	class RenderGraph {
		std::vector<Pass*> nodes;
		std::vector<Attachment*> edges;

		RenderGraphSchema* schema;
		Scene* scene;
		VulkanDevice* device;

		// multiple instances for swap synchronization purposes
		uint32_t numInstances;

	public:
		RenderGraph(RenderGraphSchema* schema, Scene* scene, uint32_t numInstances);
		~RenderGraph();

		Pass* getPass(const std::string& name);
		Attachment* getAttachment(const std::string& name);

		void render(VkCommandBuffer cmdbuf, uint32_t i);

		void createLayouts();
		void destroyLayouts();

		void createInstances();
		void destroyInstances();
	};
}