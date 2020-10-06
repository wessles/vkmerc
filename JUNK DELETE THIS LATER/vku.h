#ifndef _vku_HPP_
#define _vku_HPP_

#include <vulkan/vulkan.h>
#include <glfw3/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <functional>
#include <iostream>
#include <optional>
#include <vector>
#include <string>
#include <array>
#include <set>

namespace vku {
	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
	};
}

namespace vku::state {
	bool debugEnabled;

	GLFWwindow *windowHandle;

	VkInstance instance;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSurfaceKHR surface;
	
	QueueFamilyIndices queueFamilyIndices;

	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat screenFormat;
	VkFormat depthFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;

	VkCommandPool commandPool;
	VkDescriptorPool descriptorPool;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	const int MAX_FRAMES_IN_FLIGHT = 2;
	const int SWAPCHAIN_SIZE = 3;

	namespace {
		const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
		const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	}
}

using namespace vku;

namespace vku::debug {
	VkDebugUtilsMessengerEXT debugMessenger;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		std::cerr << "Validation layer: " << std::endl << pCallbackData->pMessage << std::endl;
 		if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) return VK_TRUE; // TODO remove, this is bad
		return VK_FALSE;
	}
	void setupDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
		createInfo.pUserData = nullptr; // Optional
	}
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, &debugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}
	void setupDebugMessenger() {
		// set up options for which messages this callback gets
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		setupDebugMessengerCreateInfo(createInfo);

		VkResult result = CreateDebugUtilsMessengerEXT(state::instance, &createInfo, nullptr);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Could not set up debug messenger.");
		}
	}
	void DestroyDebugUtilsMessengerEXT(VkInstance instance) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, nullptr);
		}
	}
}

namespace vku::window {
	namespace {
		std::function<void(GLFWwindow*, int, int)> resizeCallback;
		static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
			resizeCallback(window, width, height);
		}
	}
	void initWindow(const std::string& name, int WIDTH, int HEIGHT, const std::function<void(GLFWwindow*, int, int)>& resizeCallback) {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		state::windowHandle = glfwCreateWindow(WIDTH, HEIGHT, name.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(state::windowHandle, framebufferResizeCallback);
		glfwSetFramebufferSizeCallback(state::windowHandle, framebufferResizeCallback);
		vku::window::resizeCallback = resizeCallback;
	}
}

namespace vku::swap {
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice);
}

namespace vku::support {
	QueueFamilyIndices findQueueFamiliesSupported(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, state::surface, &presentSupport);
			if (presentSupport) {
				indices.presentFamily = i;
			}

			i++;
		}

		return indices;
	}
	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : vku::state::validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}
	std::vector<const char*> getRequiredGLFWExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (vku::state::debugEnabled) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}
	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(vku::state::deviceExtensions.begin(), vku::state::deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}
	int deviceSuitabilityRating(VkPhysicalDevice device) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		QueueFamilyIndices familyIndices = findQueueFamiliesSupported(device);
		SwapChainSupportDetails swapchainSupport = swap::querySwapChainSupport(device);

		bool hasGraphicsQueueFamily = familyIndices.graphicsFamily.has_value();
		bool hasPresentQueueFamily = familyIndices.presentFamily.has_value();
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			swapChainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
		}

		// disqualifiers
		if (!hasGraphicsQueueFamily
			|| !hasPresentQueueFamily
			|| !extensionsSupported
			|| !swapChainAdequate
			|| !deviceFeatures.samplerAnisotropy) {
			return -1;
		}

		// begin scoring.

		bool isGPU = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

		int score = 0;

		if (isGPU) score += 1000;

		return score;
	}
	VkSampleCountFlagBits getMaxUsableMSAASampleCount() {
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(state::physicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}
	uint32_t findSupportedMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(state::physicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("Could not find suitable memory type!");
	}
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(state::physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}
		throw std::runtime_error("Failed to find supported format!");
	}
	VkFormat findDepthFormat() {
		return findSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}
	bool hasStencilComponent(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}
	void pickPhysicalDevice() {
		// find all devices
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(state::instance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			throw std::runtime_error("Failed to find any GPUs with Vulkan support.");
		}

		// create array with all VkPhysicalDevice handles
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(state::instance, &deviceCount, devices.data());

		// find best device
		int bestSuitabilityScore = 0;
		for (const auto& device : devices) {
			int score = vku::support::deviceSuitabilityRating(device);
			if (score > bestSuitabilityScore) {
				bestSuitabilityScore = score;
				state::physicalDevice = device;
			}
		}
		if (state::physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("Failed to find a suitable GPU!");
		}

		// save support to state
		vku::state::queueFamilyIndices = vku::support::findQueueFamiliesSupported(state::physicalDevice);
		vku::state::msaaSamples = vku::support::getMaxUsableMSAASampleCount();
	}
}


namespace vku::create {
	VkShaderModule createShaderModule(const std::vector<uint32_t>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size() * sizeof(uint32_t);
		createInfo.pCode = code.data();

		VkShaderModule shaderModule;
		VkResult result = vkCreateShaderModule(state::device, &createInfo, nullptr, &shaderModule);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create shader module!");
		}

		return shaderModule;
	}
	VkPipelineShaderStageCreateInfo createShaderStage(VkShaderModule shaderModule, VkShaderStageFlagBits shaderStage) {


		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = shaderStage;
		shaderStageInfo.module = shaderModule;
		shaderStageInfo.pName = "main";

		return shaderStageInfo;
	}
	VkSamplerCreateInfo createSamplerCI() {
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;

		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16.0f;

		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		samplerInfo.unnormalizedCoordinates = VK_FALSE;

		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0.0f; // Optional
		samplerInfo.maxLod = 0.0f;
		samplerInfo.mipLodBias = 0.0f; // Optional

		return samplerInfo;
	}
}

namespace vku::cmd {
	VkCommandBuffer beginCommandBuffer(bool oneTimeUse = false, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = state::commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(state::device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		if (oneTimeUse)
			beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void submitCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue) {
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue);

		vkFreeCommandBuffers(state::device, state::commandPool, 1, &commandBuffer);
	}
}

namespace vku::buffer {

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBuffer commandBuffer = vku::cmd::beginCommandBuffer(true);

		// copy region
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		vku::cmd::submitCommandBuffer(commandBuffer, vku::state::graphicsQueue);
	}
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(state::device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create vertex buffer!");
		}

		// find memory type index that is suitable
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(state::device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = vku::support::findSupportedMemoryType(memRequirements.memoryTypeBits, properties);

		// allocate the memory
		if (vkAllocateMemory(state::device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate vertex buffer memory!");
		}

		// copy vertex data to newly allocated buffer
		vkBindBufferMemory(state::device, buffer, bufferMemory, 0);
	}
	template <class Type>
	void initDeviceLocalBuffer(std::vector<Type> &bufferData, VkBuffer &buffer, VkDeviceMemory &deviceMemory, VkBufferUsageFlagBits bufferUsageBit) {
		VkDeviceSize bufferSize = sizeof(bufferData[0]) * bufferData.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory);

		void* data;
		vkMapMemory(state::device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, bufferData.data(), (size_t)bufferSize);
		vkUnmapMemory(state::device, stagingBufferMemory);

		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsageBit,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			buffer,
			deviceMemory);

		// copy staging buffer to vertex buffer
		copyBuffer(stagingBuffer, buffer, bufferSize);

		vkDestroyBuffer(state::device, stagingBuffer, nullptr);
		vkFreeMemory(state::device, stagingBufferMemory, nullptr);
	}
}

namespace vku::mesh {
	struct Vertex {
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;
		glm::vec3 normal;
		glm::vec4 tangent;
		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};

			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
			std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
			attributeDescriptions.resize(5);

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

			attributeDescriptions[4].binding = 0;
			attributeDescriptions[4].location = 4;
			attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[4].offset = offsetof(Vertex, tangent);

			return attributeDescriptions;
		}
		bool operator==(const Vertex& other) const {
			return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal && tangent == other.tangent;
		}
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
		uint32_t indicesSize;

		void load(MeshData& mesh) {
			vku::buffer::initDeviceLocalBuffer<uint32_t>(mesh.indices, iBuffer, iMemory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
			vku::buffer::initDeviceLocalBuffer<Vertex>(mesh.vertices, vBuffer, vMemory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			indicesSize = mesh.indices.size();
		}
		void destroy() {
			vkDestroyBuffer(vku::state::device, vBuffer, nullptr);
			vkDestroyBuffer(vku::state::device, iBuffer, nullptr);
			vkFreeMemory(vku::state::device, vMemory, nullptr);
			vkFreeMemory(vku::state::device, iMemory, nullptr);
		}
	};

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
			0, 2, 3,
			0, 3, 1,
			1, 7, 5,
			1, 3, 7,
			4, 5, 7,
			4, 7, 6,
			4, 6, 2,
			4, 2, 0,
			6, 7, 2,
			2, 7, 3,
			1, 5, 4,
			0, 1, 4
		}
	};
	MeshData blit = {
		{
			{{-1,-1,0}, {},{0,0},{}},
			{{-1,3,0}, {},{0,2},{}},
			{{3,-1,0}, {},{2,0},{}},
		},
		{0,1,2}
	};
}
namespace std {
	template<> struct hash<vku::mesh::Vertex> {
		size_t operator()(vku::mesh::Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

namespace vku::io {
	std::vector<uint32_t> readFileIntVec(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t), 0);

		file.seekg(0);
		file.read((char*)buffer.data(), fileSize);

		file.close();

		return buffer;
	}
	std::vector<char> readFileCharVec(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read((char*)buffer.data(), fileSize);

		file.close();

		return buffer;
	}
}

namespace vku::sync {
	void createSyncObjects() {
		state::imageAvailableSemaphores.resize(state::MAX_FRAMES_IN_FLIGHT);
		state::renderFinishedSemaphores.resize(state::MAX_FRAMES_IN_FLIGHT);
		state::inFlightFences.resize(state::MAX_FRAMES_IN_FLIGHT);
		state::imagesInFlight.resize(state::swapChainImages.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < state::MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(state::device, &semaphoreInfo, nullptr, &state::imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(state::device, &semaphoreInfo, nullptr, &state::renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(state::device, &fenceInfo, nullptr, &state::inFlightFences[i]) != VK_SUCCESS) {

				throw std::runtime_error("Failed to create synchronization objects for a frame!");
			}
		}
	}
}

namespace vku::descriptor {
	struct DescriptorLayout {
		VkDescriptorType type;
		VkShaderStageFlags stageFlags;
	};
	void createDescriptorSetLayout(VkDescriptorSetLayout *layout, std::vector<DescriptorLayout> descriptorLayouts) {
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		int bindingIndex = 0;
		for (auto& descriptorLayout : descriptorLayouts) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = bindingIndex++;
			binding.descriptorCount = 1;
			binding.pImmutableSamplers = nullptr;
			binding.descriptorType = descriptorLayout.type;
			binding.stageFlags = descriptorLayout.stageFlags;
			bindings.push_back(binding);
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		if (bindings.size() == 0) {
			layoutInfo.pBindings = nullptr;
		} else {
			layoutInfo.pBindings = &bindings[0];
		}
		if (vkCreateDescriptorSetLayout(vku::state::device, &layoutInfo, nullptr, layout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor set layout.");
		}
	}
}

#endif