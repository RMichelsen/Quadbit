#include "QbVkAllocator.h"

#include <imgui/imgui.h>

#include "Engine/Core/QbVulkanDefs.h"
#include "Engine/Rendering/QbVulkanUtils.h"

constexpr int DEFAULT_DEVICE_LOCAL_POOLSIZE = 256 * 1024 * 1024;
constexpr int DEFAULT_HOST_VISIBLE_POOLSIZE = 128 * 1024 * 1024;

namespace Quadbit {
	QbVkAllocator::QbVkAllocator(VkDevice device, VkDeviceSize bufferImageGranularity, VkPhysicalDeviceMemoryProperties memoryProperties) :
		device_(device),
		bufferImageGranularity_(bufferImageGranularity),
		memoryProperties_(memoryProperties),
		garbageIndex_(0) {}

	QbVkAllocator::~QbVkAllocator() {
		for(auto&& pools : poolsByType_) {
			for(auto&& pool : pools) {
				pool->Shutdown();
			}
		}
	}

	void QbVkAllocator::CreateStagingBuffer(QbVkBuffer& buffer, VkDeviceSize size, const void* data) {
		VkBufferCreateInfo bufferInfo = VkUtils::Init::BufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		CreateBuffer(buffer, bufferInfo, QBVK_MEMORY_USAGE_CPU_ONLY);

		// Copy the data to the mapped buffer
		memcpy(buffer.alloc.data, data, static_cast<size_t>(size));
	}

	void QbVkAllocator::CreateBuffer(QbVkBuffer& buffer, VkBufferCreateInfo& bufferInfo, QbVkMemoryUsage memoryUsage) {
		VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer.buf));

		// This part finds the required memory properties for the buffer allocation
		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(device_, buffer.buf, &memoryRequirements);

		buffer.alloc = Allocate(memoryRequirements.size, memoryRequirements.alignment,
			memoryRequirements.memoryTypeBits, memoryUsage, QBVK_ALLOCATION_TYPE_BUFFER);

		VK_CHECK(vkBindBufferMemory(device_, buffer.buf, buffer.alloc.deviceMemory, buffer.alloc.offset));

		// Fill out the descriptor
		buffer.descriptor.buffer = buffer.buf;
		buffer.descriptor.offset = 0;
		buffer.descriptor.range = bufferInfo.size;
	}

	void QbVkAllocator::CreateBuffer(QbVkAsyncBuffer& buffer, VkBufferCreateInfo& bufferInfo, QbVkMemoryUsage memoryUsage) {
		VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer.buf));

		// This part finds the required memory properties for the buffer allocation
		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(device_, buffer.buf, &memoryRequirements);

		buffer.alloc = Allocate(memoryRequirements.size, memoryRequirements.alignment,
			memoryRequirements.memoryTypeBits, memoryUsage, QBVK_ALLOCATION_TYPE_BUFFER);

		VK_CHECK(vkBindBufferMemory(device_, buffer.buf, buffer.alloc.deviceMemory, buffer.alloc.offset));

		// Fill out the descriptor
		buffer.descriptor.buffer = buffer.buf;
		buffer.descriptor.offset = 0;
		buffer.descriptor.range = bufferInfo.size;
	}

	void QbVkAllocator::CreateImage(QbVkImage& image, VkImageCreateInfo& imageInfo, QbVkMemoryUsage memoryUsage) {
		VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &image.imgHandle));

		// This part finds the required memory properties for the image allocation
		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device_, image.imgHandle, &memoryRequirements);

		// Allocation type is determined by the tiling information
		auto allocType = (imageInfo.tiling == VK_IMAGE_TILING_OPTIMAL) ? QBVK_ALLOCATION_TYPE_IMAGE_OPTIMAL : QBVK_ALLOCATION_TYPE_IMAGE_LINEAR;

		image.alloc = Allocate(memoryRequirements.size, memoryRequirements.alignment,
			memoryRequirements.memoryTypeBits, memoryUsage, allocType);

		VK_CHECK(vkBindImageMemory(device_, image.imgHandle, image.alloc.deviceMemory, image.alloc.offset));
	}

	void QbVkAllocator::DestroyBuffer(QbVkAsyncBuffer& buffer, VkCommandPool commandPool) {
		if(buffer.buf != VK_NULL_HANDLE) vkDestroyBuffer(device_, buffer.buf, nullptr);
		if(buffer.alloc.deviceMemory != VK_NULL_HANDLE) Free(buffer.alloc);
		if(buffer.fence != VK_NULL_HANDLE) vkDestroyFence(device_, buffer.fence, nullptr);
		if(buffer.copyCommandBuffer != VK_NULL_HANDLE) vkFreeCommandBuffers(device_, commandPool, 1, &buffer.copyCommandBuffer);
		if(buffer.stagingBuffer.buf != VK_NULL_HANDLE) DestroyBuffer(buffer.stagingBuffer);

		buffer = QbVkAsyncBuffer{};
	}

	void QbVkAllocator::DestroyBuffer(QbVkBuffer& buffer) {
		if(buffer.buf != VK_NULL_HANDLE) vkDestroyBuffer(device_, buffer.buf, nullptr);
		if(buffer.alloc.deviceMemory != VK_NULL_HANDLE) Free(buffer.alloc);

		buffer = QbVkBuffer{};
	}

	void QbVkAllocator::DestroyImage(QbVkImage& image) {
		if(image.imgHandle != VK_NULL_HANDLE) vkDestroyImage(device_, image.imgHandle, nullptr);
		if(image.alloc.deviceMemory != VK_NULL_HANDLE) Free(image.alloc);
	}

	void QbVkAllocator::EmptyGarbage() {
		auto& garbage = garbage_[garbageIndex_];

		for(auto&& allocation : garbage) {
			allocation.pool->Free(allocation);

			if (allocation.pool->allocatedSize_ == 0) {
				poolsByType_[allocation.pool->memoryTypeIndex_].remove_if(
					[&](const auto& p) { return p.get() == allocation.pool; });
			}
		}
		garbage.clear();
		garbageIndex_ = (garbageIndex_ + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void QbVkAllocator::ImGuiDrawState() {
		ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
		ImGui::Begin("Quadbit Vulkan Allocator", nullptr);

		char memoryTypeTitle[16];
		for(auto i = 0; i < poolsByType_.size(); i++) {
			if(poolsByType_[i].empty()) continue;
			sprintf(memoryTypeTitle, "Memory Type %i", i);
			if(ImGui::CollapsingHeader(memoryTypeTitle)) {
				uint32_t num = 0;
				for(auto&& pool : poolsByType_[i]) {
					pool->DrawImGuiPool(num++);
				}
			}
		}

		ImGui::End();
	}


	int32_t QbVkAllocator::FindMemoryProperties(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties) {
		for(uint32_t memoryIndex = 0; memoryIndex < memoryProperties_.memoryTypeCount; memoryIndex++) {
			const uint32_t memoryTypeBits = (1 << memoryIndex);
			const bool isRequiredMemoryType = memoryTypeBitsRequirement & memoryTypeBits;
			const bool hasRequiredProperties = (memoryProperties_.memoryTypes[memoryIndex].propertyFlags & requiredProperties) == requiredProperties;

			if(isRequiredMemoryType && hasRequiredProperties) {
				return static_cast<int32_t>(memoryIndex);
			}
		}
		// Returns -1 if no suitable memory type is found
		return -1;
	}

	int32_t QbVkAllocator::FindMemoryTypeIndex(const uint32_t memoryTypeBits, QbVkMemoryUsage memoryUsage) {
		VkMemoryPropertyFlags requiredMemoryProperties = 0;
		VkMemoryPropertyFlags optimalMemoryProperties = 0;

		switch(memoryUsage) {
		case QBVK_MEMORY_USAGE_UNKNOWN:
			break;
		case QBVK_MEMORY_USAGE_GPU_ONLY:
			requiredMemoryProperties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		case QBVK_MEMORY_USAGE_CPU_ONLY:
			requiredMemoryProperties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		case QBVK_MEMORY_USAGE_CPU_TO_GPU:
			requiredMemoryProperties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			break;
		case QBVK_MEMORY_USAGE_GPU_TO_CPU:
			optimalMemoryProperties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
			requiredMemoryProperties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			break;
		default:
			QB_LOG_WARN("QbVkAllocator: Couldn't find appropriate memory type.\n");
		}

		// First try to find a memorytype that is both preferred and required
		uint32_t memoryType = FindMemoryProperties(memoryTypeBits, optimalMemoryProperties | requiredMemoryProperties);
		// Otherwise just find a memorytype that is required
		if(memoryType == -1) memoryType = FindMemoryProperties(memoryTypeBits, requiredMemoryProperties);

		// Returns -1 if no memory type is found
		return memoryType;
	}

	QbVkAllocation QbVkAllocator::Allocate(const VkDeviceSize size, const VkDeviceSize alignment,
		const uint32_t memoryTypeBits, const QbVkMemoryUsage memoryUsage, QbVkAllocationType allocType) {

		QbVkAllocation allocation{};

		auto memoryTypeIndex = FindMemoryTypeIndex(memoryTypeBits, memoryUsage);
		if(memoryTypeIndex == -1) {
			QB_LOG_WARN("Couldn't find appropriate memory type for allocation request\n");
			return QbVkAllocation{};
		}

		// Now try to allocate from any pool with the right memory type index
		auto& pools = poolsByType_[memoryTypeIndex];
		for(auto&& pool : pools) {
			if(pool->Allocate(size, alignment, bufferImageGranularity_, allocType, allocation)) {
				return allocation;
			}
		}

		// Otherwise we'll just create a new pool
		// 256MB for device local, 64MB for host-visible
		VkDeviceSize poolSize = (memoryUsage == QBVK_MEMORY_USAGE_GPU_ONLY) ? DEFAULT_DEVICE_LOCAL_POOLSIZE : DEFAULT_HOST_VISIBLE_POOLSIZE;
		pools.push_front(std::make_unique<QbVkPool>(device_, memoryTypeIndex, poolSize, memoryUsage));
		if(!pools.front()->Allocate(size, alignment, bufferImageGranularity_, allocType, allocation)) {
			QB_LOG_WARN("Failed to allocate new memory block\n");
		}

		return allocation;
	}

	void QbVkAllocator::Free(QbVkAllocation& allocation) {
		allocation.pool->Free(allocation);
	}
}