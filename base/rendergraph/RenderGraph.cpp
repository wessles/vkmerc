#include "RenderGraph.h"
#include "../scene/Scene.h"

#include <stdexcept>

#include <glm/glm.hpp>

#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanDescriptorSet.h"
#include "VulkanTexture.h"

namespace vku {
	RenderGraphSchema::RenderGraphSchema() {}
	RenderGraphSchema::~RenderGraphSchema() {
		for (auto* att : edges)
			delete att;
		for (auto* pass : nodes)
			delete pass;
	}
	PassSchema* RenderGraphSchema::pass(const std::string& name, std::function<void(const uint32_t, const VkCommandBuffer&)> commands) {
		PassSchema* node = new PassSchema(name);
		node->commands = commands;
		nodes.push_back(node);
		return node;
	}
	AttachmentSchema* RenderGraphSchema::attachment(const std::string& name, PassSchema* from, std::vector<PassSchema*> to) {
		AttachmentSchema* edge = new AttachmentSchema(name);
		edge->from = from;
		edge->to = to;

		if (edge->from != nullptr) {
			edge->from->out.push_back(edge);
		}

		for (PassSchema* node : edge->to) {
			node->in.push_back(edge);
		}

		edges.push_back(edge);
		return edge;
	}

	RenderGraph::RenderGraph(RenderGraphSchema* schema, Scene* scene, uint32_t numInstances) {
		this->scene = scene;
		this->device = scene->device;
		this->numInstances = numInstances;
		this->schema = schema;

		// allocate passes and attachments
		nodes.resize(schema->nodes.size());
		for (uint32_t i = 0; i < nodes.size(); i++) {
			Pass* node = new Pass();
			node->schema = schema->nodes[i];
			nodes[i] = node;
		}
		edges.resize(schema->edges.size());
		for (uint32_t i = 0; i < edges.size(); i++) {
			Attachment* edge = new Attachment();
			edge->schema = schema->edges[i];
			edges[i] = edge;
		}

		// connect them
		for (uint32_t i = 0; i < nodes.size(); i++) {
			Pass* passNode = nodes[i];
			for (auto* edge : passNode->schema->in) {
				Attachment* att = getAttachment(edge->name);
				att->to.push_back(passNode);
				passNode->in.push_back(att);
			}
			for (auto* edge : passNode->schema->out) {
				passNode->out.push_back(getAttachment(edge->name));
			}
		}
	}
	RenderGraph::~RenderGraph() {
		for (Pass* node : nodes) {
			delete node;
		}
		for (Attachment* edge : edges) {
			delete edge;
		}
	}

	Pass* RenderGraph::getPass(const std::string& name) {
		for (auto* node : nodes) {
			if (node->schema->name == name) {
				return node;
			}
		}
		return nullptr;
	}
	Attachment* RenderGraph::getAttachment(const std::string& name) {
		for (auto* edge : edges) {
			if (edge->schema->name == name) {
				return edge;
			}
		}
		return nullptr;
	}

	void RenderGraph::createLayouts() {
		// generate singular resources for nodes (renderpass, descriptor set layout) as opposed to the duplicated resources we make later (descriptor set, framebuffer)
		for (Pass* passNode : nodes) {

			const PassSchema schema = *passNode->schema;

			// generate one render pass for each node
			{
				std::vector<VkAttachmentDescription> attachments;
				std::vector<VkAttachmentReference> inputRefs;
				std::vector<VkAttachmentReference> outputRefs;
				VkAttachmentReference depthOutputRef;
				bool depthWrite = false;

				uint32_t i = 0;

				for (auto& edge : schema.in) {
					if (!edge->isInputAttachment) continue;

					VkAttachmentDescription attachment{};
					attachment.format = edge->format;
					attachment.samples = edge->samples;
					attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					if (edge->isDepth) {
						attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}
					else {
						attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					}


					attachments.push_back(attachment);
					inputRefs.push_back({ i++, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
				}

				// -1 = not found
				int attachmentIndexToResolve = -1;
				AttachmentSchema* swapchainAttachmentSchema = nullptr;

				for (AttachmentSchema* edge : schema.out) {
					VkAttachmentDescription attachment{};
					attachment.format = edge->format;
					attachment.samples = edge->samples;
					if (edge->clearOnLoad) {
						attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
						attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
					}
					else {
						attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
						attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					}
					attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

					if (edge->isDepth) {
						attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}
					else {
						attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					}

					if (edge->isSwapchain && edge->isDepth) {
						throw std::runtime_error("Cannot present a depth layout image!");
					}

					if (edge->resolve) {
						attachmentIndexToResolve = i;
						swapchainAttachmentSchema = edge;
					}
					else if (edge->isSwapchain) {
						attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
					}

					if (edge->isSampled) {
						attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					}

					if (edge->isDepth) {
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

				VkAttachmentReference resolveRef;
				VkAttachmentReference* resolveRefPtr = nullptr;

				if (attachmentIndexToResolve != -1) {
					VkAttachmentDescription colorAttachmentResolve{};
					colorAttachmentResolve.format = swapchainAttachmentSchema->format;
					colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
					colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

					if (swapchainAttachmentSchema->isSwapchain) {
						colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
					}
					else if (swapchainAttachmentSchema->isSampled) {
						colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					}
					else if (swapchainAttachmentSchema->format != device->swapchain->depthFormat) {
						colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
					}
					else {
						colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					}

					attachments.push_back(colorAttachmentResolve);
					resolveRef = { i++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
					resolveRefPtr = &resolveRef;
				}

				VkSubpassDescription subpass{};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = static_cast<uint32_t>(outputRefs.size());
				subpass.pColorAttachments = outputRefs.data();
				subpass.inputAttachmentCount = static_cast<uint32_t>(inputRefs.size());
				subpass.pInputAttachments = inputRefs.data();
				subpass.pResolveAttachments = resolveRefPtr;
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
				passNode->pass = pass;
			}
			// generate one descriptor set layout for each node
			{
				std::vector<DescriptorLayout> layouts;
				for (auto& inputEdge : schema.in) {
					layouts.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT });
				}
				passNode->inputLayout = new VulkanDescriptorSetLayout(device, layouts);
			}
			// generate one pipeline layout for each node (must be a subset of all pipeline layouts hereafter)
			{
				std::vector<VkDescriptorSetLayout> layouts = {
					scene->globalDescriptorSetLayout->handle,
					passNode->inputLayout->handle
				};

				VkPushConstantRange maxPushConst;
				maxPushConst.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
				maxPushConst.offset = 0;
				maxPushConst.size = 128;

				VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
				pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
				pipelineLayoutInfo.pSetLayouts = layouts.data();
				pipelineLayoutInfo.pushConstantRangeCount = 1;
				pipelineLayoutInfo.pPushConstantRanges = &maxPushConst;

				VkResult result = vkCreatePipelineLayout(*device, &pipelineLayoutInfo, nullptr, &passNode->pipelineLayout);
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
			if (edges[i]->schema->resolve) {
				edges[i]->resolveInstances.resize(numInstances);
			}
		}

		// we need to process multiple instances for each element of the graph
		// so that we can avoid data hazards in the render loop
		for (uint32_t i = 0; i < numInstances; i++) {
			// part I: generate attachment images
			{
				VkExtent2D& screen = device->swapchain->swapChainExtent;
				for (Attachment* edge : this->edges) {
					const AttachmentSchema* schema = edge->schema;

					if (schema->width < 0) {
						edge->width = -static_cast<int>(screen.width) / static_cast<int>(schema->width);
					}
					else {
						edge->width = schema->width;
					}
					if (schema->height < 0) {
						edge->height = -static_cast<int>(screen.height) / static_cast<int>(schema->height);
					}
					else {
						edge->height = schema->height;
					}

					if (schema->isSwapchain && !schema->resolve) {
						continue;
					}

					VkImageUsageFlags usage;
					VkImageAspectFlags aspect;
					if (edge->schema->isDepth) {
						usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
						aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
					}
					else {
						usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
						aspect = VK_IMAGE_ASPECT_COLOR_BIT;
					}

					// transient means that the data never leaves the GPU (like a depth buffer)
					if (schema->isTransient)
						usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

					if (schema->isInputAttachment)
						usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

					if (schema->isSampled)
						usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

					VulkanImageInfo imageInfo{};
					imageInfo.width = edge->width;
					imageInfo.height = edge->height;
					imageInfo.numSamples = schema->samples;
					imageInfo.format = schema->format;
					imageInfo.usage = usage;

					VulkanImageViewInfo imageViewInfo{};
					imageViewInfo.aspectFlags = aspect;

					VulkanSamplerInfo samplerInfo = edge->schema->samplerInfo;

					VulkanTextureInfo texInfo{};
					texInfo.imageInfo = imageInfo;
					texInfo.imageViewInfo = imageViewInfo;
					texInfo.samplerInfo = samplerInfo;

					edge->instances[i].texture = new VulkanTexture(device, texInfo);

					// if we need to resolve multisampling (and we don't have a spare swapchain image lying around), we need a corresponding attachment
					if (schema->resolve && !schema->isSwapchain) {
						imageInfo.numSamples = VK_SAMPLE_COUNT_1_BIT;
						texInfo.imageInfo = imageInfo;
						edge->resolveInstances[i] = { new VulkanTexture(device, texInfo) };
					}

					if (edge->schema->isDepth) {
						edge->instances[i].texture->image->transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
					}
				}
			}
			// (Part II) allocate descriptor set and framebuffers
			{
				for (Pass* node : this->nodes) {
					Pass& current = *node;

					// (Part II.A) create descriptor set based on layout
					{
						node->instances[i].descriptorSet = new VulkanDescriptorSet(current.inputLayout);
					}
					// (Part II.B) create framebuffers
					{
						std::vector<VkImageView> attachmentImageViews;

						for (auto& edge : current.in) {
							const AttachmentSchema* schema = edge->schema;

							if (schema->isInputAttachment) {
								attachmentImageViews.push_back(*edge->instances[i].texture->view);
							}
						}

						bool resolveAttachmentAtEnd = false;
						VulkanImageView* resolveAttachmentImageView = nullptr;

						for (auto& edge : current.out) {
							const AttachmentSchema* schema = edge->schema;

							if (schema->isSwapchain) {
								if (schema->resolve) {
									attachmentImageViews.push_back(*edge->instances[i].texture->view);

									resolveAttachmentAtEnd = true;
									resolveAttachmentImageView = device->swapchain->swapChainImageViews[i];
								}
								else {
									attachmentImageViews.push_back(*device->swapchain->swapChainImageViews[i]);
								}
							}
							else {
								attachmentImageViews.push_back(*edge->instances[i].texture->view);

								if (schema->resolve) {
									resolveAttachmentAtEnd = true;
									resolveAttachmentImageView = edge->resolveInstances[i].texture->view;
								}
							}
						}

						if (resolveAttachmentAtEnd) {
							attachmentImageViews.push_back(*resolveAttachmentImageView);
						}

						// 1. all nodes have at least one outgoing attachment
						// 2. all outgoing attachments and input attachments have identical dimensions
						node->width = current.out[0]->width;
						node->height = current.out[0]->height;

						VkFramebufferCreateInfo framebufferCreate{};
						framebufferCreate.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
						framebufferCreate.renderPass = node->pass;
						framebufferCreate.attachmentCount = static_cast<uint32_t>(attachmentImageViews.size());
						framebufferCreate.pAttachments = attachmentImageViews.data();
						framebufferCreate.width = node->width;
						framebufferCreate.height = node->height;
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
				if (edge->schema->isTransient) continue;

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

					if (!edge->schema->resolve) {
						node->instances[i].descriptorSet->write(bindingIndex, edge->instances[i].texture);
					}
					else {
						node->instances[i].descriptorSet->write(bindingIndex, edge->resolveInstances[i].texture);
					}
				}
			}
		}
	}

	void RenderGraph::destroyInstances() {
		for (Pass* node : nodes) {
			for (int i = 0; i < numInstances; i++) {
				delete node->instances[i].descriptorSet;
				vkDestroyFramebuffer(*device, node->instances[i].framebuffer, nullptr);
			}
		}
		for (Attachment* edge : edges) {
			for (int i = 0; i < numInstances; i++) {
				delete edge->instances[i].texture;
				if (edge->schema->resolve)
					delete edge->resolveInstances[i].texture;
			}
		}
	}

	void RenderGraph::render(VkCommandBuffer cmdbuf, uint32_t i) {
		// we'll set width/height in the loop
		VkViewport viewport{};
		viewport.x = 0.0;
		viewport.y = 0.0;
		viewport.minDepth = 0.0;
		viewport.maxDepth = 1.0;
		VkRect2D scissor{};
		scissor.offset = { 0,0 };

		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->globalPipelineLayout, 0, 1, &scene->globalDescriptorSets[i]->handle, 0, nullptr);

		for (auto& node : nodes) {
			std::vector<VkClearValue> clearValues{};
			VkClearValue clear;
			for (Attachment* edge : node->in) {
				clear.color = edge->schema->clearColor;
				clear.depthStencil = edge->schema->clearDepthStencil;
				clearValues.push_back(clear);
			}
			for (Attachment* edge : node->out) {
				clear.color = edge->schema->clearColor;
				clear.depthStencil = edge->schema->clearDepthStencil;
				clearValues.push_back(clear);
			}

			uint32_t width = node->out[0]->width;
			uint32_t height = node->out[0]->height;
			viewport.width = width;
			viewport.height = height;
			scissor.extent = { width,height };
			vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
			vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

			VkRenderPassBeginInfo passBeginInfo{};
			passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			passBeginInfo.renderArea.offset = { 0, 0 };
			passBeginInfo.renderArea.extent = scissor.extent;
			passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			passBeginInfo.pClearValues = clearValues.data();

			passBeginInfo.framebuffer = node->instances[i].framebuffer;
			passBeginInfo.renderPass = node->pass;

			vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, node->pipelineLayout, 1, 1, &node->instances[i].descriptorSet->handle, 0, nullptr);

			vkCmdBeginRenderPass(cmdbuf, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				node->schema->commands(i, cmdbuf);
			}
			vkCmdEndRenderPass(cmdbuf);
		}
	}
}