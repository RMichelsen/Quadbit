#pragma once

#include "../Common/QbVkCommon.h"

namespace Quadbit {
	class QbVkPool : public std::enable_shared_from_this<QbVkPool> {
	public:
		VkDeviceSize capacity_;
		VkDeviceSize allocatedSize_ = 0;
		int32_t memoryTypeIndex_;

		QbVkPool(VkDevice device, const int32_t memoryTypeIndex, const VkDeviceSize size, QbVkMemoryUsage usage);

		bool Allocate(VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize granularity, QbVkAllocationType allocationType, QbVkAllocation& allocation);
		void Free(QbVkAllocation& allocation);

		void Shutdown();

		void DrawImGuiPool(uint32_t num);

	private:
		VkDevice device_;

		struct Block {
			uint32_t id = 0;
			VkDeviceSize size = 0;
			VkDeviceSize offset = 0;
			Block* prev = nullptr;
			std::unique_ptr<Block> next = nullptr;
			QbVkAllocationType allocationType = QBVK_ALLOCATION_TYPE_UNKNOWN;
		};
		std::unique_ptr<Block> head_ = std::make_unique<Block>();

		uint32_t nextBlockId_ = 0;
		VkDeviceMemory deviceMemory_ = 0;
		QbVkMemoryUsage memoryUsage_;
		std::byte* data_ = nullptr;
	};
}