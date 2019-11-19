#pragma once

#include <cstdint>

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>
#include <EASTL/type_traits.h>

#include <tinygltf/tiny_gltf.h>

#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Rendering/Memory/Allocator.h"

constexpr size_t MAX_TRANSFERS_PER_FRAME = 1024;
constexpr size_t MAX_BUFFER_COUNT = 65536;
constexpr size_t MAX_TEXTURE_COUNT = 512;
constexpr size_t MAX_DESCRIPTORSET_COUNT = 2048;

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

		QbVkBufferHandle CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage);

		void TransferDataToGPU(const void* data, VkDeviceSize size, QbVkBufferHandle destination);
		QbVkBufferHandle CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount);
		QbVkBufferHandle CreateIndexBuffer(const eastl::vector<uint32_t>& indices);

		QbVkTextureHandle CreateStorageTexture(uint32_t width, uint32_t height, VkFormat format, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle CreateTexture(uint32_t width, uint32_t height, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle LoadTexture(uint32_t width, uint32_t height, const void* data, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle LoadTexture(const char* imagePath, VkSamplerCreateInfo* samplerInfo = nullptr);

		PBRModelComponent LoadModel(const char* path);

		bool TransferQueuedDataToGPU(uint32_t resourceIndex);

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
			else if constexpr (eastl::is_same<T, VkDescriptorSet>::value) {
				materialDescriptorSets_.DestroyResource(handle);
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
		QbVkResource<VkDescriptorSet, MAX_DESCRIPTORSET_COUNT> materialDescriptorSets_;
		VkDescriptorPool materialDescriptorPool_;
		VkDescriptorSetLayout materialDescriptorSetLayout_;

	private:
		QbVkContext& context_;
		PerFrameTransfers transferQueue_;
		QbVkTextureHandle emptyTexture_;

		VkSamplerCreateInfo GetSamplerInfo(const tinygltf::Model& model, int samplerIndex);
		QbVkDescriptorSetHandle GetDescriptorSetHandle(QbVkMaterial& material);
		QbVkTextureHandle CreateTextureFromResource(const tinygltf::Model& model, const tinygltf::Material& material, const char* textureName);
		QbVkMaterial ParseMaterial(const tinygltf::Model& model, const tinygltf::Material& material);
	};
}