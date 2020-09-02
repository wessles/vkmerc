#pragma once

#include "Bvk.hpp"
#include <vulkan/vulkan.h>

using namespace vku::image;

namespace vku::rgraph {


	struct REdgeInstance {
		Image image;
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
		RNode* to;
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

		REdge* addAttachment(RNode* from, RNode* to) {
			REdge *edge = new REdge();
			edge->from = from;
			edge->to = to;
			if (edge->from != nullptr)
				edge->from->out.push_back(edge);
			if (edge->to != nullptr)
				edge->to->in.push_back(edge);
			edges.push_back(edge);
			return edge;
		}

	private:
		bool processed = false;
	public:
		void process(VkDescriptorPool &pool) {
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
					uint32_t i = 0;

					for (auto &edge : current.in) {
						VkAttachmentDescription attachment{};
						attachment.format = edge->format;
						attachment.samples = edge->samples;
						attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
						attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
						attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
						attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

						attachments.push_back(attachment);
						inputRefs.push_back({ i++, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
					}

					if (isEnd && current.out.size() != 1) {
						throw std::runtime_error("Out node must have *exactly* one outgoing attachment.");
					}

					for (auto &edge : current.out) {
						VkAttachmentDescription attachment{};
						attachment.format = edge->format;
						attachment.samples = edge->samples;
						attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
						attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
						attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
						attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
						attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
						if (isEnd)
							attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

						attachments.push_back(attachment);
						outputRefs.push_back({ i++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
					}

					VkSubpassDescription subpass{};
					subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
					subpass.colorAttachmentCount = outputRefs.size();
					subpass.pColorAttachments = outputRefs.data();
					subpass.inputAttachmentCount = inputRefs.size();
					subpass.pInputAttachments = inputRefs.data();
					subpass.pDepthStencilAttachment = 0;

					std::array<VkSubpassDependency, 2> dependencies;

					dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
					dependencies[0].dstSubpass = 0;
					dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
					dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
					dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

					dependencies[1].srcSubpass = 0;
					dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
					dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
					dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
					dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

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
			}

			// we need to process multiple instances for each element of the graph
			// so that we can avoid data hazards in the render loop
			for(uint32_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
				// part I: generate attachment images
				{
					VkExtent2D &screen = vku::state::swapChainExtent;
					for (REdge *edge : this->edges) {
						if (edge->to == nullptr) {
							continue;
						}

						// alias for brevity
						Image &image = edge->instances[i].image;

						createImage(
							screen.width, screen.height, 1,
							edge->samples, edge->format,
							VK_IMAGE_TILING_OPTIMAL,
							VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image.handle, image.memory);
						image.view = createImageView(image.handle, edge->format, VK_IMAGE_ASPECT_COLOR_BIT, 1);

						VkSamplerCreateInfo samplerCI = vku::create::createSamplerCI();
						samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
						samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
						
						if (vkCreateSampler(vku::state::device, &samplerCI, nullptr, &image.sampler) != VK_SUCCESS) {
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
								attachmentImageViews.push_back(edge->instances[i].image.view);
							}
							if (isEnd) {
								attachmentImageViews.push_back(vku::state::swapChainImageViews[i]);
							}
							else {
								for (auto &edge : current.out) {
									attachmentImageViews.push_back(edge->instances[i].image.view);
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
					if (edge->to == nullptr) continue;

					int bindingIndex = -1;
					for (int k = 0; k < edge->to->in.size(); k++) {
						if (edge->to->in[k] == edge) {
							bindingIndex = k;
							break;
						}
					}
					if (bindingIndex == -1) {
						throw std::runtime_error("Binding index for an edge could not be found in target node.");
					}

					VkWriteDescriptorSet setWrite = edge->instances[i].image.getDescriptorWrite(bindingIndex, edge->to->instances[i].descriptorSet);
					vkUpdateDescriptorSets(vku::state::device, 1, &setWrite, 0, nullptr);
				}
			}
		}
		void destroy(VkDescriptorPool &pool) {
			for (RNode* node : nodes) {
				for (int i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
					vkDestroyFramebuffer(vku::state::device, node->instances[i].framebuffer, nullptr);
				}
				vkDestroyDescriptorSetLayout(vku::state::device, node->inputLayout, nullptr);
				vkDestroyRenderPass(vku::state::device, node->pass, nullptr);
			}
			for (REdge* edge : edges) {
				for (int i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
					vku::image::destroyImage(vku::state::device, edge->instances[i].image);
				}
			}

			processed = false;
		}
	};
}
