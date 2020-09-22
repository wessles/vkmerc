#pragma once

#include <vulkan/vulkan.h>

#include "vku_image.hpp"

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