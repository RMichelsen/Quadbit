#include <PCH.h>
#include "QbVkPool.h"

#include "../Common/QbVkUtils.h"

namespace Quadbit {
	QbVkPool::QbVkPool(VkDevice device, const int32_t memoryTypeIndex, const VkDeviceSize size, QbVkMemoryUsage usage) :
		device_(device), capacity_(size), memoryTypeIndex_(memoryTypeIndex), memoryUsage_(usage) {

		VkMemoryAllocateInfo memoryAllocateInfo = VkUtils::Init::MemoryAllocateInfo();
		memoryAllocateInfo.allocationSize = capacity_;
		memoryAllocateInfo.memoryTypeIndex = static_cast<uint32_t>(memoryTypeIndex_);
		VK_CHECK(vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &deviceMemory_));

		// If the memory is host visible, map it
		if(memoryUsage_ != QBVK_MEMORY_USAGE_GPU_ONLY) {
			VK_CHECK(vkMapMemory(device_, deviceMemory_, 0, size, 0, (void**)& data_));
		}

		// Set up the list
		head_->id = nextBlockId_++;
		head_->size = size;
		head_->offset = 0;
		head_->prev = nullptr;
		head_->next = nullptr;
		head_->allocationType = QBVK_ALLOCATION_TYPE_FREE;
	}

	bool QbVkPool::Allocate(VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize granularity, QbVkAllocationType allocationType, QbVkAllocation& allocation) {
		// Return immediately if there isn't room enough in the pool
		if((allocatedSize_ + size) > capacity_) return false;

		VkDeviceSize padding = 0;
		VkDeviceSize offset = 0;
		VkDeviceSize alignedSize = 0;

		Block* prev = nullptr;
		Block* current = nullptr;
		Block* bestCandidate = nullptr;

		// Now iterate the blocks to find a best candidate
		for(current = head_.get(); current != nullptr; prev = current, current = current->next.get()) {
			if(current->allocationType != QBVK_ALLOCATION_TYPE_FREE) continue;
			if(size > current->size) continue;

			offset = VkUtils::AlignUp(current->offset, alignment);

			// First check for granularity conflict with the previous allocation
			if(granularity > 1 && prev != nullptr) {
				// First check if the two resources overlap (are on the same page)
				if(VkUtils::IsOnSamePage(prev->offset, prev->size, offset, granularity)) {
					// If they are, check for a granularity conflict and adjust the 
					// offset to account for and ensure that the following property holds:
					// resourceA.endPage < resourceB.startPage
					if(VkUtils::HasGranularityConflict(prev->allocationType, allocationType)) {
						offset = VkUtils::AlignUp(offset, granularity);
					}
				}
			}

			// We now have the actual offset and need to add the padding as the "delta-offset"
			padding = offset - current->offset;
			alignedSize = padding + size;

			// If the aligned size is greater than the size of the current block try the next one
			// If the aligned size and the currently allocated size (of the pool) is larger than its capacity, 
			// its not possible to allocate the memory in this pool
			if(alignedSize > current->size) continue;
			if(alignedSize + allocatedSize_ >= capacity_) return false;

			// Now we check for granularity conflict with the next allocation
			// If there is a conflict we cannot allocate the block in this pool
			if(granularity > 1 && current->next != nullptr) {
				Block* next = current->next.get();
				if(VkUtils::IsOnSamePage(offset, size, next->offset, granularity)) {
					if(VkUtils::HasGranularityConflict(allocationType, next->allocationType)) {
						continue;
					}
				}
			}

			bestCandidate = current;
			break;
		}

		// If no candidate is found, the allocation is not possible from this pool
		if(bestCandidate == nullptr) return false;

		// If the size of the candidate is greater than the size of the requested allocation,
		// we'll allocate a new block to fill the size leftover in the old block after allocation
		if(bestCandidate->size > size) {
			std::unique_ptr<Block> newBlock = std::make_unique<Block>();
			newBlock->next = std::move(bestCandidate->next);

			newBlock->id = nextBlockId_++;
			newBlock->prev = bestCandidate;

			if(newBlock->next != nullptr) {
				newBlock->next.get()->prev = newBlock.get();
			}

			newBlock->size = bestCandidate->size - alignedSize;
			newBlock->offset = offset + size;
			newBlock->allocationType = QBVK_ALLOCATION_TYPE_FREE;

			bestCandidate->next = std::move(newBlock);
		}
		bestCandidate->size = size;
		bestCandidate->allocationType = allocationType;

		allocatedSize_ += alignedSize;

		allocation.size = bestCandidate->size;
		allocation.id = bestCandidate->id;
		allocation.deviceMemory = deviceMemory_;
		if(memoryUsage_ != QBVK_MEMORY_USAGE_GPU_ONLY) {
			allocation.data = data_ + offset;
		}
		allocation.offset = offset;
		allocation.pool = shared_from_this();

		return true;
	}

	void QbVkPool::Free(QbVkAllocation& allocation) {
		// First we find the appropriate allocation in the list of blocks
		Block* current = nullptr;
		for(current = head_.get(); current != nullptr; current = current->next.get()) {
			if(current->id == allocation.id) break;
		}

		// If no allocation was found matching the id we'll return,
		// but this shouldn't happen so we throw a diagnostic msg
		if(current == nullptr) {
			QB_LOG_WARN("QbVkAllocator: Trying to free an unknown allocation (%i) in pool %p", allocation.id, allocation.pool.get());
			return;
		}

		current->allocationType = QBVK_ALLOCATION_TYPE_FREE;

		// We need to merge either side of the freed allocation in case they are also free.
		// Merge left side
		if(current->prev != nullptr && current->prev->allocationType == QBVK_ALLOCATION_TYPE_FREE) {
			Block* prev = current->prev;

			if(current->next.get() != nullptr) {
				current->next->prev = prev;
			}
			prev->size += current->size;

			// This move should invalidate the current block so its merged into the previous
			prev->next = std::move(current->next);

			current = prev;
		}
		// Merge right side
		if(current->next.get() != nullptr && current->next->allocationType == QBVK_ALLOCATION_TYPE_FREE) {
			Block* next = current->next.get();

			if(next->next.get() != nullptr) {
				next->next.get()->prev = current;
			}
			current->size += next->size;

			// This move invalidates the next block so its merged into the current
			current->next = std::move(next->next);
		}

		allocatedSize_ -= allocation.size;
	}

	void QbVkPool::Shutdown() {
		// Unmap host-mapped memory if applicable
		if(memoryUsage_ != QBVK_MEMORY_USAGE_GPU_ONLY) {
			vkUnmapMemory(device_, deviceMemory_);
		}

		vkFreeMemory(device_, deviceMemory_, nullptr);
		deviceMemory_ = VK_NULL_HANDLE;
	}

	void QbVkPool::DrawImGuiPool(uint32_t num) {
		uint32_t occupiedBlocks = 0;
		uint32_t totalBlocks = 0;
		Block* current = nullptr;
		for(current = head_.get(); current != nullptr; current = current->next.get()) {
			if(current->allocationType != QBVK_ALLOCATION_TYPE_FREE) {
				occupiedBlocks++;
			}
			totalBlocks++;
		}
		ImGui::Text("Pool %i: %.2f/%.2f MB allocated in %i/%i blocks", num,
			allocatedSize_ / 1024.0f / 1024.0f, capacity_ / 1024.0f / 1024.0f, occupiedBlocks, totalBlocks);
	}
}