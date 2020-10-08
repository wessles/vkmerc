#include "RenderGraph.h"
#include "../scene/Scene.h"

#include <stdexcept>

#include <glm/glm.hpp>

#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanDescriptorSetLayout.h"
#include "VulkanImage.h"

namespace vku {
	RenderGraph::RenderGraph(Scene* scene, uint32_t numInstances) {
		this->scene = scene;
		this->device = scene->device;
		this->numInstances = numInstances;
	}
	RenderGraph::~RenderGraph() {
		for (Pass* node : nodes) {
			delete node;
		}
		for (Attachment* edge : edges) {
			delete edge;
		}
	}

	Pass* RenderGraph::pass(std::function<void(const uint32_t, const VkCommandBuffer&)> commands) {
		Pass* node = new Pass;
		node->commands = commands;
		nodes.push_back(node);
		return node;
	}
	Attachment* RenderGraph::attachment(Pass* from, std::vector<Pass*> to) {
		Attachment* edge = new Attachment();
		edge->from = from;
		edge->to = to;
		if (edge->from != nullptr)
			edge->from->out.push_back(edge);

		bool isTransient = edge->to.size() == 1 && edge->to[0] == nullptr;
		if (!isTransient) {
			for (Pass* node : edge->to) {
				node->in.push_back(edge);
			}
		}
		edges.push_back(edge);
		return edge;
	}

	void RenderGraph::begin(Pass* node) {
		this->start = node;
	}
	void RenderGraph::terminate(Pass* node) {
		this->end = node;
	}

	void RenderGraph::createLayouts() {
		// generate singular resources for nodes (renderpass, descriptor set layout) as opposed to the duplicated resources we make later (descriptor set, framebuffer)
		for (Pass* node : nodes) {
			// generate one render pass for each node
			{
				Pass& current = *node;

				bool isStart = this->start == &current;
				bool isEnd = this->end == &current;

				std::vector<VkAttachmentDescription> attachments;
				std::vector<VkAttachmentReference> inputRefs;
				std::vector<VkAttachmentReference> outputRefs;
				VkAttachmentReference depthOutputRef;
				bool depthWrite = false;

				uint32_t i = 0;

				for (auto& edge : current.in) {
					bool isDepth = edge->format == this->device->swapchain->depthFormat;

					VkAttachmentDescription attachment{};
					attachment.format = edge->format;
					attachment.samples = edge->samples;
					attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					if (isDepth) {
						attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
						attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}
					else {
						attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					}

					attachments.push_back(attachment);
					inputRefs.push_back({ i++, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
				}

				if (isEnd && current.out.size() != 1) {
					throw std::runtime_error("Out node must have *exactly* one outgoing attachment.");
				}

				for (auto& edge : current.out) {
					bool isDepth = edge->format == this->device->swapchain->depthFormat;

					VkAttachmentDescription attachment{};
					attachment.format = edge->format;
					attachment.samples = edge->samples;
					attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
					attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

					if (isDepth) {
						attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}
					else {
						attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					}

					if (isEnd && isDepth) {
						throw std::runtime_error("Cannot present a depth layout image!");
					}

					if (isEnd)
						attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

					if (isDepth) {
						if (depthWrite) {
							throw std::runtime_error("Cannot write to multiple depth attachments!");
						}
						depthWrite = true;
						attachments.push_back(attachment);
						depthOutputRef = { i++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
					}
					else {
						attachments.push_back(attachment);
						outputRefs.push_back({ i++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
					}
				}

				VkSubpassDescription subpass{};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = static_cast<uint32_t>(outputRefs.size());

				subpass.pColorAttachments = outputRefs.data();
				subpass.inputAttachmentCount = static_cast<uint32_t>(inputRefs.size());
				subpass.pInputAttachments = inputRefs.data();
				if (depthWrite)
					subpass.pDepthStencilAttachment = &depthOutputRef;

				std::vector<VkSubpassDependency> dependencies;
				dependencies.resize(2);

				dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
				dependencies[0].dstSubpass = 0;
				dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
				dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
				dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

				if (depthWrite) {
					dependencies.resize(3);

					dependencies[1].srcSubpass = 0;
					dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
					dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					dependencies[1].dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
					dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

					dependencies[2].srcSubpass = 0;
					dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
					dependencies[2].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
					dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
					dependencies[2].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
					dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
				}
				else {
					dependencies[1].srcSubpass = 0;
					dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
					dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
					dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
					dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
				}

				VkRenderPassCreateInfo renderPassCreateInfo{};
				renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
				renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
				renderPassCreateInfo.pAttachments = &attachments[0];
				renderPassCreateInfo.subpassCount = 1;
				renderPassCreateInfo.pSubpasses = &subpass;
				renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
				renderPassCreateInfo.pDependencies = dependencies.data();

				VkRenderPass pass;
				if (vkCreateRenderPass(*device, &renderPassCreateInfo, nullptr, &pass) != VK_SUCCESS) {
					throw std::runtime_error("Failed to create render pass.");
				}
				node->pass = pass;
			}
			// generate one descriptor set layout for each node
			{
				std::vector<DescriptorLayout> layouts;
				for (auto& inputEdge : node->in) {
					layouts.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT });
				}
				node->inputLayout = new VulkanDescriptorSetLayout(device, layouts);
			}
			// generate one pipeline layout for each node (must be a subset of all pipeline layouts hereafter)
			{
				std::vector<VkDescriptorSetLayout> layouts = {
					*scene->globalDescriptorSetLayout,
					*node->inputLayout
				};

				// transformation matrix push constant
				VkPushConstantRange transformPushConst;
				transformPushConst.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
				transformPushConst.offset = 0;
				transformPushConst.size = sizeof(glm::mat4);

				VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
				pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
				pipelineLayoutInfo.pSetLayouts = layouts.data();
				pipelineLayoutInfo.pushConstantRangeCount = 1;
				pipelineLayoutInfo.pPushConstantRanges = &transformPushConst;

				VkResult result = vkCreatePipelineLayout(*device, &pipelineLayoutInfo, nullptr, &node->pipelineLayout);
				if (result != VK_SUCCESS) {
					throw std::runtime_error("Failed to create pipeline layout for material!");
				}
			}
		}
	}
	void RenderGraph::destroyLayouts() {
		for (Pass* node : nodes) {
			vkDestroyPipelineLayout(*device, node->pipelineLayout, nullptr);
			delete node->inputLayout;
			vkDestroyRenderPass(*device, node->pass, nullptr);
		}
	}

	void RenderGraph::createInstances() {
		for (uint32_t i = 0; i < nodes.size(); i++) {
			nodes[i]->instances.resize(numInstances);
		}
		for (uint32_t i = 0; i < edges.size(); i++) {
			edges[i]->instances.resize(numInstances);
		}

		// we need to process multiple instances for each element of the graph
		// so that we can avoid data hazards in the render loop
		for (uint32_t i = 0; i < numInstances; i++) {
			// part I: generate attachment images
			{
				VkExtent2D& screen = device->swapchain->swapChainExtent;
				for (Attachment* edge : this->edges) {
					if (edge->to.size() == 0) {
						continue;
					}

					bool isDepth = edge->format == device->swapchain->depthFormat;

					VkImageUsageFlags usage;
					VkImageAspectFlags aspect;
					if (isDepth) {
						usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
						aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
					}
					else {
						usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
						aspect = VK_IMAGE_ASPECT_COLOR_BIT;
					}

					// transient means that the data never leaves the GPU (like a depth buffer)
					// indicated in graph by edge going to "nullptr"
					bool isTransient = edge->to.size() == 1 && edge->to[0] == nullptr;
					if (isTransient)
						usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

					bool isInput = edge->to.size() > 0 && !isTransient;
					if (isInput)
						usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

					VulkanImageInfo imageInfo{};
					imageInfo.width = screen.width;
					imageInfo.height = screen.height;
					imageInfo.numSamples = edge->samples;
					imageInfo.format = edge->format;
					imageInfo.usage = usage;

					VulkanImageViewInfo imageViewInfo{};
					imageViewInfo.aspectFlags = aspect;
					imageViewInfo.format = edge->format;

					VulkanSamplerInfo samplerInfo{};
					samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
					samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

					VulkanTextureInfo texInfo{};
					texInfo.imageInfo = imageInfo;
					texInfo.imageViewInfo = imageViewInfo;
					texInfo.samplerInfo = samplerInfo;

					edge->instances[i].texture = new VulkanTexture(device, texInfo);

					if (isDepth) {
						edge->instances[i].texture->image->transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
					}
				}
			}
			// (Part II) allocate descriptor set and framebuffers
			{
				for (Pass* node : this->nodes) {
					Pass& current = *node;

					bool isStart = this->start == &current;
					bool isEnd = this->end == &current;

					// (Part II.A) create descriptor set based on layout
					{
						VkDescriptorSetAllocateInfo allocInfo{};
						allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
						allocInfo.descriptorPool = device->descriptorPool;
						allocInfo.descriptorSetCount = 1;
						allocInfo.pSetLayouts = &current.inputLayout->handle;

						if (vkAllocateDescriptorSets(*device, &allocInfo, &node->instances[i].descriptorSet) != VK_SUCCESS) {
							throw std::runtime_error("Failed to allocate descriptor sets!");
						}
					}
					// (Part II.B) create framebuffers
					{
						std::vector<VkImageView> attachmentImageViews;

						for (auto& edge : current.in) {
							attachmentImageViews.push_back(*edge->instances[i].texture->view);
						}
						if (isEnd) {
							attachmentImageViews.push_back(*device->swapchain->swapChainImageViews[i]);
						}
						else {
							for (auto& edge : current.out) {
								attachmentImageViews.push_back(*edge->instances[i].texture->view);
							}
						}


						VkFramebufferCreateInfo framebufferCreate{};
						framebufferCreate.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
						framebufferCreate.renderPass = node->pass;
						framebufferCreate.attachmentCount = static_cast<uint32_t>(attachmentImageViews.size());
						framebufferCreate.pAttachments = attachmentImageViews.data();
						framebufferCreate.width = device->swapchain->swapChainExtent.width;
						framebufferCreate.height = device->swapchain->swapChainExtent.height;
						framebufferCreate.layers = 1;

						if (vkCreateFramebuffer(*device, &framebufferCreate, nullptr, &(node->instances[i].framebuffer)) != VK_SUCCESS) {
							throw std::runtime_error("Failed to create framebuffer!");
						}
					}
				}
			}
		}
		// Now that all nodes have allocated their descriptor sets, let's write input image references to them
		for (uint32_t i = 0; i < numInstances; i++) {
			for (Attachment* edge : this->edges) {
				if (edge->to.size() == 0) continue;

				bool isTransient = edge->to.size() == 1 && edge->to[0] == nullptr;
				if (isTransient) continue;

				for (Pass* node : edge->to) {
					int bindingIndex = -1;
					for (int k = 0; k < node->in.size(); k++) {
						if (node->in[k] == edge) {
							bindingIndex = k;
							break;
						}
					}
					if (bindingIndex == -1) {
						throw std::runtime_error("Binding index for an edge could not be found in target node.");
					}

					VkDescriptorImageInfo imageInfo = edge->instances[i].texture->getImageInfo();
					VkWriteDescriptorSet setWrite = edge->instances[i].texture->getDescriptorWrite(bindingIndex, node->instances[i].descriptorSet, &imageInfo);
					vkUpdateDescriptorSets(*device, 1, &setWrite, 0, nullptr);
				}
			}
		}
	}

	void RenderGraph::destroyInstances() {
		for (Pass* node : nodes) {
			for (int i = 0; i < numInstances; i++) {
				vkFreeDescriptorSets(*device, device->descriptorPool, 1, &node->instances[i].descriptorSet);
				vkDestroyFramebuffer(*device, node->instances[i].framebuffer, nullptr);
			}
		}
		for (Attachment* edge : edges) {
			for (int i = 0; i < numInstances; i++) {
				delete edge->instances[i].texture;
			}
		}
	}

	void RenderGraph::render(VkCommandBuffer cmdbuf, uint32_t i) {
		uint32_t width = device->swapchain->swapChainExtent.width;
		uint32_t height = device->swapchain->swapChainExtent.height;

		// we'll set width/height in the loop
		VkViewport viewport{};
		viewport.x = 0.0;
		viewport.y = 0.0;
		viewport.width = width;
		viewport.height = height;
		viewport.minDepth = 0.0;
		viewport.maxDepth = 1.0;
		VkRect2D scissor{};
		scissor.offset = { 0,0 };
		scissor.extent = { width,height };

		// set viewport / scissor before any drawing
		// this is because materials have dynamic viewport & scissor state by default
		vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->globalPipelineLayout, 0, 1, &scene->globalDescriptorSets[i], 0, nullptr);

		for (auto& node : nodes) {
			std::vector<VkClearValue> clearValues{};
			for (Attachment* edge : node->in) {
				VkClearValue clear{};
				clear.color = { 0.0f, 0.0f, 0.0f, 1.0f };
				clear.depthStencil = { 1, 0 };
				clearValues.push_back(clear);
			}
			for (Attachment* edge : node->out) {
				VkClearValue clear{};
				clear.color = { 1.0f, 1.0f, 1.0f, 1.0f };
				clear.depthStencil = { 1, 0 };
				clearValues.push_back(clear);
			}

			VkRenderPassBeginInfo passBeginInfo{};
			passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			passBeginInfo.renderArea.offset = { 0, 0 };
			passBeginInfo.renderArea.extent = scissor.extent;
			passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			passBeginInfo.pClearValues = clearValues.data();

			passBeginInfo.framebuffer = node->instances[i].framebuffer;
			passBeginInfo.renderPass = node->pass;

			vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, node->pipelineLayout, 1, 1, &node->instances[i].descriptorSet, 0, nullptr);

			vkCmdBeginRenderPass(cmdbuf, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				node->commands(i, cmdbuf);
			}
			vkCmdEndRenderPass(cmdbuf);
		}
	}
}