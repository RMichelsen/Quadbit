#pragma once

#include <glm/glm.hpp>
#include <EASTL/array.h>
#include <EASTL/vector.h>

#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class QbVkRenderer;
	class QbVkShaderInstance;
	struct Entity;

	class Graphics {
	public:
		Graphics(QbVkRenderer* const renderer);

		/***************/
		/*   General   */
		/***************/
		float GetAspectRatio();
		void LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour);
		template<typename T>
		VkDescriptorBufferInfo* GetDescriptorPtr(QbVkMappedBuffer<T> mappedBuffer) {
			return GetDescriptorPtr(mappedBuffer.handle);
		}
		VkDescriptorBufferInfo* GetDescriptorPtr(QbVkBufferHandle handle);
		VkDescriptorImageInfo* GetDescriptorPtr(QbVkTextureHandle handle);

		/**************/
		/*    GPU     */
		/**************/
		QbVkBufferHandle CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage = QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		template<typename T>
		QbVkMappedBuffer<T> CreateMappedGPUBuffer(VkBufferUsageFlags bufferUsage) {
			QbVkBufferHandle handle = CreateGPUBuffer(sizeof(T), bufferUsage, QbVkMemoryUsage::QBVK_MEMORY_USAGE_CPU_TO_GPU);
			return QbVkMappedBuffer<T>{ handle, reinterpret_cast<T*>(GetMappedGPUData(handle)) };
		}
		void TransferDataToGPUBuffer(const void* data, VkDeviceSize size, QbVkBufferHandle destination);
		VkMemoryBarrier CreateMemoryBarrier(VkAccessFlags srcMask, VkAccessFlags dstMask);
		void DestroyBuffer(QbVkBuffer buffer);

		/******************************/
		/*    Images and Textures     */
		/******************************/
		QbVkTextureHandle CreateTexture(uint32_t width, uint32_t height, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle CreateStorageTexture(uint32_t width, uint32_t height, VkFormat format, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle LoadTexture(const char* imagePath, VkSamplerCreateInfo* samplerInfo = nullptr);
		void DestroyTexture(QbVkTexture texture);

		VkSamplerCreateInfo CreateImageSamplerInfo(VkFilter samplerFilter, VkSamplerAddressMode addressMode, VkBool32 enableAnisotropy,
			float maxAnisotropy, VkCompareOp compareOperation, VkSamplerMipmapMode samplerMipmapMode, float maxLod = 0.0f);

		/*****************************/
		/*    Objects and Meshes     */
		/*****************************/
		Entity GetActiveCamera();
		void RegisterCamera(Entity entity);
		QbVkShaderInstance CreateShaderInstance();
		const QbVkRenderMeshInstance* CreateRenderMeshInstance(eastl::vector<QbVkRenderDescriptor>& descriptors,
			eastl::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance,
			int pushConstantStride = -1, VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT);
		const QbVkRenderMeshInstance* CreateRenderMeshInstance(eastl::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance,
			int pushConstantStride = -1, VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT);
		void DestroyMesh(RenderMeshComponent& renderMeshComponent);
		QbVkBufferHandle CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount);
		QbVkBufferHandle CreateIndexBuffer(const eastl::vector<uint32_t>& indices);
		QbVkRenderDescriptor CreateRenderDescriptor(VkDescriptorType type, void* descriptor, VkShaderStageFlagBits shaderStage);
		PBRSceneComponent LoadPBRModel(const char* path);

		template<typename T>
		RenderMeshComponent CreateMesh(const eastl::vector<T>& vertices, uint32_t vertexStride, const eastl::vector<uint32_t>& indices, const QbVkRenderMeshInstance* externalInstance,
			int pushConstantStride = -1) {

			return RenderMeshComponent{
				CreateVertexBuffer(vertices.data(), vertexStride, static_cast<uint32_t>(vertices.size())),
				CreateIndexBuffer(indices),
				static_cast<uint32_t>(indices.size()),
				eastl::array<float, 32>(),
				pushConstantStride,
				externalInstance
			};
		}
		template<typename T>
		QbVkRenderDescriptor CreateRenderDescriptor(VkDescriptorType type, eastl::vector<T>& data, VkShaderStageFlagBits shaderStage) {
			return { type, static_cast<uint32_t>(data.size()), data.data(), shaderStage };
		}

	private:
		QbVkRenderer* const renderer_;

		void* GetMappedGPUData(QbVkBufferHandle handle);
	};
}