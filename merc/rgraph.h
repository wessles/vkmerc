#pragma once

/*
This is a basic rendergraph implementation.
First, build the render graph through functions in RGraph.
Next, call graph.process() to create / allocate runtime resources
Every time the swapchain changes this should be updated.
*/

#include "Vku.hpp"
#include <vulkan/vulkan.h>

using namespace vku::image;

namespace vku::rgraph {
	struct REdgeInstance {
		Texture texture;
	};

	struct RNodeInstance {
		VkDescriptorSet descriptorSet;
		VkFramebuffer framebuffer;
	};

	struct RNode;
	struct REdge;
	struct RGraph;

	struct REdge {
		RNode* from;
		std::vector<RNode*> to;
		VkFormat format;
		VkSampleCountFlagBits samples;

		std::array<REdgeInstance, 3> instances;
	};

	struct RNode {
		std::vector<REdge*> in;
		std::vector<REdge*> out;

		std::function<void(const uint32_t, const VkCommandBuffer&)> commands;

		VkRenderPass pass;
		VkDescriptorSetLayout inputLayout;
		VkPipelineLayout pipelineLayout;

		std::array<RNodeInstance, 3> instances;
	};

	struct RGraph {
		std::vector<RNode*> nodes;
		std::vector<REdge*> edges;
		RNode* start;
		RNode* end;

		RGraph() {}
		~RGraph() {
			for (RNode* node : nodes) {
				delete node;
			}
			for (REdge* edge : edges) {
				delete edge;
			}
		}

		RNode* addPass(std::function<void(const uint32_t, const VkCommandBuffer&)> commands) {
			RNode *node = new RNode();
			node->commands = commands;
			nodes.push_back(node);
			return node;
		}

		void begin(RNode* node) {
			this->start = node;
		}

		void terminate(RNode* node) {
			this->end = node;
		}

		REdge* addAttachment(RNode* from, std::vector<RNode*> to) {
			REdge *edge = new REdge();
			edge->from = from;
			edge->to = to;
			if (edge->from != nullptr)
				edge->from->out.push_back(edge);

			bool isTransient = edge->to.size() == 1 && edge->to[0] == nullptr;
			if (!isTransient) {
				for (RNode *node : edge->to) {
					node->in.push_back(edge);
				}
			}
			edges.push_back(edge);
			return edge;
		}

	private:
		bool processed = false;
	public:
		void process(VkDescriptorSetLayout globalDSet, VkDescriptorPool &pool) {
			if (processed) {
				throw std::runtime_error("This render Graph has already been processed!");
			}
			processed = true;

			// generate singular resources for nodes (renderpass, descriptor set layout)
			// as opposed to the duplicated resources we make later (descriptor set, framebuffer)
			for(RNode *node : nodes){
				// generate one render pass for each node
				{
					RNode &current = *node;

					bool isStart = this->start == &current;
					bool isEnd = this->end == &current;

					std::vector<VkAttachmentDescription> attachments;
					std::vector<VkAttachmentReference> inputRefs;
					std::vector<VkAttachmentReference> outputRefs;
					VkAttachmentReference depthOutputRef;
					bool depthWrite = false;

					uint32_t i = 0;

					for (auto &edge : current.in) {
						bool isDepth = edge->format == vku::state::depthFormat;

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
						} else {
							attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
							attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						}

						attachments.push_back(attachment);
						inputRefs.push_back({ i++, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
					}

					if (isEnd && current.out.size() != 1) {
						throw std::runtime_error("Out node must have *exactly* one outgoing attachment.");
					}

					for (auto &edge : current.out) {
						bool isDepth = edge->format == vku::state::depthFormat;

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
						} else {
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
						} else {
							attachments.push_back(attachment);
							outputRefs.push_back({ i++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
						}
					}

					VkSubpassDescription subpass{};
					subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
					subpass.colorAttachmentCount = outputRefs.size();
					
					subpass.pColorAttachments = outputRefs.data();
					subpass.inputAttachmentCount = inputRefs.size();
					subpass.pInputAttachments = inputRefs.data();
					if(depthWrite)
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
					} else {
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
					if (vkCreateRenderPass(vku::state::device, &renderPassCreateInfo, nullptr, &pass) != VK_SUCCESS) {
						throw std::runtime_error("Failed to create render pass.");
					}
					node->pass = pass;
				}
				// generate one descriptor set layout for each node
				{
					std::vector<vku::descriptor::DescriptorLayout> layouts;
					for (auto &inputEdge : node->in) {
						layouts.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT });
					}
					vku::descriptor::createDescriptorSetLayout(&node->inputLayout, layouts);
				}
				// generate one pipeline layout for each node (must be a subset of all pipeline layouts hereafter)
				{
					std::vector<VkDescriptorSetLayout> layouts = {
						globalDSet,
						node->inputLayout
					};

					// transformation matrix push constant
					VkPushConstantRange transformPushConst;
					transformPushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
					transformPushConst.offset = 0;
					transformPushConst.size = sizeof(glm::mat4);

					VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
					pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
					pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
					pipelineLayoutInfo.pSetLayouts = layouts.data();
					pipelineLayoutInfo.pushConstantRangeCount = 1;
					pipelineLayoutInfo.pPushConstantRanges = &transformPushConst;

					VkResult result = vkCreatePipelineLayout(vku::state::device, &pipelineLayoutInfo, nullptr, &node->pipelineLayout);
					if (result != VK_SUCCESS) {
						throw std::runtime_error("Failed to create pipeline layout for material!");
					}
				}
			}

			// we need to process multiple instances for each element of the graph
			// so that we can avoid data hazards in the render loop
			for(uint32_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
				// part I: generate attachment images
				{
					VkExtent2D &screen = vku::state::swapChainExtent;
					for (REdge *edge : this->edges) {
						if (edge->to.size() == 0) {
							continue;
						}

						bool isDepth = edge->format == vku::state::depthFormat;

						// alias for brevity
						Texture &texture = edge->instances[i].texture;
						Image &image = texture.image;

						VkImageUsageFlags usage;
						VkImageAspectFlags aspect;
						if (isDepth) {
							usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
							aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
						} else {
							usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
							aspect = VK_IMAGE_ASPECT_COLOR_BIT;
						}

						// transient means that the data never leaves the GPU (like a depth buffer)
						// indicated in graph by edge going to "nullptr"
						bool isTransient = edge->to.size() == 1 && edge->to[0] == nullptr;
						if(isTransient)
							usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

						bool isInput = edge->to.size() > 0 && !isTransient;
						if (isInput)
							usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

						createImage(
							screen.width, screen.height, 1,
							edge->samples, edge->format,
							VK_IMAGE_TILING_OPTIMAL,
							usage,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image.handle, image.memory);
						image.view = createImageView(image.handle, edge->format, aspect, 1);
						if (isDepth) {
							vku::image::transitionImageLayout(image.handle, edge->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
						}

						VkSamplerCreateInfo samplerCI = vku::create::createSamplerCI();
						samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
						samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
						
						if (vkCreateSampler(vku::state::device, &samplerCI, nullptr, &texture.sampler) != VK_SUCCESS) {
							throw std::runtime_error("Failed to create attachment sampler!");
						}
					}
				}
				// (Part II) generate descriptor set/layout and framebuffers
				{
					for (RNode *node : this->nodes) {
						RNode &current = *node;

						bool isStart = this->start == &current;
						bool isEnd = this->end == &current;

						// (Part II.A) create descriptor set based on layout
						{
							VkDescriptorSetAllocateInfo allocInfo{};
							allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
							allocInfo.descriptorPool = pool;
							allocInfo.descriptorSetCount = 1;
							allocInfo.pSetLayouts = &node->inputLayout;

							for (int i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
								if (vkAllocateDescriptorSets(vku::state::device, &allocInfo, &node->instances[i].descriptorSet) != VK_SUCCESS) {
									throw std::runtime_error("Failed to allocate descriptor sets!");
								}
							}
						}
						// (Part II.B) create framebuffers
						{
							std::vector<VkImageView> attachmentImageViews;

							for (auto &edge : current.in) {
								attachmentImageViews.push_back(edge->instances[i].texture.image.view);
							}
							if (isEnd) {
								attachmentImageViews.push_back(vku::state::swapChainImageViews[i]);
							}
							else {
								for (auto &edge : current.out) {
									attachmentImageViews.push_back(edge->instances[i].texture.image.view);
								}
							}

							VkFramebufferCreateInfo framebufferCreate{};
							framebufferCreate.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
							framebufferCreate.renderPass = node->pass;
							framebufferCreate.attachmentCount = static_cast<uint32_t>(attachmentImageViews.size());
							framebufferCreate.pAttachments = attachmentImageViews.data();
							framebufferCreate.width = vku::state::swapChainExtent.width;
							framebufferCreate.height = vku::state::swapChainExtent.height;
							framebufferCreate.layers = 1;

							if (vkCreateFramebuffer(vku::state::device, &framebufferCreate, nullptr, &(node->instances[i].framebuffer)) != VK_SUCCESS) {
								throw std::runtime_error("Failed to create framebuffer!");
							}
						}
					}
				}
			}
			// Now that all nodes have allocated their descriptor sets, let's write input image references to them
			for (uint32_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
				for (REdge *edge : this->edges) {
					if (edge->to.size() == 0) continue;

					bool isTransient = edge->to.size() == 1 && edge->to[0] == nullptr;
					if (isTransient) continue;

					for (RNode *node : edge->to) {
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

						VkWriteDescriptorSet setWrite = edge->instances[i].texture.getDescriptorWrite(bindingIndex, node->instances[i].descriptorSet);
						vkUpdateDescriptorSets(vku::state::device, 1, &setWrite, 0, nullptr);
					}
				}
			}
		}
		void destroy(VkDescriptorPool &pool) {
			for (RNode* node : nodes) {
				for (int i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
					vkDestroyFramebuffer(vku::state::device, node->instances[i].framebuffer, nullptr);
				}
				vkDestroyPipelineLayout(vku::state::device, node->pipelineLayout, nullptr);
				vkDestroyDescriptorSetLayout(vku::state::device, node->inputLayout, nullptr);
				vkDestroyRenderPass(vku::state::device, node->pass, nullptr);
			}
			for (REdge* edge : edges) {
				for (int i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
					vku::image::destroyTexture(vku::state::device, edge->instances[i].texture);
				}
			}

			processed = false;
		}
	};
}
