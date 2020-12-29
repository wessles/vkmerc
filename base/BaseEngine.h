#pragma once

#include <Tracy.hpp>

#include <vulkan/vulkan.h>

#include <string>
#include <stack>

#include <chrono>
#include <thread>
#include <cstdint>

#include <VulkanContext.h>

#include "util/Semaphore.h"

namespace vku {
	struct BaseEngine {
		VulkanContext* context;

		void run() {
			init();
			postInit();
			loop();
			preCleanup();
			cleanup();
		}

	protected:
		std::string windowTitle = "vkmerc";
		bool fullscreen = false;
		double framerateLimit = 120.0;
		uint32_t width = 800, height = 600;
		bool shaderHotReloadEnabled = true;

	private:
		// this flag will be tripped on the window-resize event
		bool framebufferResized = false;

		unsigned long currentFrame = 0;

		virtual void postInit() = 0;
		virtual VkCommandBuffer draw(uint32_t swapIdx) = 0;
		virtual void destroySwapchainDependents() = 0;
		virtual void buildSwapchainDependants() = 0;
		virtual void preCleanup() = 0;

		void init() {
			VulkanContextInfo info;
			info.title = windowTitle;
			info.width = width;
			info.height = height;
			info.haltOnValidationError = false;
			info.fullscreen = fullscreen;

			const auto& resizeCallback = [this](GLFWwindow* window, int width, int height) { framebufferResized = true; };
			const auto& resizeCallbackFunc = std::function<void(GLFWwindow*, int, int)>(resizeCallback);
			info.resizeCallback = resizeCallbackFunc;

			context = new VulkanContext(info);
		}

		void refreshSwapchain() {
			int width = 0, height = 0;
			glfwGetFramebufferSize(context->windowHandle, &width, &height);
			while (width == 0 || height == 0) {
				glfwGetFramebufferSize(context->windowHandle, &width, &height);
				glfwWaitEvents();
			}

			// very important for synchronization
			vkDeviceWaitIdle(*context->device);

			destroySwapchainDependents();
			context->device->swapchain->destroySwapchain();
			context->device->swapchain->createSwapchain();
			buildSwapchainDependants();
		}

		void cleanup() {
			delete context;
		}

		void loop() {
			using clock = std::chrono::steady_clock;
			const auto frame_length_target = std::chrono::microseconds((int)(1000000.0 / framerateLimit));

			// kick off shader reload thread if need be
			std::thread hotReloadThread;
			Semaphore *initiateReload, *allowContinue;
			std::atomic<bool> requestReloadFlag(false);
			std::atomic<bool> reloadThreadKill(false);
			if (shaderHotReloadEnabled) {
				initiateReload = new Semaphore();
				allowContinue = new Semaphore();
				hotReloadThread = std::thread(&vku::hotReloadCheckingThread, context->device->shaderCache, initiateReload, allowContinue, &requestReloadFlag, &reloadThreadKill);
			}

			while (true) {
				auto frame_start = clock::now();

				{
					ZoneScopedNC("Window Polling", 0xAAAAFF);
					if (glfwWindowShouldClose(context->windowHandle))
						break;
					glfwPollEvents();
				}

				if (shaderHotReloadEnabled) {
					ZoneScopedNC("Shader Hot-Reload Check", 0xFF5555);
					if (requestReloadFlag.load()) {
						requestReloadFlag.store(false);
						initiateReload->release();
						allowContinue->acquire();
					}
				}

				VulkanSwapchain& swapchain = *context->device->swapchain;
				{
					{
						ZoneScopedN("(VK) Waiting for Swapchain Flight Fences");
						vkWaitForFences(*context->device, 1, &swapchain.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
					}

					uint32_t imageIndex;
					VkResult result;
					{
						ZoneScopedN("(VK) Acquire Next Image");
						result = vkAcquireNextImageKHR(*context->device, swapchain, UINT64_MAX,
							swapchain.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
					}
					// check for out of date swap chain
					if (result == VK_ERROR_OUT_OF_DATE_KHR) {
						ZoneScopedN("Refreshing swapchain due to invalidation");
						refreshSwapchain();
						continue;
					}
					else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
						throw std::runtime_error("Failed to acquire swap chain image!");
					}

					// Check if a previous frame is using this image (i.e. there is its fence to wait on)
					if (swapchain.imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
						ZoneScopedN("(VK) Waiting for Image Flight Fences");
						vkWaitForFences(*context->device, 1, &swapchain.imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
					}
					// Mark the image as now being in use by this frame
					swapchain.imagesInFlight[imageIndex] = swapchain.inFlightFences[currentFrame];

					VkSubmitInfo submitInfo{};
					submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
					VkSemaphore waitSemaphores[] = { swapchain.imageAvailableSemaphores[currentFrame] };
					VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
					submitInfo.waitSemaphoreCount = 1;
					submitInfo.pWaitSemaphores = waitSemaphores;
					submitInfo.pWaitDstStageMask = waitStages;
					submitInfo.commandBufferCount = 1;

					VkCommandBuffer commandBuffer;
					{
						ZoneScopedN("Recording Commmand Buffer");
						commandBuffer = this->draw(imageIndex);
					}
					submitInfo.pCommandBuffers = &commandBuffer;

					VkSemaphore signalSemaphores[] = { swapchain.renderFinishedSemaphores[currentFrame] };
					submitInfo.signalSemaphoreCount = 1;
					submitInfo.pSignalSemaphores = signalSemaphores;
					{
						ZoneScopedN("Submitting Command Buffer");
						vkResetFences(*context->device, 1, &swapchain.inFlightFences[currentFrame]);
						if (vkQueueSubmit(context->device->graphicsQueue, 1, &submitInfo, swapchain.inFlightFences[currentFrame]) != VK_SUCCESS) {
							throw std::runtime_error("Failed to submit draw command buffer!");
						}
					}

					VkPresentInfoKHR presentInfo{};
					presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

					presentInfo.waitSemaphoreCount = 1;
					presentInfo.pWaitSemaphores = signalSemaphores;

					VkSwapchainKHR swapChains[] = { *context->device->swapchain };
					presentInfo.swapchainCount = 1;
					presentInfo.pSwapchains = swapChains;
					presentInfo.pImageIndices = &imageIndex;
					presentInfo.pResults = nullptr; // Optional
					{
						ZoneScopedN("Presenting Swapchains");
						result = vkQueuePresentKHR(context->device->presentQueue, &presentInfo);
					}

					if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
						framebufferResized = false;
						refreshSwapchain();
					}
					else if (result != VK_SUCCESS) {
						throw std::runtime_error("Failed to present swap chain image!");
					}

					currentFrame = (currentFrame + 1) % swapchain.swapChainLength;
				}

				// framerate limiting
				{
					ZoneScopedNC("Frame Limiting", 0xFFAAAA);
					auto target = frame_start + frame_length_target;
					auto duration = clock::now() - frame_start;
					{
						ZoneScopedNC("Spin Lock (Precise, but 100%CPU)", 0xAAFFAA);
						while (clock::now() < target) {}
					}
				}

				FrameMark;
			}
			reloadThreadKill.store(true);
			hotReloadThread.join();
			vkDeviceWaitIdle(*context->device);
		}
	};
}