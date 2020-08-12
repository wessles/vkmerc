#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // for overriding opengl defaults
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <thread>

#include "FlyCam.h"

#include <chrono>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <array>
#include <unordered_map>

#include "Bvk.hpp"

////////////////////////////

using bvk::device;
using bvk::physicalDevice;
using bvk::surface;
using bvk::QueueFamilyIndices;
using bvk::SwapChainSupportDetails;
using bvk::msaaSamples;
using bvk::support::findQueueFamiliesSupported;
using bvk::graphicsQueue;
using bvk::presentQueue;

using bvk::swapChain;
using bvk::swapChainImages;
using bvk::swapChainImageFormat;
using bvk::swapChainExtent;
using bvk::swapChainImageViews;

using bvk::createImageView;

using bvk::commandPool;
using bvk::descriptorPool;

using bvk::MAX_FRAMES_IN_FLIGHT;
using bvk::imageAvailableSemaphores;
using bvk::renderFinishedSemaphores;
using bvk::inFlightFences;
using bvk::imagesInFlight;

using bvk::buffer::copyBuffer;
using bvk::buffer::createBuffer;

//////////////////////////////

static std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;
	glm::vec3 normal;
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};

		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}
	static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

		// vertex position attribute (vec3)
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		// vertex color attribute (vec3)
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex, normal);

		return attributeDescriptions;
	}
	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
	}
};
struct MinimalVertex {
	glm::vec3 pos;
	glm::vec2 texCoord;
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};

		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(MinimalVertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}
	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

		// vertex position attribute (vec3)
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(MinimalVertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 2;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(MinimalVertex, texCoord);

		return attributeDescriptions;
	}
	bool operator==(const MinimalVertex& other) const {
		return pos == other.pos && texCoord == other.texCoord;
	}
};
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

struct UniformBufferObject {
	glm::vec3 light;
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

struct MinimalMeshData {
	std::vector<MinimalVertex> vertices;
	std::vector<uint32_t> indices;
};


struct MeshData {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

struct MeshBuffer {
	VkBuffer vBuffer;
	VkDeviceMemory vMemory;
	VkBuffer iBuffer;
	VkDeviceMemory iMemory;
};

void destroyMeshBuffer(const VkDevice& device, MeshBuffer& meshBuffer) {
	vkDestroyBuffer(device, meshBuffer.vBuffer, nullptr);
	vkDestroyBuffer(device, meshBuffer.iBuffer, nullptr);
	vkFreeMemory(device, meshBuffer.vMemory, nullptr);
	vkFreeMemory(device, meshBuffer.iMemory, nullptr);
}

struct Image {
	VkImage image;
	VkDeviceMemory imageMemory;
	VkImageView imageView;
	uint32_t mipLevels = 1;
};

void destroyImage(const VkDevice& device, Image& image) {
	vkDestroyImage(device, image.image, nullptr);
	vkDestroyImageView(device, image.imageView, nullptr);
	vkFreeMemory(device, image.imageMemory, nullptr);
}



MeshData box = {
	{
		{{-1, -1, -1}, {}, {}, {1, 1, 1}},
		{{1, -1, -1}, {}, {}, {-1, 1, 1}},
		{{-1, 1, -1}, {}, {}, {1, -1, 1}},
		{{1, 1, -1}, {}, {}, {-1, -1, 1}},
		{{-1, -1, 1}, {}, {}, {1, 1, -1}},
		{{1, -1, 1}, {}, {}, {-1, 1, -1}},
		{{-1, 1, 1}, {}, {}, {1, -1, -1}},
		{{1, 1, 1}, {}, {}, {-1, -1, -1}},
	},
	{
		0, 3, 2,
		0, 1, 3,
		1, 5, 7,
		1, 7, 3,
		4, 7, 5,
		4, 6, 7,
		4, 2, 6,
		4, 0, 2,
		6, 2, 7,
		2, 3, 7,
		1, 4, 5,
		0, 4, 1
	}
};
MinimalMeshData blit = {
	{
		{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
		{{1.0f,	-1.0f, 0.0f}, {1.0f, 0.0f}},
		{{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
		{{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}
	},
	{0, 2, 1, 1, 2, 3}
};


///////////

class VulkanDemo {
public:
	void run() {
		const auto& resizeCallback = [&](GLFWwindow* window, int width, int height) { framebufferResized = true; };
		const auto& resizeCallbackFunc = std::function<void(GLFWwindow*, int, int)>(resizeCallback);
		bvk::window::initWindow(WIDTH, HEIGHT, resizeCallbackFunc);

		initVulkan();
		camera = FlyCam(bvk::window::window);
		mainLoop();
		cleanup();
	}

private:
	const uint32_t WIDTH = 500;
	const uint32_t HEIGHT = 500;

	const std::string MODEL_PATH = "scene.obj";
	const std::string TEXTURE_PATH = "viking_room.png";


	Image depthImage;
	Image colorImage;
	Image postImage;
	VkSampler postImageSampler;

	Image cubeMap;
	VkSampler cubeMapSampler;

	VkRenderPass renderPass;

	struct {
		VkPipeline attachmentWrite;
		VkPipeline attachmentRead;
		VkPipeline skybox;
	} pipelines;

	struct {
		std::vector<VkDescriptorSet> attachmentWrite;
		std::vector<VkDescriptorSet> attachmentRead;
	} descriptorSets;

	struct {
		VkDescriptorSetLayout attachmentWrite;
		VkDescriptorSetLayout attachmentRead;
	} descriptorSetLayouts;

	struct {
		VkPipelineLayout attachmentWrite;
		VkPipelineLayout attachmentRead;
	} pipelineLayouts;

	std::vector<VkFramebuffer> swapChainFramebuffers;

	std::vector<VkCommandBuffer> commandBuffers;

	size_t currentFrame = 0;

	Image textureImage;
	VkSampler textureSampler;

	MeshData mesh;
	MeshBuffer meshBuffer;

	MeshBuffer blitMeshBuffer;

	MeshBuffer skyboxBuffer;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;

	FlyCam camera;

	bool framebufferResized = false;

	const bool enableValidationLayers = true;

	void initVulkan() {
		bvk::createInstance(enableValidationLayers);
		bvk::createSurface();
		bvk::pickPhysicalDevice();
		bvk::createLogicalDevice();
		bvk::createSwapChain();
		bvk::createSwapchainImageViews();
		bvk::createCommandPool();
		bvk::createSyncObjects();

		// application specific initializations

		createDescriptorSetLayout();
		createRenderPass();
		createGraphicsPipelines();

		createImages();
		createFramebuffers();

		createTextureImage();
		loadModel();
		createVertexBuffer();
		createIndexBuffer();

		createUniformBuffers();
		createDescriptorSets();

		createCommandBuffers();
	}

	void createRenderPass() {

		std::array<VkAttachmentDescription, 4> attachments{};

		// color attachment
		attachments[0].format = swapChainImageFormat;
		attachments[0].samples = msaaSamples;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorAttachmentRef { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		// depth attachment
		attachments[1].format = bvk::support::findDepthFormat();
		attachments[1].samples = msaaSamples;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkAttachmentReference depthAttachmentRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		// resolver for high res MSAA colorattachment above
		attachments[2].format = swapChainImageFormat;
		attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorAttachmentResolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		// post fx attachment
		attachments[3].format = swapChainImageFormat;
		attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[3].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		VkAttachmentReference postAttachmentRef {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

		std::array<VkSubpassDescription, 2> subpasses{};

		// first subpass - fill color and depth attachments / resolve MSAA
		///
		
		subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[0].colorAttachmentCount = 1;
		subpasses[0].pColorAttachments = &colorAttachmentRef;
		subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;
		subpasses[0].pResolveAttachments = &colorAttachmentResolveRef;

		// second subpass - read input attachment and write to swapchain color attachment
		///

		subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[1].colorAttachmentCount = 1;
		subpasses[1].pColorAttachments = &postAttachmentRef;
		// resolved color buffer used as input
		VkAttachmentReference inputReferences[] {
			{2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL} // assume the resolved buffer is transitioned to shader_read_only layout
		};
		subpasses[1].inputAttachmentCount = 1;
		subpasses[1].pInputAttachments = inputReferences;

		/*
			Subpass dependencies for layout transitions
		*/
		std::array<VkSubpassDependency, 3> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// input attachment from color attachment -> shader read
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = 1;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[2].srcSubpass = 0;
		dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
		renderPassInfo.pSubpasses = subpasses.data();
		renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();

		VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	void createDescriptorSetLayout() {
		// write to attachment
		{
			VkDescriptorSetLayoutBinding uboLayoutBinding{};
			uboLayoutBinding.binding = 0;
			uboLayoutBinding.descriptorCount = 1;
			uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uboLayoutBinding.pImmutableSamplers = nullptr;
			uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

			VkDescriptorSetLayoutBinding samplerLayoutBinding{};
			samplerLayoutBinding.binding = 1;
			samplerLayoutBinding.descriptorCount = 1;
			samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerLayoutBinding.pImmutableSamplers = nullptr;
			samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutBinding cubemapSamplerLayoutBinding{};
			cubemapSamplerLayoutBinding.binding = 2;
			cubemapSamplerLayoutBinding.descriptorCount = 1;
			cubemapSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			cubemapSamplerLayoutBinding.pImmutableSamplers = nullptr;
			cubemapSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
				uboLayoutBinding,
				samplerLayoutBinding,
				cubemapSamplerLayoutBinding
			};

			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
			layoutInfo.pBindings = bindings.data();

			if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayouts.attachmentWrite) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create descriptor set layout!");
			}

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 1; // 1 descriptor layout
			pipelineLayoutInfo.pSetLayouts = &descriptorSetLayouts.attachmentWrite;
			pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
			pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

			VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayouts.attachmentWrite);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to create pipeline layout!");
			}
		}


		{
			VkDescriptorSetLayoutBinding uboLayoutBinding{};
			uboLayoutBinding.binding = 0;
			uboLayoutBinding.descriptorCount = 1;
			uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uboLayoutBinding.pImmutableSamplers = nullptr;
			uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

			VkDescriptorSetLayoutBinding inputAttachmentLayoutBinding{};
			inputAttachmentLayoutBinding.binding = 1;
			inputAttachmentLayoutBinding.descriptorCount = 1;
			inputAttachmentLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			inputAttachmentLayoutBinding.pImmutableSamplers = nullptr;
			inputAttachmentLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
				uboLayoutBinding,
				inputAttachmentLayoutBinding,
			};

			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
			layoutInfo.pBindings = bindings.data();

			if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayouts.attachmentRead) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create descriptor set layout!");
			}

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 1; // 1 descriptor layout
			pipelineLayoutInfo.pSetLayouts = &descriptorSetLayouts.attachmentRead;
			pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
			pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

			VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayouts.attachmentRead);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to create pipeline layout!");
			}
		}
	}

	void createGraphicsPipelines() {
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// vertex pipeline stage input (incomplete, will add actual descriptions later, per pipeline)
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;

		// regular viewport, size to swapchain, NOT window
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		// the scissor mask will cover the entire screen, doing nothing
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;
		// create viewport state with viewport / scissor
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		// a bunch more boilerplate...
		VkPipelineColorBlendAttachmentState colorBlendAttachment;
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		VkPipelineRasterizationStateCreateInfo rasterizer = bvk::create::createRasterizationStateCI();
		VkPipelineDepthStencilStateCreateInfo depthStencil = bvk::create::createStencilStateCI();

		// MSAA with special sample shading options
		VkPipelineMultisampleStateCreateInfo multisampling = bvk::create::createMSStateCI();
		multisampling.sampleShadingEnable = VK_TRUE; // enable sample shading in the pipeline
		multisampling.minSampleShading = .2f; // min fraction for sample shading; closer to one is smoother

		// tesselations
		VkPipelineTessellationStateCreateInfo tesselation{};
		tesselation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
		tesselation.pNext = nullptr;
		tesselation.flags = 0;
		tesselation.patchControlPoints = 3;

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDepthStencilState = &depthStencil;

		pipelineInfo.pDynamicState = nullptr; // Optional
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		// Regular pipeline begins
		{
			VkGraphicsPipelineCreateInfo regularPipelineInfo = pipelineInfo;

			regularPipelineInfo.renderPass = renderPass;
			regularPipelineInfo.subpass = 0;
			regularPipelineInfo.layout = pipelineLayouts.attachmentWrite;

			// specify vertex attribute structure
			auto bindingDescription = Vertex::getBindingDescription();
			auto attributeDescriptions = Vertex::getAttributeDescriptions();
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; // Optional
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional


			std::array<VkPipelineShaderStageCreateInfo, 4> shaderStages{};

			// Set up tesselation shader
			// required topology for tess
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
			regularPipelineInfo.pTessellationState = &tesselation;
			VkShaderModule tessControlModule = bvk::create::createShaderModule(readFile("control.spv"));
			VkShaderModule tessEvalModule = bvk::create::createShaderModule(readFile("eval.spv"));
			shaderStages[0] = bvk::create::createShaderStage(tessControlModule, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
			shaderStages[1] = bvk::create::createShaderStage(tessEvalModule, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

			// specify shaders
			VkShaderModule vertModule = bvk::create::createShaderModule(readFile("vert.spv"));
			VkShaderModule fragModule = bvk::create::createShaderModule(readFile("frag.spv"));
			shaderStages[2] = bvk::create::createShaderStage(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[3] = bvk::create::createShaderStage(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

			regularPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			regularPipelineInfo.pStages = shaderStages.data();

			VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &regularPipelineInfo, nullptr, &pipelines.attachmentWrite);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to create graphics pipeline!");
			}


			// set back to correct topology
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			vkDestroyShaderModule(device, tessControlModule, nullptr);
			vkDestroyShaderModule(device, tessEvalModule, nullptr);
			vkDestroyShaderModule(device, vertModule, nullptr);
			vkDestroyShaderModule(device, fragModule, nullptr);
		}

		// Skybox Pipeline
		{
			VkGraphicsPipelineCreateInfo skyboxPipelineInfo = pipelineInfo;

			skyboxPipelineInfo.renderPass = renderPass;
			skyboxPipelineInfo.subpass = 0;
			skyboxPipelineInfo.layout = pipelineLayouts.attachmentWrite;
			// don't write to depth buffer
			depthStencil.depthWriteEnable = false;

			// specify vertex attribute structure
			auto bindingDescription = Vertex::getBindingDescription();
			auto attributeDescriptions = Vertex::getAttributeDescriptions();
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; // Optional
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional

			std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

			// specify shaders
			VkShaderModule vertModule = bvk::create::createShaderModule(readFile("vert_skybox.spv"));
			VkShaderModule fragModule = bvk::create::createShaderModule(readFile("frag_skybox.spv"));
			shaderStages[0] = bvk::create::createShaderStage(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = bvk::create::createShaderStage(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

			skyboxPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			skyboxPipelineInfo.pStages = shaderStages.data();

			VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &skyboxPipelineInfo, nullptr, &pipelines.skybox);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to create graphics pipeline!");
			}

			vkDestroyShaderModule(device, vertModule, nullptr);
			vkDestroyShaderModule(device, fragModule, nullptr);
		}

		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

		// Blit pipeline (for PostFX) begins
		{
			VkGraphicsPipelineCreateInfo blitPipelineInfo = pipelineInfo;

			blitPipelineInfo.renderPass = renderPass;
			blitPipelineInfo.subpass = 1;
			blitPipelineInfo.layout = pipelineLayouts.attachmentRead;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			// specify vertex attribute structure
			auto bindingDescription2 = MinimalVertex::getBindingDescription();
			auto attributeDescriptions2 = MinimalVertex::getAttributeDescriptions();
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions2.size());
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription2; // Optional
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions2.data(); // Optional

			std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

			// specify shaders
			VkShaderModule vertModule = bvk::create::createShaderModule(readFile("vert_noproj.spv"));
			VkShaderModule fragModule = bvk::create::createShaderModule(readFile("frag_blit.spv"));
			shaderStages[0] = bvk::create::createShaderStage(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = bvk::create::createShaderStage(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

			blitPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			blitPipelineInfo.pStages = shaderStages.data();

			VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &blitPipelineInfo, nullptr, &pipelines.attachmentRead);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to create graphics pipeline!");
			}

			vkDestroyShaderModule(device, vertModule, nullptr);
			vkDestroyShaderModule(device, fragModule, nullptr);
		}

	}

	void createFramebuffers() {
		swapChainFramebuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			std::array<VkImageView, 4> attachments = {
				colorImage.imageView,
				depthImage.imageView,
				postImage.imageView,
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}
	}

	void createImages() {
		// color image
		VkFormat colorFormat = swapChainImageFormat;
		bvk::image::createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage.image, colorImage.imageMemory);
		colorImage.imageView = createImageView(colorImage.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

		// depth image
		VkFormat depthFormat = bvk::support::findDepthFormat();
		bvk::image::createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage.image, depthImage.imageMemory);
		depthImage.imageView = createImageView(depthImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
		bvk::image::transitionImageLayout(depthImage.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

		// post processing image
		VkFormat postFormat = swapChainImageFormat;
		bvk::image::createImage(swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, postFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, postImage.image, postImage.imageMemory);
		postImage.imageView = createImageView(postImage.image, postFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

		VkSamplerCreateInfo samplerCI = bvk::create::createSamplerCI();
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		if (vkCreateSampler(device, &samplerCI, nullptr, &postImageSampler) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture sampler!");
		}
	}

	void loadTexture(const std::string& path, Image& image) {
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

		if (!pixels) {
			throw std::runtime_error("Failed to load texture image!");
		}

		image.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

		VkDeviceSize imageSize = texWidth * texHeight * 4;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(device, stagingBufferMemory);

		stbi_image_free(pixels);

		bvk::image::createImage(texWidth, texHeight, image.mipLevels, VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			image.image, image.imageMemory);
		// transition layout to transfer destination optimized type
		bvk::image::transitionImageLayout(image.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.mipLevels);
		// copy data to image
		bvk::image::copyBufferToImage(stagingBuffer, image.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		// transition layout to readonly shader data, and generate mip maps (even if it's just one)
		bvk::image::generateMipmaps(image.image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, image.mipLevels);


		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	/*
	negx, y, z
	posx, y, z
	*/
	void loadCubemap(const std::array<std::string, 6> &paths, Image& image) {
		std::array<stbi_uc*, 6> faceData;
		int texWidth, texHeight, texChannels;

		for (int i = 0; i < paths.size(); i++) {
			faceData[i] = stbi_load(paths[i].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
			if (!faceData[i]) {
				throw std::runtime_error("Failed to load cubemap image '" + paths[i] + "'!");
			}
		}

		VkDeviceSize layerSize = texWidth * texHeight * 4;
		VkDeviceSize imageSize = layerSize * paths.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		for (int i = 0; i < paths.size(); i++) {
			memcpy(static_cast<char*>(data) + (layerSize * i), faceData[i], static_cast<size_t>(layerSize));
		}
		vkUnmapMemory(device, stagingBufferMemory);

		for (int i = 0; i < paths.size(); i++) {
			stbi_image_free(faceData[i]);
		}

		bvk::image::createImage(texWidth, texHeight, image.mipLevels, VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			image.image, image.imageMemory,
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, paths.size());
		// transition layout to transfer destination optimized type
		bvk::image::transitionImageLayout(image.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.mipLevels, paths.size());
		// copy data to image
		bvk::image::copyBufferToImage(stagingBuffer, image.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), paths.size());
		// transition layout to readonly shader data, and generate mip maps (even if it's just one)
		//bvk::image::generateMipmaps(image.image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, image.mipLevels);
		bvk::image::transitionImageLayout(image.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, image.mipLevels, paths.size());

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);

	}

	void createTextureImage() {
		// regular texture
		loadTexture(TEXTURE_PATH, textureImage);
		textureImage.imageView = createImageView(textureImage.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, textureImage.mipLevels);

		VkSamplerCreateInfo samplerInfo = bvk::create::createSamplerCI();
		samplerInfo.maxLod = static_cast<float>(textureImage.mipLevels);
		if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture sampler!");
		}

		//cubemap texture
		loadCubemap({
			"res/cubemap/posx.jpg",
			"res/cubemap/negx.jpg",
			"res/cubemap/posy.jpg",
			"res/cubemap/negy.jpg",
			"res/cubemap/posz.jpg",
			"res/cubemap/negz.jpg",
		}, cubeMap);
		cubeMap.imageView = createImageView(cubeMap.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, cubeMap.mipLevels, VK_IMAGE_VIEW_TYPE_CUBE, 6);

		VkSamplerCreateInfo samplerCI = bvk::create::createSamplerCI();
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		if (vkCreateSampler(device, &samplerCI, nullptr, &cubeMapSampler) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture sampler!");
		}
	}

	void loadModel() {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
			throw std::runtime_error(warn + err);
		}

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.normal = {
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2],
				};

				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.color = { 1.0f, 1.0f, 1.0f };

				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(mesh.vertices.size());
					mesh.vertices.push_back(vertex);
				}

				mesh.indices.push_back(uniqueVertices[vertex]);
			}
		}
	}

	// automatically write vertex or index buffer to device local memory using staging buffer
	template <class Type>
	void initDeviceLocalBuffer(std::vector<Type> &vertices, VkBuffer &vBuffer, VkDeviceMemory &vbMemory, VkBufferUsageFlagBits bufferUsageBit) {
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsageBit,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vBuffer,
			vbMemory);

		// copy staging buffer to vertex buffer
		copyBuffer(stagingBuffer, vBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void createVertexBuffer() {
		initDeviceLocalBuffer<Vertex>(mesh.vertices, meshBuffer.vBuffer, meshBuffer.vMemory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		initDeviceLocalBuffer<MinimalVertex>(blit.vertices, blitMeshBuffer.vBuffer, blitMeshBuffer.vMemory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		initDeviceLocalBuffer<Vertex>(box.vertices, skyboxBuffer.vBuffer, skyboxBuffer.vMemory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	}

	void createIndexBuffer() {
		// model
		initDeviceLocalBuffer<uint32_t>(mesh.indices, meshBuffer.iBuffer, meshBuffer.iMemory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		// fullscreen blit
		initDeviceLocalBuffer<uint32_t>(blit.indices, blitMeshBuffer.iBuffer, blitMeshBuffer.iMemory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		// skybox box
		initDeviceLocalBuffer<uint32_t>(box.indices, skyboxBuffer.iBuffer, skyboxBuffer.iMemory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	}

	void updateUniformBuffer(uint32_t currentImage) {
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		camera.update();

		UniformBufferObject ubo{};
		ubo.light = glm::rotate(glm::vec3(0., 1., .25), glm::radians(time*100.0f), glm::vec3(0, 1, 0));
		ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
		ubo.view = glm::inverse(camera.getTransform());
		ubo.proj = camera.getProjMatrix((float)swapChainExtent.width, (float)swapChainExtent.height);

		void* data;
		vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
	}
	void createUniformBuffers() {
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		uniformBuffers.resize(swapChainImages.size());
		uniformBuffersMemory.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); i++) {
			createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		}
	}

	void createCommandBuffers() {
		// allocate
		commandBuffers.resize(swapChainFramebuffers.size());

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate command buffers!");
		}

		// prepare recording
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.flags = 0; // Optional
		cmdBufInfo.pInheritanceInfo = nullptr; // Optional

		std::array<VkClearValue, 4> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1, 0 };
		clearValues[2].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
		clearValues[3].color = { {1.0f, 0.0f, 1.0f, 0.0f} };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = swapChainExtent;
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassBeginInfo.pClearValues = clearValues.data();

		// record identical command buffers for each swap buffer
		for (int32_t i = 0; i < commandBuffers.size(); i++) {
			renderPassBeginInfo.framebuffer = swapChainFramebuffers[i];

			if (vkBeginCommandBuffer(commandBuffers[i], &cmdBufInfo) != VK_SUCCESS) {
				throw std::runtime_error("Could not begin command buffer!");
			}

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkDeviceSize offsets[] = { 0 };
			{
				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.attachmentWrite, 0, 1, &descriptorSets.attachmentWrite[i], 0, nullptr);

				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
				VkBuffer skyboxVBuffers[] = { skyboxBuffer.vBuffer };
				vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, skyboxVBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffers[i], skyboxBuffer.iBuffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(box.indices.size()), 1, 0, 0, 0);

				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.attachmentWrite);

				VkBuffer vertexBuffers[] = { meshBuffer.vBuffer };
				vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffers[i], meshBuffer.iBuffer, 0, VK_INDEX_TYPE_UINT32);

				vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
			}

			{
				vkCmdNextSubpass(commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.attachmentRead);


				VkBuffer vertexBuffers[] = { blitMeshBuffer.vBuffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffers[i], blitMeshBuffer.iBuffer, 0, VK_INDEX_TYPE_UINT32);

				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.attachmentRead, 0, 1, &descriptorSets.attachmentRead[i], 0, nullptr);
				vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(blit.indices.size()), 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(commandBuffers[i]);

			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to end command buffer record!");
			}
		}
	}

	void createDescriptorSets() {
		// create descriptor pool
		std::array<VkDescriptorPoolSize, 3> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size() * 2);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainImages.size() * 3);
		poolSizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		poolSizes[2].descriptorCount = static_cast<uint32_t>(swapChainImages.size());

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size() * 2);

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor pool!");
		}

		// write attachment
		{
			std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayouts.attachmentWrite);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
			allocInfo.pSetLayouts = layouts.data();

			descriptorSets.attachmentWrite.resize(swapChainImages.size());
			if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.attachmentWrite.data()) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate descriptor sets!");
			}

			for (size_t i = 0; i < swapChainImages.size(); i++) {
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = uniformBuffers[i];
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(UniformBufferObject);

				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = textureImage.imageView;
				imageInfo.sampler = textureSampler;

				VkDescriptorImageInfo cubemapInfo{};
				cubemapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				cubemapInfo.imageView = cubeMap.imageView;
				cubemapInfo.sampler = cubeMapSampler;

				std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = descriptorSets.attachmentWrite[i];
				descriptorWrites[0].dstBinding = 0;
				descriptorWrites[0].dstArrayElement = 0;
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[0].descriptorCount = 1;
				descriptorWrites[0].pBufferInfo = &bufferInfo;

				descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[1].dstSet = descriptorSets.attachmentWrite[i];
				descriptorWrites[1].dstBinding = 1;
				descriptorWrites[1].dstArrayElement = 0;
				descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[1].descriptorCount = 1;
				descriptorWrites[1].pImageInfo = &imageInfo;

				descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[2].dstSet = descriptorSets.attachmentWrite[i];
				descriptorWrites[2].dstBinding = 2;
				descriptorWrites[2].dstArrayElement = 0;
				descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[2].descriptorCount = 1;
				descriptorWrites[2].pImageInfo = &cubemapInfo;

				vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			}
		}
		{
			std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayouts.attachmentRead);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
			allocInfo.pSetLayouts = layouts.data();

			descriptorSets.attachmentRead.resize(swapChainImages.size());
			if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.attachmentRead.data()) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate descriptor sets!");
			}

			for (size_t i = 0; i < swapChainImages.size(); i++) {
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = uniformBuffers[i];
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(UniformBufferObject);

				VkDescriptorImageInfo inputAttachmentInfo{};
				inputAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				inputAttachmentInfo.imageView = postImage.imageView;
				inputAttachmentInfo.sampler = postImageSampler;

				std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = descriptorSets.attachmentRead[i];
				descriptorWrites[0].dstBinding = 0;
				descriptorWrites[0].dstArrayElement = 0;
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[0].descriptorCount = 1;
				descriptorWrites[0].pBufferInfo = &bufferInfo;

				descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[1].dstSet = descriptorSets.attachmentRead[i];
				descriptorWrites[1].dstBinding = 1;
				descriptorWrites[1].dstArrayElement = 0;
				descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[1].descriptorCount = 1;
				descriptorWrites[1].pImageInfo = &inputAttachmentInfo;

				vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			}
		}
	}

	void mainLoop() {
		std::chrono::system_clock::time_point a = std::chrono::system_clock::now();
		std::chrono::system_clock::time_point b = std::chrono::system_clock::now();
		std::chrono::duration<double, std::milli> elapsed;
		double ms = 1000.0 / 60.0;

		while (!glfwWindowShouldClose(bvk::window::window)) {
			a = std::chrono::system_clock::now();
			elapsed = a - b;

			if (elapsed.count() < ms)
			{
				std::chrono::duration<double, std::milli> delta_ms(ms - elapsed.count());
				auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
				std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration.count()));
			}
			elapsed = std::chrono::system_clock::now() - b;
			//std::cout << 1000 / (int)elapsed.count() << " FPS" << std::endl;

			b = std::chrono::system_clock::now();

			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle(device);
	}

	void drawFrame() {
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
			imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		// check for out of date swap chain
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Failed to acquire swap chain image!");
		}

		// Check if a previous frame is using this image (i.e. there is its fence to wait on)
		if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}
		// Mark the image as now being in use by this frame
		imagesInFlight[imageIndex] = inFlightFences[currentFrame];

		updateUniformBuffer(imageIndex);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(device, 1, &inFlightFences[currentFrame]);
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to submit draw command buffer!");
		}

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // Optional

		result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void cleanupSwapChain() {
		destroyImage(device, depthImage);
		destroyImage(device, colorImage);
		destroyImage(device, postImage);
		vkDestroySampler(device, postImageSampler, nullptr);

		for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
		}

		vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

		vkDestroyPipeline(device, pipelines.skybox, nullptr);

		vkDestroyPipeline(device, pipelines.attachmentWrite, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.attachmentWrite, nullptr);

		vkDestroyPipeline(device, pipelines.attachmentRead, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.attachmentRead, nullptr);

		vkDestroyRenderPass(device, renderPass, nullptr);

		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			vkDestroyImageView(device, swapChainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);

		for (size_t i = 0; i < swapChainImages.size(); i++) {
			vkDestroyBuffer(device, uniformBuffers[i], nullptr);
			vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
		}


		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.attachmentWrite, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.attachmentRead, nullptr);

		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}
	void recreateSwapChain() {
		int width = 0, height = 0;
		glfwGetFramebufferSize(bvk::window::window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(bvk::window::window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(device);

		cleanupSwapChain();

		bvk::createSwapChain();
		bvk::createSwapchainImageViews();

		createDescriptorSetLayout();
		createRenderPass();
		createGraphicsPipelines();
		createImages();
		createFramebuffers();
		createUniformBuffers();
		createDescriptorSets();
		createCommandBuffers();
	}

	void cleanup() {
		cleanupSwapChain();

		vkDestroySampler(device, textureSampler, nullptr);
		destroyImage(device, textureImage);

		vkDestroySampler(device, cubeMapSampler, nullptr);
		destroyImage(device, cubeMap);

		destroyMeshBuffer(device, meshBuffer);
		destroyMeshBuffer(device, blitMeshBuffer);
		destroyMeshBuffer(device, skyboxBuffer);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}
		vkDestroyCommandPool(device, commandPool, nullptr);

		vkDestroyDevice(device, nullptr);
		if (enableValidationLayers) {
			bvk::debug::DestroyDebugUtilsMessengerEXT(bvk::instance, nullptr);
		}
		vkDestroySurfaceKHR(bvk::instance, surface, nullptr);
		vkDestroyInstance(bvk::instance, nullptr);

		glfwDestroyWindow(bvk::window::window);
		glfwTerminate();
		std::cout << "Clean exit.\n";
	}
};


int main() {
	VulkanDemo app;

	try {

		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}