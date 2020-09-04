#ifndef _vku_HPP_
#define _vku_HPP_

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // for overriding opengl defaults
#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "stb_image.h"
#include "tiny_obj_loader.h"

#include <functional>
#include <iostream>
#include <optional>
#include <vector>
#include <string>
#include <array>
#include <set>

namespace vku::state {
	bool debugEnabled;

	GLFWwindow *windowHandle;

	VkInstance instance;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSurfaceKHR surface;

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
	} queueFamilyIndices;

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
	state::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice);
}

namespace vku::support {
	state::QueueFamilyIndices findQueueFamiliesSupported(VkPhysicalDevice device) {
		state::QueueFamilyIndices indices;

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

		state::QueueFamilyIndices familyIndices = findQueueFamiliesSupported(device);
		state::SwapChainSupportDetails swapchainSupport = swap::querySwapChainSupport(device);

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
	VkShaderModule createShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

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
	VkCommandBuffer beginSingleTimeCommands() {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = state::commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(state::device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}
	void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(state::graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(state::graphicsQueue);

		vkFreeCommandBuffers(state::device, state::commandPool, 1, &commandBuffer);
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

namespace vku::buffer {

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBuffer commandBuffer = vku::create::beginSingleTimeCommands();

		// copy region
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		vku::create::endSingleTimeCommands(commandBuffer);
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
		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};

			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
			std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
			attributeDescriptions.resize(4);

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

namespace vku::image {
	struct Image {
		VkImage handle;
		VkDeviceMemory memory;
		VkImageView view;
		VkSampler sampler;
		uint32_t mipLevels = 1;

	private:
		VkDescriptorImageInfo info;

	public:
		VkWriteDescriptorSet getDescriptorWrite(uint32_t binding, VkDescriptorSet descriptorSet, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			info.imageLayout = layout;
			info.imageView = view;
			info.sampler = sampler;

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = descriptorSet;
			descriptorWrite.dstBinding = binding;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pImageInfo = &info;

			return descriptorWrite;
		}
	};

	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
		// check for linear blitting support
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(state::physicalDevice, imageFormat, &formatProperties);
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("Texture image format does not support linear blitting.");
		}

		VkCommandBuffer commandBuffer = vku::create::beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			// blit from mip level i-1 to i, at half the resolution
			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			// move mip level i-1 from SRC to SHADER_READ_ONLY layout, since we're done blitting
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			// reduce resolution for next mip blit (fun word)
			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		// before we're done, just transition that last mip level from DST to READ_ONLY
		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		vku::create::endSingleTimeCommands(commandBuffer);
	}
	void createImage(
		uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
		VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
		VkImage& image, VkDeviceMemory& imageMemory,
		VkImageCreateFlags imageCreateFlags = 0, uint32_t arrayLayers = 1)
	{
		// set up image create, as a dst for the staging buffer we just made
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = arrayLayers;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = numSamples;
		imageInfo.flags = imageCreateFlags;

		if (vkCreateImage(state::device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(state::device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = vku::support::findSupportedMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(state::device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate image memory!");
		}

		vkBindImageMemory(state::device, image, imageMemory, 0);
	}
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount = 1) {
		VkCommandBuffer commandBuffer = vku::create::beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;

		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = layerCount;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		// special stuff for depth and stencil attachment
		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (vku::support::hasStencilComponent(format)) {
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}
		}

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else {
			throw std::invalid_argument("Unsupported layout transition!");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		vku::create::endSingleTimeCommands(commandBuffer);
	}
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount = 1) {
		VkCommandBuffer commandBuffer = vku::create::beginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = layerCount;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		vku::create::endSingleTimeCommands(commandBuffer);
	}
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D, uint32_t layerCount = 1) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = imageViewType;
		viewInfo.format = format;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = layerCount;
		viewInfo.subresourceRange.aspectMask = aspectFlags;

		VkImageView view;
		if (vkCreateImageView(state::device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture image view!");
		}

		return view;
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
		vku::buffer::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(state::device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(state::device, stagingBufferMemory);

		stbi_image_free(pixels);

		vku::image::createImage(texWidth, texHeight, image.mipLevels, VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			image.handle, image.memory);
		// transition layout to transfer destination optimized type
		vku::image::transitionImageLayout(image.handle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.mipLevels);
		// copy data to image
		vku::image::copyBufferToImage(stagingBuffer, image.handle, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		// transition layout to readonly shader data, and generate mip maps (even if it's just one)
		vku::image::generateMipmaps(image.handle, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, image.mipLevels);


		vkDestroyBuffer(state::device, stagingBuffer, nullptr);
		vkFreeMemory(state::device, stagingBufferMemory, nullptr);
	}
	void loadCubemap(const std::array<std::string, 6> &paths, Image& image) {
		/*
		negx, y, z
		posx, y, z
		*/
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
		vku::buffer::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(state::device, stagingBufferMemory, 0, imageSize, 0, &data);
		for (int i = 0; i < paths.size(); i++) {
			memcpy(static_cast<char*>(data) + (layerSize * i), faceData[i], static_cast<size_t>(layerSize));
		}
		vkUnmapMemory(state::device, stagingBufferMemory);

		for (int i = 0; i < paths.size(); i++) {
			stbi_image_free(faceData[i]);
		}

		vku::image::createImage(texWidth, texHeight, image.mipLevels, VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			image.handle, image.memory,
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, paths.size());
		// transition layout to transfer destination optimized type
		vku::image::transitionImageLayout(image.handle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.mipLevels, paths.size());
		// copy data to image
		vku::image::copyBufferToImage(stagingBuffer, image.handle, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), paths.size());
		// transition layout to readonly shader data, and generate mip maps (even if it's just one)
		//vku::image::generateMipmaps(image.image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, image.mipLevels);
		vku::image::transitionImageLayout(image.handle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, image.mipLevels, paths.size());

		vkDestroyBuffer(state::device, stagingBuffer, nullptr);
		vkFreeMemory(state::device, stagingBufferMemory, nullptr);

	}
	void destroyImage(const VkDevice& device, Image& image) {
		vkDestroySampler(device, image.sampler, nullptr);
		vkDestroyImageView(device, image.view, nullptr);
		vkDestroyImage(device, image.handle, nullptr);
		vkFreeMemory(device, image.memory, nullptr);
	}
}

namespace vku::io {
	std::vector<char> readFile(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

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

namespace vku::swap {
	namespace {
		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
			// prefer SRGB if we can get it, because it's the best for our purposes
			for (const auto& availableFormat : availableFormats) {
				if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					return availableFormat;
				}
			}

			// if we can't have SRGB, fine. We'll just do whatever.
			return availableFormats[0];
		}
		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
			// look to see if we can get triple buffering
			for (const auto& availablePresentMode : availablePresentModes) {
				if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
					return availablePresentMode;
				}
			}

			// fallback to regular ol' vsync
			return VK_PRESENT_MODE_FIFO_KHR;
		}
		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
			if (capabilities.currentExtent.width != UINT32_MAX) {
				return capabilities.currentExtent;
			}
			else {
				int width, height;
				glfwGetFramebufferSize(state::windowHandle, &width, &height);

				VkExtent2D actualExtent = {
					static_cast<uint32_t>(width),
					static_cast<uint32_t>(height)
				};

				actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
				actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

				return actualExtent;
			}
		}
	}
	state::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		state::SwapChainSupportDetails details;

		// query supported surface capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, state::surface, &details.capabilities);

		// query supported surface formats
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, state::surface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, state::surface, &formatCount, details.formats.data());
		}

		// query supported presentation modes
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, state::surface, &presentModeCount, nullptr);
		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, state::surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}
	void createSwapChain() {
		state::SwapChainSupportDetails swapchainSupport = querySwapChainSupport(vku::state::physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

		// image count should be minImageCount+1, but also not exceeding the maxImageCount
		uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
		if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
			imageCount = swapchainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = state::surface;

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if (state::queueFamilyIndices.graphicsFamily != state::queueFamilyIndices.presentFamily) {
			uint32_t queueIndices[] = { state::queueFamilyIndices.graphicsFamily.value(), state::queueFamilyIndices.presentFamily.value() };
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueIndices;
		} else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		// no transform
		createInfo.preTransform = swapchainSupport.capabilities.currentTransform;

		// we don't care about alpha blending on a window
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		// we don't care to render pixels that are behind another window, so clip.
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		createInfo.oldSwapchain = VK_NULL_HANDLE;

		// create the swap chain
		bool result = vkCreateSwapchainKHR(state::device, &createInfo, nullptr, &state::swapChain);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create swap chain!");
		}

		// retrieval...

		vkGetSwapchainImagesKHR(state::device, state::swapChain, &imageCount, nullptr);
		state::swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(state::device, state::swapChain, &imageCount, state::swapChainImages.data());

		state::screenFormat = surfaceFormat.format;
		state::depthFormat = support::findDepthFormat();
		state::swapChainExtent = extent;

		// create image views
		state::swapChainImageViews.resize(state::swapChainImages.size());
		for (size_t i = 0; i < state::swapChainImages.size(); i++) {
			state::swapChainImageViews[i] = vku::image::createImageView(state::swapChainImages[i], state::screenFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		}
	}
	void cleanupSwapchain() {
		for (size_t i = 0; i < state::swapChainImageViews.size(); i++) {
			vkDestroyImageView(state::device, state::swapChainImageViews[i], nullptr);
		}
		state::swapChainImageViews.clear();

		vkDestroySwapchainKHR(state::device, state::swapChain, nullptr);
	}
	void recreateSwapchain() {
		cleanupSwapchain();
		createSwapChain();
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

namespace vku {

	namespace {
		void createInstance(bool debug) {
			if (vku::state::debugEnabled) {
				if (!vku::support::checkValidationLayerSupport()) {
					throw std::runtime_error("Validation layers requested, but not available!");
				}
				else {
					std::cout << "Will attempt to enable validation layer\n";
				}
			}

			// basic information about the instance
			VkApplicationInfo appInfo{};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "merc";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "MERC";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_0;

			// creation definition begins, pass app info
			VkInstanceCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &appInfo;

			// find required extensions from GLFW
			auto extensions = vku::support::getRequiredGLFWExtensions();
			createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
			createInfo.ppEnabledExtensionNames = extensions.data();

			// enable validation layers if need be
			VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
			if (vku::state::debugEnabled) {
				createInfo.enabledLayerCount = static_cast<uint32_t>(vku::state::validationLayers.size());
				createInfo.ppEnabledLayerNames = vku::state::validationLayers.data();

				vku::debug::setupDebugMessengerCreateInfo(debugCreateInfo);
				createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
			}
			else {
				createInfo.enabledLayerCount = 0;
				createInfo.pNext = nullptr;
			}

			VkResult result = vkCreateInstance(&createInfo, nullptr, &state::instance);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to create Vulkan instance!");
			}

			// enumerate possible extensions
			std::cout << "available extensions:\n";
			for (const auto& ext : extensions) {
				std::cout << '\t' << ext << '\n';
			}

			if (vku::state::debugEnabled)
				vku::debug::setupDebugMessenger();
		}
		void createSurface() {
			if (glfwCreateWindowSurface(state::instance, state::windowHandle, nullptr, &state::surface) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create window surface!");
			}
		}
		void createLogicalDevice() {
			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
			std::set<uint32_t> uniqueQueueFamilies = { state::queueFamilyIndices.graphicsFamily.value(), state::queueFamilyIndices.presentFamily.value() };

			float queuePriority = 1.0f;
			for (uint32_t queueFamily : uniqueQueueFamilies) {
				VkDeviceQueueCreateInfo queueCreateInfo{};
				queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfo.queueFamilyIndex = queueFamily;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &queuePriority;
				queueCreateInfos.push_back(queueCreateInfo);
			}

			VkPhysicalDeviceFeatures deviceFeatures{};
			deviceFeatures.samplerAnisotropy = VK_TRUE;
			// enable sample shading feature for the device, so that edges AND interiors of textures are MSAA'd
			deviceFeatures.sampleRateShading = VK_TRUE;
			deviceFeatures.tessellationShader = VK_TRUE;
			deviceFeatures.fillModeNonSolid = VK_TRUE;
			deviceFeatures.wideLines = VK_TRUE;


			VkDeviceCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
			createInfo.pQueueCreateInfos = queueCreateInfos.data();

			createInfo.pEnabledFeatures = &deviceFeatures;

			// enable all the extensions we need
			createInfo.enabledExtensionCount = static_cast<uint32_t>(vku::state::deviceExtensions.size());
			createInfo.ppEnabledExtensionNames = vku::state::deviceExtensions.data();

			// this part is not necessary as it fell out of spec (per device validation layers were removed)
			// but we will keep it for backwards compatibility.
			if (vku::state::debugEnabled) {
				createInfo.enabledLayerCount = static_cast<uint32_t>(vku::state::validationLayers.size());
				createInfo.ppEnabledLayerNames = vku::state::validationLayers.data();
			}
			else {
				createInfo.enabledLayerCount = 0;
			}

			VkResult result = vkCreateDevice(state::physicalDevice, &createInfo, nullptr, &state::device);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Could not create logical device!");
			}

			vkGetDeviceQueue(state::device, state::queueFamilyIndices.graphicsFamily.value(), 0, &state::graphicsQueue);
			vkGetDeviceQueue(state::device, state::queueFamilyIndices.presentFamily.value(), 0, &state::presentQueue);
		}
		void createCommandPool() {
			VkCommandPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.queueFamilyIndex = state::queueFamilyIndices.graphicsFamily.value();
			poolInfo.flags = 0; // Optional

			if (vkCreateCommandPool(state::device, &poolInfo, nullptr, &state::commandPool) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create command pool!");
			}
		}
	}
	void init(bool debug, const std::string& windowName, VkExtent2D windowSize, std::function<void(GLFWwindow*,int,int)> resizeCallback) {
		const auto& resizeCallbackFunc = std::function<void(GLFWwindow*, int, int)>(resizeCallback);
		vku::window::initWindow(windowName, windowSize.width, windowSize.height, resizeCallbackFunc);

		vku::state::debugEnabled = debug;

		vku::createInstance(debug);
		vku::createSurface();
		vku::support::pickPhysicalDevice();
		vku::createLogicalDevice();
		vku::swap::createSwapChain();
		vku::createCommandPool();
		vku::sync::createSyncObjects();
	}
	void cleanup() {
		for (size_t i = 0; i < state::MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(state::device, state::renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(state::device, state::imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(state::device, state::inFlightFences[i], nullptr);
		}
		
		vkDestroyCommandPool(state::device, state::commandPool, nullptr);
		vkDestroyDevice(state::device, nullptr);

		if (vku::state::debugEnabled) {
			vku::debug::DestroyDebugUtilsMessengerEXT(vku::state::instance);
		}

		vkDestroySurfaceKHR(state::instance, state::surface, nullptr);
		vkDestroyInstance(state::instance, nullptr);
		glfwDestroyWindow(vku::state::windowHandle);
		glfwTerminate();
	}
}

#endif