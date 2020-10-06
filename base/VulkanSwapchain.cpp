#include "VulkanSwapchain.h"

#include <algorithm>
#include <stdexcept>

#include <vulkan/vulkan.h>

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"

namespace vku {
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
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VulkanDevice* device) {
		if (capabilities.currentExtent.width != UINT32_MAX) {
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(device->context->windowHandle, &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	VulkanSwapchain::VulkanSwapchain(VulkanDevice* device) {
		this->device = device;
		createSwapchain();
		createSyncObjects();
	}

	VulkanSwapchain::~VulkanSwapchain() {
		destroySyncObjects();
		destroySwapchain();
	}

	void VulkanSwapchain::createSwapchain() {
		device->swapchainSupportInfo = device->queryDeviceSwapchainSupportInformation(device->physicalDevice);
		auto swapchainSupport = device->swapchainSupportInfo;

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities, device);

		// image count should be minswapChainLength+1, but also not exceeding the maxswapChainLength
		this->swapChainLength = swapchainSupport.capabilities.minImageCount + 1;
		if (swapchainSupport.capabilities.minImageCount > 0 && swapChainLength > swapchainSupport.capabilities.minImageCount) {
			swapChainLength = swapchainSupport.capabilities.minImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = device->context->surface;

		createInfo.minImageCount = swapChainLength;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if (device->supportInfo.graphicsFamily != device->supportInfo.presentFamily) {
			uint32_t queueIndices[] = { device->supportInfo.graphicsFamily.value(), device->supportInfo.presentFamily.value() };
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueIndices;
		}
		else {
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
		bool result = vkCreateSwapchainKHR(*device, &createInfo, nullptr, &swapchain);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create swap chain!");
		}

		// retrieval...
		vkGetSwapchainImagesKHR(*device, swapchain, &swapChainLength, nullptr);
		swapChainImages.resize(swapChainLength);
		vkGetSwapchainImagesKHR(*device, swapchain, &swapChainLength, swapChainImages.data());

		screenFormat = surfaceFormat.format;
		depthFormat = device->findDepthFormat();
		swapChainExtent = extent;

		// create image views
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			VulkanImageViewInfo viewInfo{};
			viewInfo.image = swapChainImages[i];
			viewInfo.format = screenFormat;
			swapChainImageViews[i] = new VulkanImageView(device, viewInfo);
		}
	}

	void VulkanSwapchain::destroySwapchain() {
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			delete swapChainImageViews[i];
		}
		swapChainImageViews.clear();

		vkDestroySwapchainKHR(*device, swapchain, nullptr);
	}

	void VulkanSwapchain::createSyncObjects() {
		imageAvailableSemaphores.resize(this->swapChainLength);
		renderFinishedSemaphores.resize(this->swapChainLength);
		inFlightFences.resize(this->swapChainLength);
		imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < this->swapChainLength; i++) {
			if (vkCreateSemaphore(*device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
				|| vkCreateSemaphore(*device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS
				|| vkCreateFence(*device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

				throw std::runtime_error("Failed to create synchronization objects for a frame!");
			}
		}
	}

	void VulkanSwapchain::destroySyncObjects() {
		for (size_t i = 0; i < this->swapChainLength; i++) {
			vkDestroySemaphore(*device, imageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(*device, renderFinishedSemaphores[i], nullptr);
			vkDestroyFence(*device, inFlightFences[i], nullptr);
		}
	}
}