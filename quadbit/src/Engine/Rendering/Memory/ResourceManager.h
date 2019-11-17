#pragma once

#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Rendering/Memory/Allocator.h"

constexpr uint32_t MAX_TRANSFERS_PER_FRAME = 1024;

constexpr uint32_t MAX_BUFFER_COUNT = 65536;
constexpr uint32_t MAX_TEXTURE_COUNT = 128;

namespace Quadbit {
	// Handles all buffer transfers submitted during the frame in one commandbuffer,
	// and signals a semaphore that is waited on by the final queue submit
	struct PerFrameTransfers {
		uint32_t count;
		std::array<QbVkBuffer, MAX_TRANSFERS_PER_FRAME> stagingBuffers;
		std::array<QbVkTransfer, MAX_TRANSFERS_PER_FRAME> transfers;
		VkCommandBuffer commandBuffer;

		PerFrameTransfers(const QbVkContext& context);

		QbVkTransfer& operator[](int index) {
			return transfers[index];
		}

		void Reset() { count = 0; }
	};

	class QbVkResourceManager {
	public:
		QbVkResourceManager(QbVkContext& context);
		~QbVkResourceManager();
		void TransferDataToGPU(const void* data, VkDeviceSize size, QbVkBufferHandle destination);
		bool TransferQueuedDataToGPU(uint32_t resourceIndex);

		template<typename T>
		void DestroyResource(QbVkResourceHandle<T> handle) {
			if constexpr (std::is_same<T, QbVkBuffer>::value) {
				context_.allocator->DestroyBuffer(buffers[handle]);
				buffers.DestroyResource(handle);
			}
			else if constexpr (std::is_same<T, QbVkTexture>::value) {
				QbVkTexture& texture = textures[handle];
				context_.allocator->DestroyImage(texture.image);
				if (texture.imageView != VK_NULL_HANDLE) vkDestroyImageView(context_.device, texture.imageView, nullptr);
				if (texture.sampler != VK_NULL_HANDLE) vkDestroySampler(context_.device, texture.sampler, nullptr);
			}
		}

		QbVkResource<QbVkBuffer, MAX_BUFFER_COUNT> buffers;
		QbVkResource<QbVkTexture, MAX_TEXTURE_COUNT> textures;

	private:
		QbVkContext& context_;
		PerFrameTransfers transferQueue_;
	};
}