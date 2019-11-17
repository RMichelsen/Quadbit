#pragma once

#include <glm/glm.hpp>

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


		/**************/
		/*    GPU     */
		/**************/
		QbGPUBuffer CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage = QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		template<typename T>
		QbMappedGPUBuffer<T> CreateMappedGPUBuffer(VkBufferUsageFlags bufferUsage) {
			QbGPUBuffer buffer = CreateGPUBuffer(sizeof(T), bufferUsage, QbVkMemoryUsage::QBVK_MEMORY_USAGE_CPU_TO_GPU);
			return QbMappedGPUBuffer<T>{ buffer.handle, reinterpret_cast<T*>(GetMappedGPUData(buffer.handle)), buffer.descriptor };
		}
		void TransferDataToGPUBuffer(const void* data, VkDeviceSize size, QbGPUBuffer destination);
		VkMemoryBarrier CreateMemoryBarrier(VkAccessFlags srcMask, VkAccessFlags dstMask);
		void DestroyBuffer(QbVkBuffer buffer);

		/******************************/
		/*    Images and Textures     */
		/******************************/
		QbTexture CreateTexture(uint32_t width, uint32_t height, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
			VkImageAspectFlags imageAspectFlags, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage, QbVkMemoryUsage memoryUsage,
			VkSampler sampler = VK_NULL_HANDLE, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
		QbTexture LoadTexture(const char* imagePath, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
			VkImageAspectFlags imageAspectFlags, QbVkMemoryUsage memoryUsage, VkSamplerCreateInfo* samplerCreateInfo = nullptr,
			VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
		void DestroyTexture(QbVkTexture texture);

		VkSampler CreateImageSampler(VkFilter samplerFilter, VkSamplerAddressMode addressMode, VkBool32 enableAnisotropy,
			float maxAnisotropy, VkCompareOp compareOperation, VkSamplerMipmapMode samplerMipmapMode, float maxLod = 0.0f);

		/*****************************/
		/*    Objects and Meshes     */
		/*****************************/
		Entity GetActiveCamera();
		void RegisterCamera(Entity entity);
		QbVkShaderInstance CreateShaderInstance();
		const QbVkRenderMeshInstance* CreateRenderMeshInstance(std::vector<QbVkRenderDescriptor>& descriptors,
			std::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance,
			int pushConstantStride = -1, VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT);
		const QbVkRenderMeshInstance* CreateRenderMeshInstance(std::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance,
			int pushConstantStride = -1, VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT);
		RenderTexturedObjectComponent CreateObject(const char* objPath, const char* texturePath, VkFormat textureFormat);
		RenderMeshComponent CreateMesh(const char* objPath, std::vector<QbVkVertexInputAttribute> vertexModel, const QbVkRenderMeshInstance* externalInstance,
			int pushConstantStride = -1);
		void DestroyMesh(RenderMeshComponent& renderMeshComponent);
		QbVkBufferHandle CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount);
		QbVkBufferHandle CreateIndexBuffer(const std::vector<uint32_t>& indices);
		QbVkRenderDescriptor CreateRenderDescriptor(VkDescriptorType type, void* descriptor, VkShaderStageFlagBits shaderStage);

		template<typename T>
		RenderMeshComponent CreateMesh(std::vector<T> vertices, uint32_t vertexStride, const std::vector<uint32_t>& indices, const QbVkRenderMeshInstance* externalInstance,
			int pushConstantStride = -1) {

			return RenderMeshComponent{
				CreateVertexBuffer(vertices.data(), vertexStride, static_cast<uint32_t>(vertices.size())),
				CreateIndexBuffer(indices),
				static_cast<uint32_t>(indices.size()),
				std::array<float, 32>(),
				pushConstantStride,
				externalInstance
			};
		}
		template<typename T>
		QbVkRenderDescriptor CreateRenderDescriptor(VkDescriptorType type, std::vector<T>& data, VkShaderStageFlagBits shaderStage) {
			return { type, static_cast<uint32_t>(data.size()), data.data(), shaderStage };
		}

	private:
		QbVkRenderer* const renderer_;

		void* GetMappedGPUData(QbVkBufferHandle handle);
	};
}