#pragma once

#include <vector>
#include <functional>
#include <array>

#include <vulkan/vulkan.h>

#include "VulkanTexture.h"
#include "shader/ShaderVariant.h"
#include "VulkanMaterial.h"

namespace vku {

	// RenderGraph Schema
	///

	struct AttachmentSchema;

	struct PassReadOptions {
		bool clear = false;
		VkClearColorValue colorClearValue = { 1.0, 0.0, 1.0, 1.0 };
		VkClearDepthStencilValue depthClearValue = { 1.0, 0 };
	};

	struct PassAttachmentRead {
		AttachmentSchema* attachment;
		PassReadOptions options;

		operator AttachmentSchema* () const { return attachment; }
	};

	struct PassWriteOptions {
		bool clear = true;
		VkClearColorValue colorClearValue = { 1.0, 0.0, 1.0, 1.0 };
		VkClearDepthStencilValue depthClearValue = { 1.0, 0 };

		// transition attachment to sampled after this render pass
		bool sampled = true;
	};

	struct PassAttachmentWrite {
		AttachmentSchema* attachment;
		PassWriteOptions options;

		operator AttachmentSchema* () const { return attachment; }
	};

	struct PassSchema {
		std::string name;

		std::vector<PassAttachmentRead> in;
		std::vector<PassAttachmentWrite> out;

		uint32_t layerMask = 1;

		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

		bool materialOverride = false;
		VulkanMaterialInfo overrideMaterialInfo;

		bool isBlitPass = false;
		VulkanMaterialInfo blitPassMaterialInfo;

		PassSchema(const std::string name) {
			this->name = name;
		}

		PassAttachmentRead* read(size_t slot, AttachmentSchema* in, PassReadOptions options = {});
		PassAttachmentWrite* write(size_t slot, AttachmentSchema* out, PassWriteOptions  options = {});
	};

	struct AttachmentSchema {
		std::string name;

		VkFormat format;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

		// if so, no image will be created for this attachment, and the corresponding swapchain will be assumed as its image
		bool isSwapchain = false;

		// attachment does not get read later, it's only read by the pipeline internally
		bool isTransient = false;

		// this is a very specific term referring to the vulkan spec. An "incoming attachment" is usually *not* an input attachment.
		bool isInputAttachment = false;

		bool isSampled = false;

		bool isDepth = false;

		// resolve MSAA to single sample IF this is the swapchain
		bool resolve = false;

		// by setting to a negative value, you are setting the dimension to equal floor(-swapchain.width / -1). You can do half resolution by setting width = -2
		int width = -1, height = -1;

		VulkanSamplerInfo samplerInfo{};

		AttachmentSchema(const std::string name) {
			this->name = name;
		}
	};

	struct RenderGraphSchema {
		std::vector<PassSchema*> nodes;
		std::vector<AttachmentSchema*> edges;

		RenderGraphSchema();
		~RenderGraphSchema();

		PassSchema* pass(const std::string& name);
		PassSchema* blitPass(const std::string& name, const ShaderVariant& shaderVariant);
		PassSchema* blitPass(const std::string& name, const VulkanMaterialInfo& blitShaderInfo);
		AttachmentSchema* attachment(const std::string& name);
	};

	// RenderGraph
	///

	struct Scene;
	struct VulkanDevice;
	struct VulkanTexture;
	struct VulkanDescriptorSet;
	struct VulkanDescriptorSetLayout;
	struct VulkanMeshBuffer;
	struct VulkanMaterial;
	struct VulkanMaterialInstance;

	struct PassInstance {
		VulkanDescriptorSet* descriptorSet;
		VkFramebuffer framebuffer;
	};

	struct AttachmentInstance {
		VulkanTexture* texture;
		VkImageLayout currentLayout;
	};

	struct RenderGraph;

	struct Attachment;
	struct Pass {
		std::vector<Attachment*> in;
		std::vector<Attachment*> out;

		// this will be set during the createInstances function, based on outgoing edge dimensions (which must be equal)
		uint32_t width, height;

		// this only exists if schema.materialOverride or schema.isBlitPass
		VulkanMaterial* material = nullptr;
		VulkanMaterialInstance* materialInstance = nullptr;

		const PassSchema* schema;

		VkRenderPass pass;
		VulkanDescriptorSetLayout* inputLayout;
		VkPipelineLayout pipelineLayout;

		std::vector<PassInstance> instances;
	};

	struct Attachment {
		// this will be set during the createInstances function, based on schema and (optionally) swapchain dimensions
		uint32_t width, height;

		const AttachmentSchema* schema;

		std::vector<AttachmentInstance> instances;

		// we need a seperate attachment for each MSAA resolve step if schema.resolve=true
		std::vector<AttachmentInstance> resolveInstances;
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
		VulkanMeshBuffer* blitMesh = nullptr;

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