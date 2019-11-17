#include "ResourceManager.h"

#include "Engine/Rendering/VulkanUtils.h"

namespace Quadbit {
	PerFrameTransfers::PerFrameTransfers(const QbVkContext& context) : count(0) {
		commandBuffer = VkUtils::CreatePersistentCommandBuffer(context);
	}

	QbVkResourceManager::QbVkResourceManager(QbVkContext& context) : context_(context), transferQueue_(PerFrameTransfers(context)) {}

	QbVkResourceManager::~QbVkResourceManager() {
		// Destroy staging buffers...
		for (auto& stagingBuffer : transferQueue_.stagingBuffers) {
			// Simply break when we reach the end of the buffers
			if (stagingBuffer.buf == VK_NULL_HANDLE) break;
			context_.allocator->DestroyBuffer(stagingBuffer);
		}
		// Destroy regular GPU buffers
		for (uint16_t i = 0; i < buffers.resourceIndex; i++) {
			if (std::find(buffers.freeList.begin(), buffers.freeList.end(), i) == buffers.freeList.end()) {
				DestroyResource<QbVkBuffer>(buffers.GetHandle(i));
			}
		}
		// Destroy textures...
		for (uint16_t i = 0; i < textures.resourceIndex; i++) {
			if (std::find(textures.freeList.begin(), textures.freeList.end(), i) == textures.freeList.end()) {
				DestroyResource<QbVkTexture>(textures.GetHandle(i));
			}
		}
	}

	void QbVkResourceManager::TransferDataToGPU(const void* data, VkDeviceSize size, QbVkBufferHandle destination) {
		// If its the first transfer of the frame, we destroy the staging buffers from the previous transfer frame
		if (transferQueue_.count == 0) {
			for (auto& stagingBuffer : transferQueue_.stagingBuffers) {
				// Simply break when we reach the end of the buffers
				if (stagingBuffer.buf == VK_NULL_HANDLE) break;
				context_.allocator->DestroyBuffer(stagingBuffer);
			}
		}

		context_.allocator->CreateStagingBuffer(transferQueue_.stagingBuffers[transferQueue_.count], size, data);
		transferQueue_.transfers[transferQueue_.count] = { size, destination };
		transferQueue_.count++;
	}

	bool QbVkResourceManager::TransferQueuedDataToGPU(uint32_t resourceIndex) {
		if (transferQueue_.count == 0) return false;

		VkCommandBufferBeginInfo commandBufferInfo = VkUtils::Init::CommandBufferBeginInfo();
		commandBufferInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(transferQueue_.commandBuffer, &commandBufferInfo));

		for (uint32_t i = 0; i < transferQueue_.count; i++) {
			// Issue copy command
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = transferQueue_[i].size;
			vkCmdCopyBuffer(transferQueue_.commandBuffer, transferQueue_.stagingBuffers[i].buf, buffers[transferQueue_[i].destinationBuffer].buf, 1, &copyRegion);
		}

		// End recording
		VK_CHECK(vkEndCommandBuffer(transferQueue_.commandBuffer));

		VkSubmitInfo submitInfo = VkUtils::Init::SubmitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &transferQueue_.commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		std::array<VkSemaphore, 1> transferSemaphore{ context_.renderingResources[resourceIndex].transferSemaphore };
		submitInfo.pSignalSemaphores = transferSemaphore.data();

		// Submit to queue
		VK_CHECK(vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

		transferQueue_.count = 0;
		return true;
	}
}