#pragma once

#include <glm/glm.hpp>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>

#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Rendering/Memory/ResourceManager.h"
#include "Engine/Rendering/Pipelines/PipelinePresets.h"

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
		float GetSunAzimuth();
		float GetSunAltitude();
		void LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour);

		/**************/
		/*    GPU     */
		/**************/
		QbVkBufferHandle CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage = QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		void TransferDataToGPUBuffer(const void* data, VkDeviceSize size, QbVkBufferHandle destination);

		template<typename T>
		QbVkUniformBuffer<T> CreateUniformBuffer() {
			return renderer_->context_->resourceManager->CreateUniformBuffer<T>();
		}

		template<typename T>
		T* const GetUniformBufferPtr(QbVkUniformBuffer<T>& ubo) {
			return renderer_->context_->resourceManager->GetUniformBufferPtr<T>(ubo);
		}

		// This function initializes all the mapped buffer instances
		// of the UBO to values given by the struct T
		template<typename T>
		void InitializeUBO(QbVkUniformBuffer<T>& ubo, const T* t) {
			renderer_->context_->resourceManager->InitializeUBO(ubo, t);
		}

		/******************************/
		/*    Images and Textures     */
		/******************************/
		QbVkTextureHandle CreateTexture(uint32_t width, uint32_t height, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle CreateStorageTexture(uint32_t width, uint32_t height, VkFormat format, VkSamplerCreateInfo* samplerInfo = nullptr);
		QbVkTextureHandle LoadTexture(const char* imagePath, VkSamplerCreateInfo* samplerInfo = nullptr);

		VkSamplerCreateInfo CreateImageSamplerInfo(VkFilter samplerFilter, VkSamplerAddressMode addressMode, VkBool32 enableAnisotropy,
			float maxAnisotropy, VkCompareOp compareOperation, VkSamplerMipmapMode samplerMipmapMode, float maxLod = 0.0f);

		/*****************************/
		/*    Objects and Meshes     */
		/*****************************/
		Entity GetActiveCamera();
		void RegisterCamera(Entity entity);
		QbVkShaderInstance CreateShaderInstance();
		QbVkBufferHandle CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount);
		QbVkBufferHandle CreateIndexBuffer(const eastl::vector<uint32_t>& indices);
		QbVkPipelineHandle CreatePipeline(const char* vertexPath, const char* vertexEntry, const char* fragmentPath, const char* fragmentEntry,
			const QbVkPipelineDescription pipelineDescription, const VkRenderPass renderPass = VkRenderPass(-1), const uint32_t maxInstances = 1, 
			const eastl::vector<eastl::tuple<VkFormat, uint32_t>>& vertexAttributeOverride = {});

		void BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name,
			const QbVkBufferHandle bufferHandle, const QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);
		void BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name,
			const QbVkTextureHandle textureHandle, QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);
		void BindResourceArray(const QbVkPipelineHandle pipelineHandle, const eastl::string name,
			const eastl::vector<QbVkBufferHandle> bufferHandles, const QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);
		void BindResourceArray(const QbVkPipelineHandle pipelineHandle, const eastl::string name,
			const eastl::vector<QbVkTextureHandle> textureHandles, const QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);

		PBRSceneComponent LoadPBRModel(const char* path);

		template<typename T>
		CustomMeshComponent CreateMesh(const eastl::vector<T>& vertices, uint32_t vertexStride, 
			const eastl::vector<uint32_t>& indices, QbVkPipelineHandle pipelineHandle,
			int pushConstantStride = -1, QbVkDescriptorSetsHandle descriptorSets = QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {

			return CustomMeshComponent{
				CreateVertexBuffer(vertices.data(), vertexStride, static_cast<uint32_t>(vertices.size())),
				CreateIndexBuffer(indices),
				static_cast<uint32_t>(indices.size()),
				eastl::array<float, 32>(),
				pushConstantStride,
				pipelineHandle,
				descriptorSets
			};
		}
		void DestroyMesh(const Entity& entity);

	private:
		QbVkRenderer* const renderer_;
		QbVkResourceManager* const resourceManager_;
	};
}