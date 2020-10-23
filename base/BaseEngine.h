#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <stack>

#include <chrono>
#include <thread>
#include <cstdint>

#include <VulkanContext.h>

namespace vku {
	/*
		A class which provides an empty Vulkan application.
		It takes care of boilerplate stuff so you can limit
		your concern to Demo-specific things. It handles:
		- Vulkan initialization
		- Resizing callbacks
		- Synchronization
		- Game-loop
	*/
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
		std::string windowTitle = "Untitled";
		bool fullscreen = false;
		double framerateLimit = 120.0;
		uint32_t width = 800, height = 600;
		bool debugEnabled = true;

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
			info.debugEnabled = debugEnabled;
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
			auto next_frame = clock::now();
			const auto step = std::chrono::milliseconds((int)(1000.0 / framerateLimit));

			while (!glfwWindowShouldClose(context->windowHandle)) {
				// framerate limiting
				next_frame += step;
				std::this_thread::sleep_until(next_frame);


				glfwPollEvents();

				VulkanSwapchain& swapchain = *context->device->swapchain;
				{
					vkWaitForFences(*context->device, 1, &swapchain.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

					uint32_t imageIndex;
					VkResult result = vkAcquireNextImageKHR(*context->device, swapchain, UINT64_MAX,
						swapchain.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

					// check for out of date swap chain
					if (result == VK_ERROR_OUT_OF_DATE_KHR) {
						refreshSwapchain();
						continue;
					}
					else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
						throw std::runtime_error("Failed to acquire swap chain image!");
					}

					// Check if a previous frame is using this image (i.e. there is its fence to wait on)
					if (swapchain.imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
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

					VkCommandBuffer commandBuffer = this->draw(imageIndex);
					submitInfo.pCommandBuffers = &commandBuffer;

					VkSemaphore signalSemaphores[] = { swapchain.renderFinishedSemaphores[currentFrame] };
					submitInfo.signalSemaphoreCount = 1;
					submitInfo.pSignalSemaphores = signalSemaphores;

					vkResetFences(*context->device, 1, &swapchain.inFlightFences[currentFrame]);
					if (vkQueueSubmit(context->device->graphicsQueue, 1, &submitInfo, swapchain.inFlightFences[currentFrame]) != VK_SUCCESS) {
						throw std::runtime_error("Failed to submit draw command buffer!");
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

					result = vkQueuePresentKHR(context->device->presentQueue, &presentInfo);

					if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
						framebufferResized = false;
						refreshSwapchain();
					}
					else if (result != VK_SUCCESS) {
						throw std::runtime_error("Failed to present swap chain image!");
					}

					currentFrame = (currentFrame + 1) % swapchain.swapChainLength;
				}
			}

			vkDeviceWaitIdle(*context->device);
		}
	};
}