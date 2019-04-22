#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

#define VK_ERROR_STRING(x) case (int)x: return #x;

#define VK_CHECK(x) { \
VkResult ret = x; \
if(ret != VK_SUCCESS) QB_LOG_WARN("VkResult: %s is %s in %s at line %d\n", #x, VulkanErrorToString(x), __FILE__, __LINE__); \
}

#define VK_VALIDATE(x, msg) { \
if(!(x)) QB_LOG_WARN("VK: %s - %s\n", msg, #x); \
}

#ifdef QBDEBUG
constexpr int VALIDATION_LAYER_COUNT = 2;
constexpr const char* VALIDATION_LAYERS[VALIDATION_LAYER_COUNT]{
"VK_LAYER_LUNARG_standard_validation",
"VK_LAYER_LUNARG_monitor",
};
#endif

// Add debug messenger callback extension for validation layer if in debug mode
#ifdef QBDEBUG
constexpr int INSTANCE_EXT_COUNT = 3;
constexpr const char* INSTANCE_EXT_NAMES[INSTANCE_EXT_COUNT]{
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};
#else
constexpr int INSTANCE_EXT_COUNT = 2;
constexpr const char* INSTANCE_EXT_NAMES[INSTANCE_EXT_COUNT]{
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME
};
#endif

constexpr int DEVICE_EXT_COUNT = 1;
constexpr const char* DEVICE_EXT_NAMES[DEVICE_EXT_COUNT]{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr int MAX_MESH_COUNT = 8192;

constexpr const char* VulkanErrorToString(VkResult vkResult) {
	switch(vkResult) {
		VK_ERROR_STRING(VK_SUCCESS);
		VK_ERROR_STRING(VK_NOT_READY);
		VK_ERROR_STRING(VK_TIMEOUT);
		VK_ERROR_STRING(VK_EVENT_SET);
		VK_ERROR_STRING(VK_EVENT_RESET);
		VK_ERROR_STRING(VK_INCOMPLETE);
		VK_ERROR_STRING(VK_ERROR_OUT_OF_HOST_MEMORY);
		VK_ERROR_STRING(VK_ERROR_OUT_OF_DEVICE_MEMORY);
		VK_ERROR_STRING(VK_ERROR_INITIALIZATION_FAILED);
		VK_ERROR_STRING(VK_ERROR_DEVICE_LOST);
		VK_ERROR_STRING(VK_ERROR_MEMORY_MAP_FAILED);
		VK_ERROR_STRING(VK_ERROR_LAYER_NOT_PRESENT);
		VK_ERROR_STRING(VK_ERROR_EXTENSION_NOT_PRESENT);
		VK_ERROR_STRING(VK_ERROR_FEATURE_NOT_PRESENT);
		VK_ERROR_STRING(VK_ERROR_INCOMPATIBLE_DRIVER);
		VK_ERROR_STRING(VK_ERROR_TOO_MANY_OBJECTS);
		VK_ERROR_STRING(VK_ERROR_FORMAT_NOT_SUPPORTED);
		VK_ERROR_STRING(VK_ERROR_SURFACE_LOST_KHR);
		VK_ERROR_STRING(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
		VK_ERROR_STRING(VK_SUBOPTIMAL_KHR);
		VK_ERROR_STRING(VK_ERROR_OUT_OF_DATE_KHR);
		VK_ERROR_STRING(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
		VK_ERROR_STRING(VK_ERROR_VALIDATION_FAILED_EXT);
		VK_ERROR_STRING(VK_ERROR_INVALID_SHADER_NV);
		VK_ERROR_STRING(VK_RESULT_BEGIN_RANGE);
		VK_ERROR_STRING(VK_RESULT_RANGE_SIZE);
	default: return "UNKNOWN";
	}
}

namespace Quadbit {
	using VertexBufHandle = uint16_t;
	using IndexBufHandle = uint16_t;

	enum QbVkMemoryUsage {
		QBVK_MEMORY_USAGE_UNKNOWN,
		QBVK_MEMORY_USAGE_CPU_ONLY,
		QBVK_MEMORY_USAGE_GPU_ONLY,
		QBVK_MEMORY_USAGE_CPU_TO_GPU,
		QBVK_MEMORY_USAGE_GPU_TO_CPU
	};

	enum QbVkAllocationType {
		QBVK_ALLOCATION_TYPE_UNKNOWN,
		QBVK_ALLOCATION_TYPE_FREE,
		QBVK_ALLOCATION_TYPE_BUFFER,
		QBVK_ALLOCATION_TYPE_IMAGE_UNKNOWN,
		QBVK_ALLOCATION_TYPE_IMAGE_LINEAR,
		QBVK_ALLOCATION_TYPE_IMAGE_OPTIMAL,
	};

	class QbVkPool;
	struct QbVkAllocation {
		std::shared_ptr<QbVkPool> pool = nullptr;
		uint32_t id = 0;
		VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
		VkDeviceSize offset = 0;
		VkDeviceSize size = 0;
		std::byte* data = nullptr;
	};

	struct QbVkBuffer {
		VkBuffer buf = VK_NULL_HANDLE;
		QbVkAllocation alloc{};
	};

	struct QbVkImage {
		VkImage img = VK_NULL_HANDLE;
		QbVkAllocation alloc{};
	};

	struct GPU {
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

		VkSurfaceCapabilitiesKHR surfaceCapabilities{};
		VkPhysicalDeviceProperties deviceProps{};
		VkPhysicalDeviceMemoryProperties memoryProps{};
		VkPhysicalDeviceFeatures features{};

		std::vector<VkSurfaceFormatKHR> surfaceFormats;
		std::vector<VkPresentModeKHR> presentModes;

		std::vector<VkQueueFamilyProperties> queueProps;
		std::vector<VkExtensionProperties> extensionProps;

		int graphicsFamilyIdx = -1;
		int presentFamilyIdx = -1;
	};

	struct Swapchain {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkFormat imageFormat{};
		VkPresentModeKHR presentMode{};
		VkExtent2D extent{};
		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
	};

	struct DepthResources {
		QbVkImage depthImage{};
		VkImageView imageView = VK_NULL_HANDLE;
	};

	struct RenderingResources {
		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		VkCommandBuffer commandbuffer = VK_NULL_HANDLE;
		VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
		VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
	};

	class QbVkAllocator;
	struct QbVkContext {
		std::unique_ptr<GPU> gpu;
		std::unique_ptr<QbVkAllocator> allocator;
		VkDevice device = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkQueue graphicsQueue = VK_NULL_HANDLE;
		VkQueue presentQueue = VK_NULL_HANDLE;

		DepthResources depthResources{};
		Swapchain swapchain{};

		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkRenderPass mainRenderPass = VK_NULL_HANDLE;

		std::array<RenderingResources, MAX_FRAMES_IN_FLIGHT> renderingResources;
	};
}