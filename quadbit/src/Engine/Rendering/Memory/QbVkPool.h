#pragma once

#include "Engine/Core/QbVulkanDefs.h"

namespace Quadbit {
	struct QbVkPool {
		VkDeviceSize capacity_;
		VkDeviceSize allocatedSize_ = 0;
		int32_t memoryTypeIndex_;
		VkDevice device_;
		uint32_t nextBlockId_ = 0;
		VkDeviceMemory deviceMemory_ = 0;
		QbVkMemoryUsage memoryUsage_;
		unsigned char* data_ = nullptr;

		struct Block {
			uint32_t id = 0;
			VkDeviceSize size = 0;
			VkDeviceSize offset = 0;
			Block* prev = nullptr;
			std::unique_ptr<Block> next = nullptr;
			QbVkAllocationType allocationType = QbVkAllocationType::QBVK_ALLOCATION_TYPE_UNKNOWN;
		};
		std::unique_ptr<Block> head_ = std::make_unique<Block>();

		QbVkPool(VkDevice device, const int32_t memoryTypeIndex, const VkDeviceSize size, QbVkMemoryUsage usage);
		~QbVkPool();

		bool Allocate(VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize granularity, QbVkAllocationType allocationType, QbVkAllocation& allocation);
		void Free(QbVkAllocation& allocation);
		void Shutdown();
		void DrawImGuiPool(uint32_t num);
	};
}