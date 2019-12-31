#include "Renderer.h"

#include <EASTL/array.h>
#include <EASTL/vector.h>

#include "Engine/Core/Time.h"
#include "Engine/Application/InputHandler.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Entities/SystemDispatch.h"
#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Memory/ResourceManager.h"
#include "Engine/Rendering/Pipelines/PBRPipeline.h"
#include "Engine/Rendering/Pipelines/SkyPipeline.h"
#include "Engine/Rendering/Pipelines/ImGuiPipeline.h"
#include "Engine/Rendering/Pipelines/Pipeline.h"
#include "Engine/Rendering/Shaders/ShaderCompiler.h"


#ifndef NDEBUG
inline constexpr int VALIDATION_LAYER_COUNT = 1;
inline constexpr const char* VALIDATION_LAYERS[VALIDATION_LAYER_COUNT]{
	"VK_LAYER_KHRONOS_validation"
};
#else
inline constexpr int VALIDATION_LAYER_COUNT = 0;
inline constexpr const char* const* VALIDATION_LAYERS = nullptr;
#endif

// Add debug messenger callback extension for validation layer if in debug mode
#ifndef NDEBUG
inline constexpr int INSTANCE_EXT_COUNT = 3;
inline constexpr const char* INSTANCE_EXT_NAMES[INSTANCE_EXT_COUNT]{
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
};
#else
inline constexpr int INSTANCE_EXT_COUNT = 2;
inline constexpr const char* INSTANCE_EXT_NAMES[INSTANCE_EXT_COUNT]{
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME
};
#endif

inline constexpr int DEVICE_EXT_COUNT = 1;
inline constexpr const char* DEVICE_EXT_NAMES[DEVICE_EXT_COUNT]{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

inline constexpr uint32_t SHADOWMAP_RESOLUTION = 1024;

namespace Quadbit {
	QbVkRenderer::QbVkRenderer(HINSTANCE hInstance, HWND hwnd, InputHandler* inputHandler, EntityManager* entityManager) :
	    localHandle_(hInstance), 
		windowHandle_(hwnd), 
		context_(eastl::make_unique<QbVkContext>()) {

		context_->inputHandler = inputHandler;
		context_->entityManager = entityManager;

		// REMEMBER: Order matters as some functions depend on member variables being initialized.
		CreateInstance();
#ifndef NDEBUG
		CreateDebugMessenger();
#endif
		CreateSurface();
		CreatePhysicalDevice();
		CreateLogicalDeviceAndQueues();
		CreateCommandPool();
		CreateSyncObjects();
		AllocateCommandBuffers();

		context_->allocator = eastl::make_unique<QbVkAllocator>(context_->device, context_->gpu->deviceProps.limits.bufferImageGranularity, context_->gpu->memoryProps);
		context_->shaderCompiler = eastl::make_unique<QbVkShaderCompiler>(*context_);
		context_->resourceManager = eastl::make_unique<QbVkResourceManager>(*context_);

		// Swapchain and dependent resources
		CreateSwapChain();
		CreateMultisamplingResources();
		CreateDepthResources();
		CreateShadowmapResources();
		CreateMainRenderPass();

		// Pipelines
		pbrPipeline_ = eastl::make_unique<PBRPipeline>(*context_);
		imGuiPipeline_ = eastl::make_unique<ImGuiPipeline>(*context_);
		//skyPipeline_ = eastl::make_unique<SkyPipeline>(*context_);

		// Set up camera
		context_->fallbackCamera = context_->entityManager->Create();
		context_->entityManager->AddComponent<RenderCamera>(context_->fallbackCamera, 
			Quadbit::RenderCamera(180, 55, glm::vec3(10.0f, 30.0f, -10.0f), 16.0f / 9.0f, 1000.0f));
	}

	QbVkRenderer::~QbVkRenderer() {
		// We need to start off by waiting for the GPU to be idle
		VK_CHECK(vkDeviceWaitIdle(context_->device));

		// Destroy pipelines
		pbrPipeline_.reset();
		imGuiPipeline_.reset();

		// Destroy persistent buffers and textures from the resource manager
		context_->resourceManager.reset();

		// Destroy rendering resources
		for (const auto& renderingResource : context_->renderingResources) {
			if (renderingResource.frameBuffer != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(context_->device, renderingResource.frameBuffer, nullptr);
			}
			vkFreeCommandBuffers(context_->device, context_->commandPool, 1, &renderingResource.commandBuffer);
			vkDestroySemaphore(context_->device, renderingResource.imageAvailableSemaphore, nullptr);
			vkDestroySemaphore(context_->device, renderingResource.renderFinishedSemaphore, nullptr);
			vkDestroySemaphore(context_->device, renderingResource.transferSemaphore, nullptr);
			vkDestroyFence(context_->device, renderingResource.fence, nullptr);
		}

		for (const auto& shadowmapFramebuffer : context_->shadowmapResources.framebuffers) {
			vkDestroyFramebuffer(context_->device, shadowmapFramebuffer, nullptr);
		}
		vkDestroyRenderPass(context_->device, context_->shadowmapResources.renderPass, nullptr);
		
		// Cleanup mesh and depth resources
		context_->allocator->DestroyImage(context_->multisamplingResources.msaaImage);
		vkDestroyImageView(context_->device, context_->multisamplingResources.msaaImageView, nullptr);
		context_->allocator->DestroyImage(context_->depthResources.depthImage);
		vkDestroyImageView(context_->device, context_->depthResources.imageView, nullptr);

		// Destroy swapchain and its imageviews
		for (auto&& imageView : context_->swapchain.imageViews) {
			vkDestroyImageView(context_->device, imageView, nullptr);
		}
		vkDestroySwapchainKHR(context_->device, context_->swapchain.swapchain, nullptr);

		// Destroy command pool
		vkDestroyCommandPool(context_->device, context_->commandPool, nullptr);

		// Destroy render passes
		vkDestroyRenderPass(context_->device, context_->mainRenderPass, nullptr);

		// Empty garbage and destroy allocator
		context_->allocator->EmptyGarbage();
		context_->allocator.reset();

		// Destroy device
		vkDestroyDevice(context_->device, nullptr);

		// Destroy surface
		vkDestroySurfaceKHR(instance_, context_->surface, nullptr);

#ifndef NDEBUG
		auto DestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
		if (DestroyDebugUtilsMessengerEXT != nullptr) {
			DestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
		}
#endif

		// Destroy instance
		vkDestroyInstance(instance_, nullptr);
	}

	void QbVkRenderer::DrawFrame() {
		// Return early if we cannot render (if swapchain is being recreated)
		if (!canRender_) return;

		// Clean up resources that are due for removal
		context_->resourceManager->DestroyUnusedResources();

		// Before we render, we transfer all queued data to buffers on the GPU
		// we make sure we only wait on the transfer if there was any data transferred
		const bool transferActive = context_->resourceManager->TransferQueuedDataToGPU(context_->resourceIndex);

		RenderingResources& currentRenderingResources = context_->renderingResources[context_->resourceIndex];
		// First we need to wait for the frame to be finished then rebuild command buffer and go
		VK_CHECK(vkWaitForFences(context_->device, 1, &currentRenderingResources.fence, VK_TRUE, UINT64_MAX));
		VK_CHECK(vkResetFences(context_->device, 1, &currentRenderingResources.fence));

		// Then we will acquire an image from the swapchain
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(context_->device, context_->swapchain.swapchain, UINT64_MAX,
			currentRenderingResources.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			Rebuild();
			return;
		}
		VK_CHECK(result);

		// Prepare the frame for rendering
		PrepareFrame(context_->resourceIndex, currentRenderingResources.commandBuffer, currentRenderingResources.frameBuffer, context_->swapchain.imageViews[imageIndex]);

		// We will now submit the command buffer
		VkSubmitInfo submitInfo = VkUtils::Init::SubmitInfo();
		// Here we specify the semaphores to wait for and the stage in which to wait
		// The semaphores and stages are matched to eachother by index
		auto waitSemaphores = transferActive ? 
			eastl::vector{ currentRenderingResources.imageAvailableSemaphore, currentRenderingResources.transferSemaphore } :
			eastl::vector{ currentRenderingResources.imageAvailableSemaphore };
		auto waitStages = transferActive ? 
			eastl::vector<VkPipelineStageFlags> { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT} :
			eastl::vector<VkPipelineStageFlags> { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = transferActive ? 2 : 1;
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		submitInfo.pWaitDstStageMask = waitStages.data();
		// Here we specify the appropriate command buffer for the swapchain image received earlier
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &currentRenderingResources.commandBuffer;
		// Here we will specify which semaphores to signal once execution completes
		VkSemaphore signalSemaphores[] = { currentRenderingResources.renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		// Finally we submit the command buffer to the graphics queue, we will also reset the fence to be ready for next frame
		VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submitInfo, currentRenderingResources.fence));

		// We can now go ahead and submit the result to the present queue
		VkPresentInfoKHR presentInfo = VkUtils::Init::PresentInfoKHR();

		// Wait for the render to finish before presenting
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapchains[] = { context_->swapchain.swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(context_->presentQueue, &presentInfo);
		if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				Rebuild();
				return;
			}
		}
		VK_CHECK(result);

		// Increment (and mod) resource index to get the next resource
		context_->resourceIndex = (context_->resourceIndex + 1) % MAX_FRAMES_IN_FLIGHT;

		// Rebuild shaders if requested
		if (context_->inputHandler->keyState_[0x10] && context_->inputHandler->controlKeysPressed_.enter) {
			QB_LOG_INFO("Recompiling shaders and rebuilding pipelines!\n");
			vkDeviceWaitIdle(context_->device);
			context_->resourceManager->RebuildPipelines();
		}
	}

#ifndef NDEBUG
	// Debug messenger creation and callback function
	VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {

		QB_LOG_INFO("Vulkan Validation Layer: %s\n", pCallbackData->pMessage);
		return VK_FALSE;
	}

	void QbVkRenderer::CreateDebugMessenger() {
		VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerInfo{};
		debugUtilsMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugUtilsMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugUtilsMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugUtilsMessengerInfo.pfnUserCallback = DebugMessengerCallback;

		// Because we are using an extension function we need to load it ourselves.
		auto CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
		if (CreateDebugUtilsMessengerEXT != nullptr) {
			VK_CHECK(CreateDebugUtilsMessengerEXT(instance_, &debugUtilsMessengerInfo, nullptr, &debugMessenger_));
		}
	}
#endif

	void QbVkRenderer::CreateInstance() {
		// Fill out stuff for instance creation
		VkApplicationInfo appInfo = VkUtils::Init::ApplicationInfo();
		appInfo.pApplicationName = "Quadbit";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Quadbit_Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_1;

		VkInstanceCreateInfo instanceInfo = VkUtils::Init::InstanceCreateInfo();
		instanceInfo.pApplicationInfo = &appInfo;

		instanceInfo.enabledLayerCount = VALIDATION_LAYER_COUNT;
		instanceInfo.ppEnabledLayerNames = VALIDATION_LAYERS;

		instanceInfo.enabledExtensionCount = INSTANCE_EXT_COUNT;
		instanceInfo.ppEnabledExtensionNames = INSTANCE_EXT_NAMES;

		// Create instance
		VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance_));
	}

	void QbVkRenderer::CreateSurface() {
		VkWin32SurfaceCreateInfoKHR surfaceInfo = VkUtils::Init::Win32SurfaceCreateInfoKHR();
		surfaceInfo.hinstance = localHandle_;
		surfaceInfo.hwnd = windowHandle_;

		VK_CHECK(vkCreateWin32SurfaceKHR(instance_, &surfaceInfo, nullptr, &context_->surface));
	}

	void QbVkRenderer::CreatePhysicalDevice() {
		// Get device count
		uint32_t deviceCount = 0;
		VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr));
		VK_VALIDATE(deviceCount > 0, "VkEnumeratePhysicalDevices returned zero devices.");

		// Allocate array of physical devices
		eastl::vector<VkPhysicalDevice> physicalDevices(deviceCount);

		// Fill array of physical devices
		VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, physicalDevices.data()));

		// Now find a suitable GPU
		if (!VkUtils::FindSuitableGPU(*context_, physicalDevices)) {
			QB_LOG_ERROR("No suitable GPU's found on machine.\n");
		}
	}

	void QbVkRenderer::CreateLogicalDeviceAndQueues() {
		// We'll start by filling out device queue information
		eastl::vector<int> queueIndices;
		queueIndices.push_back(context_->gpu->graphicsFamilyIdx);
		if (context_->gpu->graphicsFamilyIdx != context_->gpu->presentFamilyIdx) {
			queueIndices.push_back(context_->gpu->presentFamilyIdx);
		}
		if (context_->gpu->graphicsFamilyIdx != context_->gpu->computeFamilyIdx && context_->gpu->presentFamilyIdx != context_->gpu->computeFamilyIdx) {
			queueIndices.push_back(context_->gpu->computeFamilyIdx);
		}

		eastl::vector<VkDeviceQueueCreateInfo> deviceQueueInfo;

		const float priority = 1.0f;
		for (auto i = 0; i < queueIndices.size(); i++) {
			VkDeviceQueueCreateInfo queueInfo = VkUtils::Init::DeviceQueueCreateInfo();
			queueInfo.queueFamilyIndex = queueIndices[i];
			queueInfo.queueCount = 1;

			// Priority doesn't matter (apparently)
			queueInfo.pQueuePriorities = &priority;

			deviceQueueInfo.push_back(queueInfo);
		}

		// We'll now specify some device features that we need (Add more as necessary)
		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.imageCubeArray = VK_TRUE;
		deviceFeatures.fillModeNonSolid = VK_TRUE;
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.depthClamp = VK_TRUE;
		deviceFeatures.depthBiasClamp = VK_TRUE;
		//deviceFeatures.sampleRateShading = VK_TRUE;

		// Now we will fill out the actual information for device creation
		VkDeviceCreateInfo deviceInfo = VkUtils::Init::DeviceCreateInfo();
		deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueInfo.size());
		deviceInfo.pQueueCreateInfos = deviceQueueInfo.data();
		deviceInfo.pEnabledFeatures = &deviceFeatures;

		deviceInfo.enabledLayerCount = VALIDATION_LAYER_COUNT;
		deviceInfo.ppEnabledLayerNames = VALIDATION_LAYERS;

		deviceInfo.enabledExtensionCount = DEVICE_EXT_COUNT;
		deviceInfo.ppEnabledExtensionNames = DEVICE_EXT_NAMES;

		// Create the logical device
		VK_CHECK(vkCreateDevice(context_->gpu->physicalDevice, &deviceInfo, nullptr, &context_->device));

		// Now get the queues from the logical device
		vkGetDeviceQueue(context_->device, context_->gpu->graphicsFamilyIdx, 0, &context_->graphicsQueue);
		vkGetDeviceQueue(context_->device, context_->gpu->presentFamilyIdx, 0, &context_->presentQueue);
		vkGetDeviceQueue(context_->device, context_->gpu->computeFamilyIdx, 0, &context_->computeQueue);
	}

	void QbVkRenderer::CreateCommandPool() {
		// Create command pool to avoid unnecessary allocations during rendering
		VkCommandPoolCreateInfo commandPoolInfo = VkUtils::Init::CommandPoolCreateInfo();

		// Implicitly reset command pool when vkBeginCommandBuffer is called and that the command buffers will be short-lived
		commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

		// Specify that the command buffers will be built for the graphics queue
		commandPoolInfo.queueFamilyIndex = context_->gpu->graphicsFamilyIdx;

		VK_CHECK(vkCreateCommandPool(context_->device, &commandPoolInfo, nullptr, &context_->commandPool));
	}

	void QbVkRenderer::CreateSyncObjects() {
		// Create semaphores and fences for synchronization
		VkSemaphoreCreateInfo sempahoreInfo = VkUtils::Init::SemaphoreCreateInfo();
		VkFenceCreateInfo fenceInfo = VkUtils::Init::FenceCreateInfo();
		// We will create the fences with signaled bit set, so that the first call to vkWaitForFences doesn't hang
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VK_CHECK(vkCreateSemaphore(context_->device, &sempahoreInfo, nullptr, &context_->renderingResources[i].imageAvailableSemaphore));
			VK_CHECK(vkCreateSemaphore(context_->device, &sempahoreInfo, nullptr, &context_->renderingResources[i].renderFinishedSemaphore));
			VK_CHECK(vkCreateSemaphore(context_->device, &sempahoreInfo, nullptr, &context_->renderingResources[i].transferSemaphore));
			VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &context_->renderingResources[i].fence));
		}
	}

	void QbVkRenderer::AllocateCommandBuffers() {
		// Allocate the command buffers required for the rendering resources
		for (auto&& renderingResource : context_->renderingResources) {
			renderingResource.commandBuffer = VkUtils::CreatePersistentCommandBuffer(*context_);
		}
	}

	void QbVkRenderer::CreateMultisamplingResources() {
		VkFormat colourFormat = context_->swapchain.imageFormat;

		VkImageCreateInfo imageInfo = VkUtils::Init::ImageCreateInfo(context_->swapchain.extent.width, context_->swapchain.extent.height, colourFormat,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, context_->multisamplingResources.msaaSamples);

		context_->allocator->CreateImage(context_->multisamplingResources.msaaImage, imageInfo, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		context_->multisamplingResources.msaaImageView = VkUtils::CreateImageView(*context_, context_->multisamplingResources.msaaImage.imgHandle, colourFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		// Transition image layout with a temporary command buffer
		auto commandBuffer = VkUtils::CreateSingleTimeCommandBuffer(*context_);
		VkUtils::TransitionImageLayout(commandBuffer, context_->multisamplingResources.msaaImage.imgHandle,
			VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		VkUtils::FlushCommandBuffer(*context_, commandBuffer);
	}

	void QbVkRenderer::CreateDepthResources() {
		VkFormat depthFormat = VkUtils::FindDepthFormat(*context_);

		VkImageCreateInfo imageInfo = VkUtils::Init::ImageCreateInfo(context_->swapchain.extent.width, context_->swapchain.extent.height, depthFormat,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, context_->multisamplingResources.msaaSamples);;

		context_->allocator->CreateImage(context_->depthResources.depthImage, imageInfo, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		context_->depthResources.imageView = VkUtils::CreateImageView(*context_, context_->depthResources.depthImage.imgHandle, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

		// Transition depth image
		VkImageAspectFlags aspectFlags = VkUtils::HasStencilComponent(depthFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
		auto commandBuffer = VkUtils::CreateSingleTimeCommandBuffer(*context_);
		VkUtils::TransitionImageLayout(commandBuffer, context_->depthResources.depthImage.imgHandle, aspectFlags, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
		VkUtils::FlushCommandBuffer(*context_, commandBuffer);
	}

	void QbVkRenderer::CreateShadowmapResources() {
		// Prepare the shadowmap texture
		VkImageCreateInfo imageInfo = VkUtils::Init::ImageCreateInfo(SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION, VkUtils::FindDepthFormat(*context_),
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

		VkSamplerCreateInfo samplerInfo = VkUtils::Init::SamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 
			VK_FALSE, 1.0f, VK_COMPARE_OP_LESS, VK_SAMPLER_MIPMAP_MODE_LINEAR);
		samplerInfo.compareEnable = VK_TRUE;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		context_->shadowmapResources.texture = context_->resourceManager->CreateTexture(&imageInfo, VK_IMAGE_ASPECT_DEPTH_BIT, 
			VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL, &samplerInfo);

		// Prepare the offscreen renderpass
		VkAttachmentDescription attachmentDescription{};
		attachmentDescription.format = VkUtils::FindDepthFormat(*context_);
		attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		VkAttachmentReference depthReference{};
		depthReference.attachment = 0;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription{};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 0;
		subpassDescription.pDepthStencilAttachment = &depthReference;

		eastl::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = VkUtils::Init::RenderPassCreateInfo();
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &attachmentDescription;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();

		VK_CHECK(vkCreateRenderPass(context_->device, &renderPassInfo, nullptr, &context_->shadowmapResources.renderPass));

		QbVkPipelineDescription pipelineDescription{};
		pipelineDescription.colourBlending = QbVkPipelineColourBlending::QBVK_COLOURBLENDING_NOATTACHMENT;
		pipelineDescription.depth = QbVkPipelineDepth::QBVK_PIPELINE_DEPTH_ENABLE;
		pipelineDescription.dynamicState = QbVkPipelineDynamicState::QBVK_DYNAMICSTATE_DEPTHBIAS;
		pipelineDescription.enableMSAA = false;
		pipelineDescription.rasterization = QbVkPipelineRasterization::QBVK_PIPELINE_RASTERIZATION_SHADOWMAP;
		pipelineDescription.viewportExtents = { SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION };

		context_->shadowmapResources.pipeline = context_->resourceManager->CreateGraphicsPipeline("Assets/Quadbit/Shaders/shadowmap.vert", "main", 
			nullptr, nullptr, pipelineDescription, context_->shadowmapResources.renderPass);
	}

	void QbVkRenderer::CreateSwapChain() {
		canRender_ = false;

		VkSwapchainKHR oldSwapchain = context_->swapchain.swapchain;

		// First pick a surface format, present mode and the surface extents for the swap chain
		VkSurfaceFormatKHR surfaceFormat = VkUtils::ChooseSurfaceFormat(context_->gpu->surfaceFormats);
		VkPresentModeKHR presentMode = VkUtils::ChoosePresentMode(context_->gpu->presentModes);
		VkExtent2D extent = VkUtils::ChooseSurfaceExtent(context_->gpu->surfaceCapabilities, windowHandle_);

		// Pick a minimum count of images in the swap chain
		uint32_t desiredSwapchainImageCount = context_->gpu->surfaceCapabilities.minImageCount + 1;
		if (context_->gpu->surfaceCapabilities.maxImageCount > 0 && (desiredSwapchainImageCount > context_->gpu->surfaceCapabilities.maxImageCount)) {
			desiredSwapchainImageCount = context_->gpu->surfaceCapabilities.maxImageCount;
		}
		// Prefer a non-rotated transform if possible
		VkSurfaceTransformFlagsKHR preTransform;
		if (context_->gpu->surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else {
			preTransform = context_->gpu->surfaceCapabilities.currentTransform;
		}

		// Time to fill out the swapchain create struct
		VkSwapchainCreateInfoKHR swapchainInfo = VkUtils::Init::SwapchainCreateInfoKHR();
		swapchainInfo.surface = context_->surface;
		swapchainInfo.minImageCount = desiredSwapchainImageCount;
		swapchainInfo.imageFormat = surfaceFormat.format;
		swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
		swapchainInfo.imageExtent = extent;
		swapchainInfo.imageArrayLayers = 1;
		// Here we specify the image usage with some bit flags
		swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainInfo.preTransform = static_cast<VkSurfaceTransformFlagBitsKHR>(preTransform);
		// Opaque bit is set as we don't need any transparency / blending
		swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainInfo.presentMode = presentMode;
		swapchainInfo.oldSwapchain = oldSwapchain;
		// Allow vulkan to discard operations outside of renderable space
		swapchainInfo.clipped = VK_TRUE;

		// We will enable transfer source/destination for swapchain images if its supported
		if (context_->gpu->surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
			swapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (context_->gpu->surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
			swapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		// We need to check if the graphics and present family indices match,
		// in which case the queue can have exclusive access to the images which offers the best performance
		if (context_->gpu->graphicsFamilyIdx == context_->gpu->presentFamilyIdx) {
			swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
		// Otherwise the image has to be usable across multiple queue families
		else {
			uint32_t queueFamilyIndices[] = {
				static_cast<uint32_t>(context_->gpu->graphicsFamilyIdx),
				static_cast<uint32_t>(context_->gpu->presentFamilyIdx)
			};
			swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainInfo.queueFamilyIndexCount = 2;
			swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
		}

		// Create the swapchain
		VK_CHECK(vkCreateSwapchainKHR(context_->device, &swapchainInfo, nullptr, &context_->swapchain.swapchain));

		// Save a few properties of the swapchain in the render context for later use
		context_->swapchain.imageFormat = surfaceFormat.format;
		context_->swapchain.presentMode = presentMode;
		context_->swapchain.extent = extent;

		// Delete the old swapchain if it exists, along with its imageviews.
		if (oldSwapchain != VK_NULL_HANDLE) {
			for (auto i = 0; i < context_->swapchain.imageViews.size(); i++) {
				vkDestroyImageView(context_->device, context_->swapchain.imageViews[i], nullptr);
			}
			vkDestroySwapchainKHR(context_->device, oldSwapchain, nullptr);
		}

		// Then we will retrieve the new swapchain images
		uint32_t imageCount = 0;
		VK_CHECK(vkGetSwapchainImagesKHR(context_->device, context_->swapchain.swapchain, &imageCount, 0));
		context_->swapchain.images.resize(imageCount);
		VK_CHECK(vkGetSwapchainImagesKHR(context_->device, context_->swapchain.swapchain, &imageCount, context_->swapchain.images.data()));

		// And finally we will create the image views that act as interfaces for the images
		context_->swapchain.imageViews.resize(context_->swapchain.images.size());
		for (auto i = 0; i < context_->swapchain.images.size(); i++) {
			context_->swapchain.imageViews[i] = VkUtils::CreateImageView(*context_, context_->swapchain.images[i], context_->swapchain.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		}

		canRender_ = true;
	}

	void QbVkRenderer::RecreateSwapchain() {
		// Wait for the device to be idle
		vkDeviceWaitIdle(context_->device);

		// Destroy msaa resources
		context_->allocator->DestroyImage(context_->multisamplingResources.msaaImage);
		vkDestroyImageView(context_->device, context_->multisamplingResources.msaaImageView, nullptr);

		// Destroy depth resources
		context_->allocator->DestroyImage(context_->depthResources.depthImage);
		vkDestroyImageView(context_->device, context_->depthResources.imageView, nullptr);

		// Recreate swapchain and dependent resources
		CreateSwapChain();
		CreateMultisamplingResources();
		CreateDepthResources();

		// Requery the surface capabilities
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_->gpu->physicalDevice, context_->surface, &context_->gpu->surfaceCapabilities));
		vkDeviceWaitIdle(context_->device);
	}

	void QbVkRenderer::CreateMainRenderPass() {
		// The render pass manages framebuffers and their contents

		// This part includes the colour attachment
		VkAttachmentDescription colourAttachment{};
		colourAttachment.format = context_->swapchain.imageFormat;
		colourAttachment.samples = context_->multisamplingResources.msaaSamples;

		// Contents are cleared to a constant at the start of the render pass (color and depth data)
		colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		// Rendered contents will be stored in memory and can be read later on (color and depth data)
		colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		// As we aren't doing any stencil testing we don't care for now
		colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		// We don't care which layout the contents of the image has at start
		colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		// We do however need the final layout to be ready for present using the swapchain
		colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Now we need an attachment reference
		VkAttachmentReference colourAttachmentRef{};
		// Since we only have one attachment description, its index is 0
		colourAttachmentRef.attachment = 0;
		colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// NOTICE: the index of the attachment is directly referenced in the shader --> layout(location = 0) out vec4 outColor

		// This part includes the depth attachment (similar values as above so refer there for explanations)
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = VkUtils::FindDepthFormat(*context_);
		depthAttachment.samples = context_->multisamplingResources.msaaSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// We need an attachment to resolve the multisampled image
		VkAttachmentDescription colourAttachmentResolve{};
		colourAttachmentResolve.format = context_->swapchain.imageFormat;
		colourAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colourAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colourAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colourAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colourAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colourAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colourAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colourAttachmentResolveRef{};
		colourAttachmentResolveRef.attachment = 2;
		colourAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// We can now create the render subpass
		// TODO: External subpasses add unnecessary complexity, redo renderpass system
		VkSubpassDescription subpassDesc{};
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc.colorAttachmentCount = 1;
		subpassDesc.pColorAttachments = &colourAttachmentRef;
		subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;
		subpassDesc.pResolveAttachments = &colourAttachmentResolveRef;

		// We need to add a subpass dependency here to make the render pass 
		// wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage
		VkSubpassDependency dependency{};
		// Source is the implicit subpass before or after the render pass
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		// 0 is the index of our subpass
		dependency.dstSubpass = 0;

		// Here we specify that we want to wait for the color attachment output stage
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;

		// Here we specify which stage to wait on and that we need both read and write access
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// Now we can create the actual renderpass
		eastl::array<VkAttachmentDescription, 3> attachments =
		{ colourAttachment, depthAttachment, colourAttachmentResolve };
		VkRenderPassCreateInfo renderPassInfo = VkUtils::Init::RenderPassCreateInfo();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDesc;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK(vkCreateRenderPass(context_->device, &renderPassInfo, nullptr, &context_->mainRenderPass));
	}

	void QbVkRenderer::ImGuiUpdateContent() {

		imGuiPipeline_->ImGuiDrawState();
		context_->entityManager->systemDispatch_->ImGuiDrawState();
		context_->allocator->ImGuiDrawState();

		// Any ImGui draw commands before this call will be rendered to the screen
		// this also means user-code as Game->Simulate() is done before rendering 
		// each frame
		ImGui::Render();
	}

	void QbVkRenderer::PrepareFrame(uint32_t resourceIndex, VkCommandBuffer commandBuffer, VkFramebuffer& framebuffer, VkImageView imageView) {
		// Let the allocator cleanup stuff before preparing the frame
		context_->allocator->EmptyGarbage();

		eastl::array<VkClearValue, 2> clearValues;
		clearValues[0].color = { {0.2f, 0.2f, 0.2f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkCommandBufferBeginInfo commandBufferInfo = VkUtils::Init::CommandBufferBeginInfo();
		commandBufferInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferInfo));

		auto& shadowmapResources = context_->shadowmapResources;

		VkUtils::CreateFrameBuffer(*context_, SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION,
			shadowmapResources.framebuffers[resourceIndex], shadowmapResources.renderPass,
			{ context_->resourceManager->textures_[shadowmapResources.texture].descriptor.imageView });

		VkRenderPassBeginInfo renderPassInfo = VkUtils::Init::RenderPassBeginInfo();
		renderPassInfo.renderPass = context_->shadowmapResources.renderPass;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION };

		// This specifies the clear values to use for the VK_ATTACHMENT_LOAD_OP_CLEAR operation (in the color and depth attachments)
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearValues[1];
		renderPassInfo.framebuffer = shadowmapResources.framebuffers[resourceIndex];

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		

		// Depth bias parameters to reduce shadow artifacts
		constexpr float depthBiasConstant = 1.25f;
		constexpr float depthBiasSlope = 1.75f;
		constexpr float depthBiasClamp = 0.0078125f;

		vkCmdSetDepthBias(commandBuffer, depthBiasConstant, depthBiasClamp, depthBiasSlope);

		auto& pipeline = context_->resourceManager->pipelines_[shadowmapResources.pipeline];
		pipeline->Bind(commandBuffer);

		pbrPipeline_->DrawShadows(resourceIndex, commandBuffer);

		vkCmdEndRenderPass(commandBuffer);

		// Create the frame buffer
		VkUtils::CreateFrameBuffer(*context_, context_->swapchain.extent.width, context_->swapchain.extent.height, 
			framebuffer, context_->mainRenderPass, { context_->multisamplingResources.msaaImageView, context_->depthResources.imageView, imageView });

		renderPassInfo = VkUtils::Init::RenderPassBeginInfo();
		renderPassInfo.renderPass = context_->mainRenderPass;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = context_->swapchain.extent;

		// This specifies the clear values to use for the VK_ATTACHMENT_LOAD_OP_CLEAR operation (in the color and depth attachments)
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues.data();
		renderPassInfo.framebuffer = framebuffer;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		//skyPipeline_->DrawFrame(commandBuffer);
		pbrPipeline_->DrawFrame(resourceIndex, commandBuffer);

		ImGuiUpdateContent();
		imGuiPipeline_->DrawFrame(resourceIndex, commandBuffer);

		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK(vkEndCommandBuffer(commandBuffer));
	}
	
	void QbVkRenderer::Rebuild() {
		RecreateSwapchain();
		context_->resourceManager->RebuildPipelines();

		context_->entityManager->AddComponent<CameraUpdateAspectRatioTag>(context_->fallbackCamera);
		if (context_->userCamera != NULL_ENTITY && context_->entityManager->IsValid(context_->userCamera)) {
			context_->entityManager->AddComponent<CameraUpdateAspectRatioTag>(context_->userCamera);
		}
	}
}