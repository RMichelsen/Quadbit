#include <PCH.h>
// Necessary define for stb_image library and tiny obj loader
#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include "QbVkRenderer.h"

#include "Common/QbVkUtils.h"
#include "../Core/InputHandler.h"
#include "../Core/Time.h"
#include "../Global/ImGuiState.h"

namespace Quadbit {
	QbVkRenderer::QbVkRenderer(HINSTANCE hInstance, HWND hwnd) : 
		localHandle_(hInstance), 
		windowHandle_(hwnd),
		entityManager_(EntityManager::GetOrCreate()) {
		// REMEMBER: Order matters as some functions depend on member variables being initialized.
		CreateInstance();
#ifdef QBDEBUG
		CreateDebugMessenger();
#endif
		CreateSurface();
		CreatePhysicalDevice();
		CreateLogicalDeviceAndQueues();

		context_->allocator = std::make_unique<QbVkAllocator>(context_->device, context_->gpu->deviceProps.limits.bufferImageGranularity, context_->gpu->memoryProps);

		CreateCommandPool();
		CreateSyncObjects();
		AllocateCommandBuffers();

		// Swapchain and dependent resources
		CreateSwapChain();
		CreateMultisamplingResources();
		CreateDepthResources();
		CreateMainRenderPass();

		// Mesh Pipeline
		meshPipeline_ = std::make_unique<MeshPipeline>(context_);

		// ImGui Pipeline (Debug build only)
		imGuiPipeline_ = std::make_unique<ImGuiPipeline>(context_);

		// Compute Pipeline
		computePipeline_ = std::make_unique<ComputePipeline>(context_);
	}

	QbVkRenderer::~QbVkRenderer() {
		// We need to start off by waiting for the GPU to be idle
		VK_CHECK(vkDeviceWaitIdle(context_->device));

#ifdef QBDEBUG
		// Destroy debug messenger
		auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
		destroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
#endif

		// Destroy rendering resources
		for(const auto& renderingResource : context_->renderingResources) {
			if(renderingResource.framebuffer != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(context_->device, renderingResource.framebuffer, nullptr);
			}
			vkFreeCommandBuffers(context_->device, context_->commandPool, 1, &renderingResource.commandbuffer);
			vkDestroySemaphore(context_->device, renderingResource.imageAvailableSemaphore, nullptr);
			vkDestroySemaphore(context_->device, renderingResource.renderFinishedSemaphore, nullptr);
			vkDestroyFence(context_->device, renderingResource.fence, nullptr);
		}

		// Cleanup mesh and depth resources
		context_->allocator->DestroyImage(context_->multisamplingResources.msaaImage);
		vkDestroyImageView(context_->device, context_->multisamplingResources.msaaImageView, nullptr);
		context_->allocator->DestroyImage(context_->depthResources.depthImage);
		vkDestroyImageView(context_->device, context_->depthResources.imageView, nullptr);

		// Destroy swapchain
		vkDestroySwapchainKHR(context_->device, context_->swapchain.swapchain, nullptr);

		// Destroy command pool
		vkDestroyCommandPool(context_->device, context_->commandPool, nullptr);

		// Destroy render passes
		vkDestroyRenderPass(context_->device, context_->mainRenderPass, nullptr);

		// Destroy pipelines
		meshPipeline_.reset();
		imGuiPipeline_.reset();
		computePipeline_.reset();

		// Destroy allocator
		context_->allocator.reset();

		// Destroy device
		vkDestroyDevice(context_->device, nullptr);

		// Destroy surface
		vkDestroySurfaceKHR(instance_, context_->surface, nullptr);

		// Destroy instance
		vkDestroyInstance(instance_, nullptr);
	}

	float QbVkRenderer::GetAspectRatio() {
		return static_cast<float>(context_->swapchain.extent.width) / static_cast<float>(context_->swapchain.extent.height);
	}

	void QbVkRenderer::RegisterCamera(Entity entity) {
		if(!entity.HasComponent<RenderCamera>()) {
			QB_LOG_ERROR("Cannot register camera: Entity must have the Quadbit::RenderCamera component\n");
			return;
		}
		meshPipeline_->userCamera_ = entity;
	}

	RenderMeshComponent QbVkRenderer::CreateMesh(const char* objPath, std::vector<QbVkVertexInputAttribute> vertexModel, std::shared_ptr<QbVkRenderMeshInstance> externalInstance,
		int pushConstantStride) {
		QbVkModel model = VkUtils::LoadModel(objPath, vertexModel);

		return RenderMeshComponent{
			meshPipeline_->CreateVertexBuffer(model.vertices.data(), model.vertexStride, static_cast<uint32_t>(model.vertices.size())),
			meshPipeline_->CreateIndexBuffer(model.indices),
			static_cast<uint32_t>(model.indices.size()),
			std::array<float, 32>(),
			pushConstantStride,
			externalInstance
		};
	}

	RenderTexturedObjectComponent QbVkRenderer::CreateObject(const char* objPath, const char* texturePath, VkFormat textureFormat) {
		return meshPipeline_->CreateObject(objPath, texturePath, textureFormat);
	}

	void QbVkRenderer::LoadEnvironmentMap(const char* environmentTexture, VkFormat textureFormat) {
		meshPipeline_->LoadEnvironmentMap(environmentTexture, textureFormat);
	}

	Entity QbVkRenderer::GetActiveCamera() {
		return meshPipeline_->GetActiveCamera();
	}

	VkDescriptorImageInfo QbVkRenderer::GetEnvironmentMapDescriptor() {
		return meshPipeline_->GetEnvironmentMapDescriptor();
	}

	QbVkTexture QbVkRenderer::LoadCubemap(const char* imagePath, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout, VkImageAspectFlags imageAspectFlags, QbVkMemoryUsage memoryUsage) {
		return VkUtils::LoadCubemap(context_, imagePath, imageFormat, imageTiling, imageUsage, imageLayout, imageAspectFlags, memoryUsage);
	}

	QbVkTexture QbVkRenderer::LoadTexture(const char* imagePath, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
		VkImageAspectFlags imageAspectFlags, QbVkMemoryUsage memoryUsage, VkSamplerCreateInfo* samplerCreateInfo, VkSampleCountFlagBits numSamples) {
		return VkUtils::LoadTexture(context_, imagePath, imageFormat, imageTiling, imageUsage, imageLayout, imageAspectFlags, memoryUsage, samplerCreateInfo, numSamples);
	}

	void QbVkRenderer::CreateTexture(QbVkTexture& texture, uint32_t width, uint32_t height, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, 
		VkImageLayout imageLayout, VkImageAspectFlags imageAspectFlags, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage, QbVkMemoryUsage memoryUsage, 
		VkSamplerCreateInfo* samplerCreateInfo, VkSampleCountFlagBits numSamples) {
		VkUtils::CreateTexture(context_, texture, width, height, imageFormat, imageTiling, imageUsage, imageLayout, imageAspectFlags, srcStage, dstStage, memoryUsage, samplerCreateInfo, numSamples);
	}

	void QbVkRenderer::CreateGPUBuffer(QbVkBuffer& buffer, VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage) {
		VkUtils::CreateGPUBuffer(context_, buffer, size, bufferUsage, memoryUsage);
	}

	void QbVkRenderer::TransferDataToGPUBuffer(QbVkBuffer& buffer, VkDeviceSize size, const void* data) {
		VkUtils::TransferDataToGPUBuffer(context_, buffer, size, data);
	}

	std::shared_ptr<QbVkComputeInstance> QbVkRenderer::CreateComputeInstance(std::vector<QbVkComputeDescriptor>& descriptors,
		const char* shader, const char* shaderFunc, const VkSpecializationInfo* specInfo, uint32_t pushConstantRangeSize) {
		return computePipeline_->CreateInstance(descriptors, shader, shaderFunc, specInfo, pushConstantRangeSize);
	}

	std::shared_ptr<QbVkRenderMeshInstance> QbVkRenderer::CreateRenderMeshInstance(std::vector<QbVkRenderDescriptor>& descriptors,
		std::vector<QbVkVertexInputAttribute> vertexAttribs, const char* vertexShader, const char* vertexEntry, const char* fragmentShader, const char* fragmentEntry,
		int pushConstantStride, VkShaderStageFlags pushConstantShaderStage) {
		return meshPipeline_->CreateInstance(descriptors, vertexAttribs, vertexShader, vertexEntry, fragmentShader, fragmentEntry, pushConstantStride, pushConstantShaderStage);
	}

	std::shared_ptr<QbVkRenderMeshInstance> QbVkRenderer::CreateRenderMeshInstance(std::vector<QbVkVertexInputAttribute> vertexAttribs, const char* vertexShader, 
		const char* vertexEntry, const char* fragmentShader, const char* fragmentEntry, int pushConstantStride, VkShaderStageFlags pushConstantShaderStage) {
		std::vector<QbVkRenderDescriptor> empty{};
		return meshPipeline_->CreateInstance(empty, vertexAttribs, vertexShader, vertexEntry, fragmentShader, fragmentEntry, pushConstantStride, pushConstantShaderStage);
	}

	void QbVkRenderer::ComputeDispatch(std::shared_ptr<QbVkComputeInstance> instance) {
		computePipeline_->Dispatch(instance);
	}

	void QbVkRenderer::ComputeRecord(std::shared_ptr<QbVkComputeInstance> instance, std::function<void()> func) {
		computePipeline_->RecordCommands(instance, func);
	}

	void QbVkRenderer::DestroyMesh(const RenderMeshComponent& mesh) {
		meshPipeline_->DestroyVertexBuffer(mesh.vertexHandle);
		meshPipeline_->DestroyIndexBuffer(mesh.indexHandle);
	}

	void QbVkRenderer::DrawFrame() {
		// Return early if we cannot render (if swapchain is being recreated)
		if(!canRender_) return;

		static uint32_t resourceIndex = 0;
		RenderingResources& currentRenderingResources = context_->renderingResources[resourceIndex];
		// First we need to wait for the frame to be finished then rebuild command buffer and go
		VK_CHECK(vkWaitForFences(context_->device, 1, &currentRenderingResources.fence, VK_TRUE, UINT64_MAX));
		VK_CHECK(vkResetFences(context_->device, 1, &currentRenderingResources.fence));

		// Then we will acquire an image from the swapchain
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(context_->device, context_->swapchain.swapchain, UINT64_MAX,
			currentRenderingResources.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		if(result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapchain();
			return;
		}
		VK_CHECK(result);

		// Prepare the frame for rendering
		PrepareFrame(resourceIndex, currentRenderingResources.commandbuffer, currentRenderingResources.framebuffer, context_->swapchain.imageViews[imageIndex]);

		// We will now submit the command buffer
		VkSubmitInfo submitInfo = VkUtils::Init::SubmitInfo();
		// Here we specify the semaphores to wait for and the stage in which to wait
		// The semaphores and stages are matched to eachother by index
		VkSemaphore waitSemaphores[] = { currentRenderingResources.imageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		// Here we specify the appropriate command buffer for the swapchain image received earlier
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &currentRenderingResources.commandbuffer;
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
		if(!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
			if(result == VK_ERROR_OUT_OF_DATE_KHR) {
				RecreateSwapchain();
				return;
			}
		}
		VK_CHECK(result);

		// Increment (and mod) resource index to get the next resource
		resourceIndex = (resourceIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}

#ifdef QBDEBUG
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
		auto debugUtilsMessengerCreateFunc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
		if(debugUtilsMessengerCreateFunc != nullptr) {
			VK_CHECK(debugUtilsMessengerCreateFunc(instance_, &debugUtilsMessengerInfo, nullptr, &debugMessenger_));
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

#ifdef QBDEBUG
		instanceInfo.enabledLayerCount = VALIDATION_LAYER_COUNT;
		instanceInfo.ppEnabledLayerNames = VALIDATION_LAYERS;
#endif

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
		std::vector<VkPhysicalDevice> physicalDevices(deviceCount);

		// Fill array of physical devices
		VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, physicalDevices.data()));

		// Now find a suitable GPU
		if(!VkUtils::FindSuitableGPU(context_, physicalDevices)) {
			QB_LOG_ERROR("No suitable GPU's found on machine.\n");
		}
	}

	void QbVkRenderer::CreateLogicalDeviceAndQueues() {
		// We'll start by filling out device queue information
		std::vector<int> queueIndices;
		queueIndices.push_back(context_->gpu->graphicsFamilyIdx);
		if(context_->gpu->graphicsFamilyIdx != context_->gpu->presentFamilyIdx) {
			queueIndices.push_back(context_->gpu->presentFamilyIdx);
		}
		if (context_->gpu->graphicsFamilyIdx != context_->gpu->computeFamilyIdx && context_->gpu->presentFamilyIdx != context_->gpu->computeFamilyIdx) {
			queueIndices.push_back(context_->gpu->computeFamilyIdx);
		}

		std::vector<VkDeviceQueueCreateInfo> deviceQueueInfo;

		const float priority = 1.0f;
		for(auto i = 0; i < queueIndices.size(); i++) {
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
		//deviceFeatures.sampleRateShading = VK_TRUE;

		// Now we will fill out the actual information for device creation
		VkDeviceCreateInfo deviceInfo = VkUtils::Init::DeviceCreateInfo();
		deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueInfo.size());
		deviceInfo.pQueueCreateInfos = deviceQueueInfo.data();
		deviceInfo.pEnabledFeatures = &deviceFeatures;

#ifdef QBDEBUG
		deviceInfo.enabledLayerCount = VALIDATION_LAYER_COUNT;
		deviceInfo.ppEnabledLayerNames = VALIDATION_LAYERS;
#endif

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

		for(auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VK_CHECK(vkCreateSemaphore(context_->device, &sempahoreInfo, nullptr, &context_->renderingResources[i].imageAvailableSemaphore));
			VK_CHECK(vkCreateSemaphore(context_->device, &sempahoreInfo, nullptr, &context_->renderingResources[i].renderFinishedSemaphore));
			VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &context_->renderingResources[i].fence));
		}
	}

	void QbVkRenderer::AllocateCommandBuffers() {
		// Allocate the command buffers required for the rendering resources
		for(auto&& renderingResource : context_->renderingResources) {
			VkCommandBufferAllocateInfo commandBufferAllocateInfo = VkUtils::Init::CommandBufferAllocateInfo(context_->commandPool,
				VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
			VK_CHECK(vkAllocateCommandBuffers(context_->device, &commandBufferAllocateInfo, &renderingResource.commandbuffer));
		}
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = VkUtils::Init::CommandBufferAllocateInfo(context_->commandPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
	}

	void QbVkRenderer::CreateMultisamplingResources() {
		VkFormat colourFormat = context_->swapchain.imageFormat;

		VkImageCreateInfo imageInfo = VkUtils::Init::ImageCreateInfo(context_->swapchain.extent.width, context_->swapchain.extent.height, colourFormat,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, context_->multisamplingResources.msaaSamples);

		context_->allocator->CreateImage(context_->multisamplingResources.msaaImage, imageInfo, QBVK_MEMORY_USAGE_GPU_ONLY);
		context_->multisamplingResources.msaaImageView = VkUtils::CreateImageView(context_, context_->multisamplingResources.msaaImage.imgHandle, colourFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		// Transition image layout with a temporary command buffer
		VkUtils::TransitionImageLayout(context_, context_->multisamplingResources.msaaImage.imgHandle, 
			VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	}

	void QbVkRenderer::CreateDepthResources() {
		VkFormat depthFormat = VkUtils::FindDepthFormat(context_);

		VkImageCreateInfo imageInfo = VkUtils::Init::ImageCreateInfo(context_->swapchain.extent.width, context_->swapchain.extent.height, depthFormat,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, context_->multisamplingResources.msaaSamples);;

		context_->allocator->CreateImage(context_->depthResources.depthImage, imageInfo, QBVK_MEMORY_USAGE_GPU_ONLY);
		context_->depthResources.imageView = VkUtils::CreateImageView(context_, context_->depthResources.depthImage.imgHandle, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

		// Transition depth image
		VkImageAspectFlags aspectFlags = VkUtils::HasStencilComponent(depthFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
		VkUtils::TransitionImageLayout(context_, context_->depthResources.depthImage.imgHandle, aspectFlags, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
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
		if(context_->gpu->surfaceCapabilities.maxImageCount > 0 && (desiredSwapchainImageCount > context_->gpu->surfaceCapabilities.maxImageCount)) {
			desiredSwapchainImageCount = context_->gpu->surfaceCapabilities.maxImageCount;
		}
		// Prefer a non-rotated transform if possible
		VkSurfaceTransformFlagsKHR preTransform;
		if(context_->gpu->surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
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
		if(context_->gpu->surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
			swapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if(context_->gpu->surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
			swapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		// We need to check if the graphics and present family indices match,
		// in which case the queue can have exclusive access to the images which offers the best performance
		if(context_->gpu->graphicsFamilyIdx == context_->gpu->presentFamilyIdx) {
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
		if(oldSwapchain != VK_NULL_HANDLE) {
			for(auto i = 0; i < context_->swapchain.imageViews.size(); i++) {
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
		for(auto i = 0; i < context_->swapchain.images.size(); i++) {
			context_->swapchain.imageViews[i] = VkUtils::CreateImageView(context_, context_->swapchain.images[i], context_->swapchain.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
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

		// Mesh Pipeline
		meshPipeline_->RebuildPipeline();

		// ImGui Pipeline (Debug only)
		imGuiPipeline_->RebuildPipeline();

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
		depthAttachment.format = VkUtils::FindDepthFormat(context_);
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
		std::array<VkAttachmentDescription, 3> attachments = 
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
		ImGui::NewFrame();

		imGuiPipeline_->ImGuiDrawState();
		EntityManager::GetOrCreate()->systemDispatch_->ImGuiDrawState();
		context_->allocator->ImGuiDrawState();
		computePipeline_->ImGuiDrawState();

		// Get injected ImGui commands from the global state
		for (const auto& injector : ImGuiState::injectors) {
			injector();
		}

		// Render to generate draw buffers
		ImGui::Render();
	}

	void QbVkRenderer::PrepareFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer, VkFramebuffer& framebuffer, VkImageView imageView) {
		// Let the allocator cleanup stuff before preparing the frame
		context_->allocator->EmptyGarbage();

		std::array<VkClearValue, 2> clearValues;

		VkCommandBufferBeginInfo commandBufferInfo = VkUtils::Init::CommandBufferBeginInfo();
		commandBufferInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(commandbuffer, &commandBufferInfo));

		// Create the frame buffer
		VkUtils::CreateFrameBuffer(context_, framebuffer, imageView);

		VkRenderPassBeginInfo renderPassInfo = VkUtils::Init::RenderPassBeginInfo();
		renderPassInfo.renderPass = context_->mainRenderPass;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = context_->swapchain.extent;

		// This specifies the clear values to use for the VK_ATTACHMENT_LOAD_OP_CLEAR operation (in the color and depth attachments)
		clearValues[0].color = { 0.2f, 0.2f, 0.2f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues.data();
		renderPassInfo.framebuffer = framebuffer;

		vkCmdBeginRenderPass(commandbuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		meshPipeline_->DrawFrame(resourceIndex, commandbuffer);

		ImGuiUpdateContent();
		imGuiPipeline_->DrawFrame(resourceIndex, commandbuffer);

		vkCmdEndRenderPass(commandbuffer);
		VK_CHECK(vkEndCommandBuffer(commandbuffer));
	}
}