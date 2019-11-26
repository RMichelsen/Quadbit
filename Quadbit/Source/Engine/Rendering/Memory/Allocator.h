#pragma once
#include <EASTL/array.h>
#include <EASTL/slist.h>
#include <EASTL/unique_ptr.h>

#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Rendering/Memory/Pool.h"

namespace Quadbit {
	class QbVkAllocator {
	public:
		QbVkAllocator(VkDevice device, VkDeviceSize bufferImageGranularity, VkPhysicalDeviceMemoryProperties memoryProperties);

		void CreateStagingBuffer(QbVkBuffer& buffer, VkDeviceSize size, const void* data);
		void CreateBuffer(QbVkBuffer& buffer, VkBufferCreateInfo& bufferInfo, QbVkMemoryUsage memoryUsage);
		void CreateImage(QbVkImage& image, VkImageCreateInfo& imageInfo, QbVkMemoryUsage memoryUsage);

		void DestroyBuffer(QbVkBuffer& buffer);
		void DestroyImage(QbVkImage& image);
		void EmptyGarbage();

		void ImGuiDrawState();

	private:
		VkDevice device_;
		VkDeviceSize bufferImageGranularity_;
		VkPhysicalDeviceMemoryProperties memoryProperties_;

		eastl::array<eastl::slist<eastl::unique_ptr<QbVkPool>>, VK_MAX_MEMORY_TYPES> poolsByType_;

		uint32_t garbageIndex_;
		eastl::array<eastl::vector<QbVkAllocation>, MAX_FRAMES_IN_FLIGHT> garbage_;

		int32_t FindMemoryProperties(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties);
		int32_t FindMemoryTypeIndex(const uint32_t memoryTypeBitsRequirement, QbVkMemoryUsage memoryUsage);

		QbVkAllocation Allocate(const VkDeviceSize size, const VkDeviceSize alignment, const uint32_t memoryTypeBits,
			const QbVkMemoryUsage memoryUsage, QbVkAllocationType allocType);
		void Free(QbVkAllocation& allocation);
	};
}