#pragma once

#include "../Common/QbVkCommon.h"

class QbVkPool : public std::enable_shared_from_this<QbVkPool> {
public:
	VkDeviceSize capacity_;
	VkDeviceSize allocatedSize_;
	int32_t memoryTypeIndex_;

	QbVkPool(VkDevice device, const int32_t memoryTypeIndex, const VkDeviceSize size, QbVkMemoryUsage usage);

	bool Allocate(VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize granularity, QbVkAllocationType allocationType, QbVkAllocation& allocation);
	void Free(QbVkAllocation& allocation);

	void Shutdown();

private:
	VkDevice device_;

	struct Block {
		Block() {
			//static int created = 0;
			//printf("Blocks created: %i\n", created++);
		}
		~Block() {
			//static int destroyed = 0;
			//printf("Blocks destroyed: %i\n", destroyed++);
		}

		uint32_t id = 0;
		VkDeviceSize size = 0;
		VkDeviceSize offset = 0;
		Block* prev = nullptr;
		std::unique_ptr<Block> next = nullptr;
		QbVkAllocationType allocationType = QBVK_ALLOCATION_TYPE_UNKNOWN;
	};
	std::unique_ptr<Block> head_;

	uint32_t nextBlockId_;
	VkDeviceMemory deviceMemory_;
	QbVkMemoryUsage memoryUsage_;
	std::byte* data_;
};