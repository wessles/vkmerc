#pragma once

#include <functional>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "vku.h"
#include "vku_swap.hpp"

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
	void init(bool debug, const std::string& windowName, VkExtent2D windowSize, std::function<void(GLFWwindow*, int, int)> resizeCallback) {
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