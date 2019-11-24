#pragma once

#include <cstdint>

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>
#include <EASTL/type_traits.h>

#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Rendering/Pipelines/Pipeline.h"

constexpr size_t MAX_TRANSFERS_PER_FRAME = 1024;
constexpr size_t MAX_BUFFER_COUNT = 65536;
constexpr size_t MAX_TEXTURE_COUNT = 512;
constexpr size_t MAX_DESCRIPTOR_INSTANCES = 128;
constexpr size_t MAX_PIPELINES = 128;

namespace Quadbit {
	// Handles all buffer transfers submitted during the frame in one commandbuffer,
	// and signals a semaphore that is waited on by the final queue submit
	struct PerFrameTransfers {
		uint32_t count;
		eastl::array<QbVkBuffer, MAX_TRANSFERS_PER_FRAME> stagingBuffers{};
		eastl::array<QbVkTransfer, MAX_TRANSFERS_PER_FRAME> transfers{};
		VkCommandBuffer commandBuffer;

		PerFrameTransfers(const QbVkContext& context);

		QbVkTransfer& operator[](int index) {
			return transfers[index];
		}

		void Reset() { 
			count = 0; 
		}
	};

	class QbVkResourceManager {
	public:
		QbVkResourceManager(QbVkContext& context);
		~QbVkResourceManager();

		void TransferDataToGPU(const void* data, VkDeviceSize size, QbVkBufferHandle destination);
		bool TransferQueuedDataToGPU(uint32_t resourceIndex);

		QbVkBufferHandle CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage);
		QbVkBufferHandle CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount);
		QbVkBufferHandle CreateIndexBuffer(const eastl::vector<uint32_t>& indices);

		QbVkTextureHandle CreateTexture(uint32_t width, uint32_t height, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle CreateTexture(VkImageCreateInfo* imageInfo, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle CreateStorageTexture(uint32_t width, uint32_t height, VkFormat format, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle LoadTexture(uint32_t width, uint32_t height, const void* data, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle LoadTexture(const char* imagePath, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle GetEmptyTexture();

		QbVkDescriptorAllocatorHandle CreateDescriptorAllocator(const eastl::vector<VkDescriptorSetLayout>& setLayouts,
			const eastl::vector<VkDescriptorPoolSize>& poolSizes, const uint32_t maxInstances);
		QbVkDescriptorSetsHandle GetNextDescriptorSetsHandle(const QbVkDescriptorAllocatorHandle allocatorHandle);
		const eastl::array<eastl::vector<VkDescriptorSet>, MAX_FRAMES_IN_FLIGHT>& GetDescriptorSets(
			const QbVkDescriptorAllocatorHandle allocatorHandle, const QbVkDescriptorSetsHandle setsHandle);
		void WriteDescriptor(QbVkDescriptorAllocatorHandle allocatorHandle, QbVkDescriptorSetsHandle descriptorSetsHandle,
			QbVkBufferHandle bufferHandle, VkDescriptorType descriptorType, uint32_t set, uint32_t binding, uint32_t descriptorCount);
		void WriteDescriptor(QbVkDescriptorAllocatorHandle allocatorHandle, QbVkDescriptorSetsHandle descriptorSetsHandle,
			QbVkTextureHandle textureHandle, VkDescriptorType descriptorType, uint32_t set, uint32_t binding, uint32_t descriptorCount);

		template<typename T>
		void DestroyResource(QbVkResourceHandle<T> handle) {
			if constexpr (eastl::is_same<T, QbVkBuffer>::value) {
				context_.allocator->DestroyBuffer(buffers_[handle]);
				buffers_.DestroyResource(handle);
			}
			else if constexpr (eastl::is_same<T, QbVkTexture>::value) {
				QbVkTexture& texture = textures_[handle];
				context_.allocator->DestroyImage(texture.image);
				if (texture.descriptor.imageView != VK_NULL_HANDLE) vkDestroyImageView(context_.device, texture.descriptor.imageView, nullptr);
				if (texture.descriptor.sampler != VK_NULL_HANDLE) vkDestroySampler(context_.device, texture.descriptor.sampler, nullptr);
				textures_.DestroyResource(handle);
			}
			else if constexpr (eastl::is_same<T, QbVkDescriptorAllocator>::value) {
				QbVkDescriptorAllocator& allocator = descriptorAllocators_[handle];
				vkDestroyDescriptorPool(context_.device, allocator.pool, nullptr);
				descriptorAllocators_.DestroyResource(handle);
			}
		}

		template<typename T, typename U>
		U* GetDescriptorPtr(QbVkResourceHandle<T> handle) {
			if constexpr (eastl::is_same<T, QbVkBuffer>::value) {
				return &buffers_[handle].descriptor;
			}
			else if constexpr (eastl::is_same<T, QbVkTexture>::value) {
				return &textures_[handle].descriptor;
			}
		}

		QbVkResource<QbVkBuffer, MAX_BUFFER_COUNT> buffers_;
		QbVkResource<QbVkTexture, MAX_TEXTURE_COUNT> textures_;
		QbVkResource<QbVkDescriptorAllocator, MAX_DESCRIPTOR_INSTANCES> descriptorAllocators_;
		QbVkResource<eastl::unique_ptr<QbVkPipeline>, MAX_PIPELINES> pipelines_;

	private:
		QbVkContext& context_;
		PerFrameTransfers transferQueue_;
		QbVkTextureHandle emptyTexture_ = QBVK_TEXTURE_NULL_HANDLE;
	};
}