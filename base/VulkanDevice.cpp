#include "VulkanDevice.h"

#include <set>
#include <optional>
#include <stdexcept>

#include "VulkanContext.h"
#include "VulkanSwapchain.h"

namespace vku {
	bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*> deviceExtensions) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	DeviceSwapchainSupportInformation VulkanDevice::queryDeviceSwapchainSupportInformation(VkPhysicalDevice physicalDevice) {
		DeviceSwapchainSupportInformation swapchainSupportInfo;

		// query supported surface capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, context->surface, &swapchainSupportInfo.capabilities);

		// query supported surface formats
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, context->surface, &formatCount, nullptr);
		if (formatCount != 0) {
			swapchainSupportInfo.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, context->surface, &formatCount, swapchainSupportInfo.formats.data());
		}

		// query supported presentation modes
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, context->surface, &presentModeCount, nullptr);
		if (presentModeCount != 0) {
			swapchainSupportInfo.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, context->surface, &presentModeCount, swapchainSupportInfo.presentModes.data());
		}

		return swapchainSupportInfo;
	}

	DeviceSupportInformation VulkanDevice::queryDeviceSupportInformation(VkPhysicalDevice physicalDevice) {
		DeviceSupportInformation supportInfo;

		// find queue indices for graphics / present
		{
			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

			int i = 0;
			for (const auto& queueFamily : queueFamilies) {
				if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					supportInfo.graphicsFamily = i;
				}

				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, context->surface, &presentSupport);
				if (presentSupport) {
					supportInfo.presentFamily = i;
				}

				i++;
			}
		}

		// device props / features
		vkGetPhysicalDeviceProperties(physicalDevice, &supportInfo.deviceProperties);
		vkGetPhysicalDeviceFeatures(physicalDevice, &supportInfo.deviceFeatures);

		// get max MSAA sample count
		{
			vkGetPhysicalDeviceProperties(physicalDevice, &supportInfo.deviceProperties);

			VkSampleCountFlags counts = supportInfo.deviceProperties.limits.framebufferColorSampleCounts & supportInfo.deviceProperties.limits.framebufferDepthSampleCounts;
			if (counts & VK_SAMPLE_COUNT_64_BIT) { supportInfo.maxSampleCount = VK_SAMPLE_COUNT_64_BIT; }
			if (counts & VK_SAMPLE_COUNT_32_BIT) { supportInfo.maxSampleCount = VK_SAMPLE_COUNT_32_BIT; }
			if (counts & VK_SAMPLE_COUNT_16_BIT) { supportInfo.maxSampleCount = VK_SAMPLE_COUNT_16_BIT; }
			if (counts & VK_SAMPLE_COUNT_8_BIT) { supportInfo.maxSampleCount = VK_SAMPLE_COUNT_8_BIT; }
			if (counts & VK_SAMPLE_COUNT_4_BIT) { supportInfo.maxSampleCount = VK_SAMPLE_COUNT_4_BIT; }
			if (counts & VK_SAMPLE_COUNT_2_BIT) { supportInfo.maxSampleCount = VK_SAMPLE_COUNT_2_BIT; }

			supportInfo.maxSampleCount = VK_SAMPLE_COUNT_1_BIT;
		}

		return supportInfo;
	}


	int deviceSuitabilityRating(DeviceSupportInformation supportInfo, DeviceSwapchainSupportInformation swapchainSupportInfo) {
		bool hasGraphicsQueueFamily = supportInfo.graphicsFamily.has_value();
		bool hasPresentQueueFamily = supportInfo.presentFamily.has_value();
		bool swapChainAdequate = !swapchainSupportInfo.formats.empty() && !swapchainSupportInfo.presentModes.empty();

		// disqualifiers
		if (!hasGraphicsQueueFamily || !hasPresentQueueFamily || !swapChainAdequate || !supportInfo.deviceFeatures.samplerAnisotropy) {
			return -1;
		}

		// begin scoring.

		bool isGPU = supportInfo.deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

		int score = 0;
		if (isGPU) score += 1000;

		return score;
	}

	VulkanDevice::VulkanDevice(VulkanDeviceInfo info)
	{
		this->context = info.context;

		const std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		// find the best physical device
		{
			// find all devices
			uint32_t deviceCount = 0;
			vkEnumeratePhysicalDevices(info.context->instance, &deviceCount, nullptr);
			if (deviceCount == 0) {
				throw std::runtime_error("Failed to find any GPUs with Vulkan support.");
			}

			// create array with all VkPhysicalDevice handles
			std::vector<VkPhysicalDevice> devices(deviceCount);
			vkEnumeratePhysicalDevices(info.context->instance, &deviceCount, devices.data());

			// find best device
			int bestSuitabilityScore = 0;
			for (const auto& device : devices) {
				if (!checkDeviceExtensionSupport(device, extensions)) {
					continue;
				}

				DeviceSupportInformation supportInfo = queryDeviceSupportInformation(device);
				DeviceSwapchainSupportInformation swapchainSupportInfo = queryDeviceSwapchainSupportInformation(device);
				int score = deviceSuitabilityRating(supportInfo, swapchainSupportInfo);

				if (score > bestSuitabilityScore) {
					bestSuitabilityScore = score;
					this->supportInfo = supportInfo;
					this->swapchainSupportInfo = swapchainSupportInfo;
					this->physicalDevice = device;
				}
			}
			if (this->physicalDevice == VK_NULL_HANDLE) {
				throw std::runtime_error("Failed to find a suitable GPU!");
			}
		}

		// create logical device
		{
			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
			std::set<uint32_t> uniqueQueueFamilies = { supportInfo.graphicsFamily.value(), supportInfo.presentFamily.value() };

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
			createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
			createInfo.ppEnabledExtensionNames = extensions.data();

			createInfo.enabledLayerCount = 0;

			VkResult result = vkCreateDevice(this->physicalDevice, &createInfo, nullptr, &this->handle);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Could not create logical device!");
			}

			vkGetDeviceQueue(this->handle, supportInfo.graphicsFamily.value(), 0, &this->graphicsQueue);
			vkGetDeviceQueue(this->handle, supportInfo.presentFamily.value(), 0, &this->presentQueue);
		}

		// create command pool
		{
			VkCommandPoolCreateInfo commandPoolCI{};
			commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			commandPoolCI.queueFamilyIndex = supportInfo.graphicsFamily.value();
			vkCreateCommandPool(this->handle, &commandPoolCI, nullptr, &this->commandPool);
		}

		// create descriptor pool
		{
			VkDescriptorPoolCreateInfo descriptorPoolCI{};
			descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(info.descriptorPoolSizes.size());
			descriptorPoolCI.pPoolSizes = info.descriptorPoolSizes.data();
			descriptorPoolCI.maxSets = info.maxDescriptorSets;
			descriptorPoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			vkCreateDescriptorPool(this->handle, &descriptorPoolCI, nullptr, &this->descriptorPool);
		}

		// create swapchain
		this->swapchain = new VulkanSwapchain(this);
	}

	VulkanDevice::~VulkanDevice() {
		delete swapchain;
		vkDestroyDescriptorPool(handle, descriptorPool, nullptr);
		vkDestroyCommandPool(handle, commandPool, nullptr);
		vkDestroyDevice(handle, nullptr);
	}

	VkCommandBuffer VulkanDevice::createCommandBuffer(bool oneTimeUse, VkCommandBufferLevel level) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(*this, &allocInfo, &commandBuffer);

		return commandBuffer;
	}

	VkCommandBuffer VulkanDevice::beginCommandBuffer(bool oneTimeUse, VkCommandBufferLevel level) {
		VkCommandBuffer commandBuffer = createCommandBuffer(oneTimeUse, level);

		beginCommandBuffer(commandBuffer, oneTimeUse);

		return commandBuffer;
	}

	void VulkanDevice::beginCommandBuffer(VkCommandBuffer commandBuffer, bool oneTimeUse) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		if (oneTimeUse)
			beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);
	}

	void VulkanDevice::submitCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue) {
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue);

		vkFreeCommandBuffers(*this, commandPool, 1, &commandBuffer);
	}

	void VulkanDevice::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBuffer commandBuffer = beginCommandBuffer(true);

		// copy region
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		submitCommandBuffer(commandBuffer, graphicsQueue);
	}
	void VulkanDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(*this, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create vertex buffer!");
		}

		// find memory type index that is suitable
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(*this, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findSupportedMemoryType(memRequirements.memoryTypeBits, properties);

		// allocate the memory
		if (vkAllocateMemory(*this, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate vertex buffer memory!");
		}

		// copy vertex data to newly allocated buffer
		vkBindBufferMemory(*this, buffer, bufferMemory, 0);
	}
	template <class Type>
	void VulkanDevice::initDeviceLocalBuffer(std::vector<Type>& bufferData, VkBuffer& buffer, VkDeviceMemory& deviceMemory, VkBufferUsageFlagBits bufferUsageBit) {
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

	uint32_t VulkanDevice::findSupportedMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("Could not find suitable memory type!");
	}

	VkFormat VulkanDevice::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}
		throw std::runtime_error("Failed to find supported format!");
	}

	VkFormat VulkanDevice::findDepthFormat() {
		return findSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}
}