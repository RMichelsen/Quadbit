#include "Graphics.h"

#include "Engine/Core/Logging.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/Renderer.h"
#include "Engine/Rendering/Shaders/ShaderInstance.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Pipelines/Pipeline.h"
#include "Engine/Rendering/Pipelines/PBRPipeline.h"
#include "Engine/Rendering/Memory/ResourceManager.h"

namespace Quadbit {
	Graphics::Graphics(QbVkRenderer* const renderer) : renderer_(renderer), resourceManager_(renderer->context_->resourceManager.get()) { }

#pragma region General
	float Graphics::GetAspectRatio() {
		return static_cast<float>(renderer_->context_->swapchain.extent.width) / static_cast<float>(renderer_->context_->swapchain.extent.height);
	}

	float Graphics::GetSunAzimuth() {
		return renderer_->context_->sunAzimuth;
	}

	float Graphics::GetSunAltitude() {
		return renderer_->context_->sunAltitude;
	}

	void Graphics::LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour) {
		renderer_->pbrPipeline_->LoadSkyGradient(botColour, topColour);
	}

#pragma endregion

#pragma region GPU
	QbVkBufferHandle Graphics::CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage) {
		return resourceManager_->CreateGPUBuffer(size, bufferUsage, memoryUsage);
	}

	void Graphics::TransferDataToGPUBuffer(const void* data, VkDeviceSize size, QbVkBufferHandle destination) {
		auto& buffer = resourceManager_->buffers_[destination];
		VkUtils::TransferDataToGPUBuffer(*renderer_->context_, data, size, buffer);
	}
#pragma endregion

#pragma region Images and Textures
	QbVkTextureHandle Graphics::CreateTexture(uint32_t width, uint32_t height, VkSamplerCreateInfo* samplerInfo) {
		return resourceManager_->CreateTexture(width, height, samplerInfo);
	}

	QbVkTextureHandle Graphics::CreateStorageTexture(uint32_t width, uint32_t height, VkFormat format, VkSamplerCreateInfo* samplerInfo) {
		return resourceManager_->CreateStorageTexture(width, height, format, samplerInfo);
	}

	QbVkTextureHandle Graphics::LoadTexture(const char* imagePath, VkSamplerCreateInfo* samplerInfo) {
		return resourceManager_->LoadTexture(imagePath, samplerInfo);
	}

	VkSamplerCreateInfo Graphics::CreateImageSamplerInfo(VkFilter samplerFilter, VkSamplerAddressMode addressMode, VkBool32 enableAnisotropy,
		float maxAnisotropy, VkCompareOp compareOperation, VkSamplerMipmapMode samplerMipmapMode, float maxLod) {
		auto samplerInfo = VkUtils::Init::SamplerCreateInfo(samplerFilter, addressMode, enableAnisotropy, maxAnisotropy, compareOperation, samplerMipmapMode, maxLod);
		return samplerInfo;
	}
#pragma endregion

#pragma region Objects and Meshes
	Entity Graphics::GetActiveCamera() {
		return renderer_->context_->GetActiveCamera();
	}

	void Graphics::RegisterCamera(Entity entity) {
		if (!renderer_->context_->entityManager->HasComponent<RenderCamera>(entity)) {
			QB_LOG_ERROR("Cannot register camera: Entity must have the Quadbit::RenderCamera component\n");
			return;
		}
		renderer_->context_->SetCamera(entity);
	}

	QbVkShaderInstance Graphics::CreateShaderInstance() {
		return QbVkShaderInstance(*renderer_->context_);
	}

	QbVkBufferHandle Graphics::CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount) {
		return resourceManager_->CreateVertexBuffer(vertices, vertexStride, vertexCount);
	}

	QbVkBufferHandle Graphics::CreateIndexBuffer(const eastl::vector<uint32_t>& indices) {
		return resourceManager_->CreateIndexBuffer(indices);
	}

	// Max instances here refers to the maximum number shader resource instances
	QbVkPipelineHandle Graphics::CreatePipeline(const char* vertexPath, const char* vertexEntry, const char* fragmentPath, const char* fragmentEntry,
		const QbVkPipelineDescription pipelineDescription, const VkRenderPass renderPass, const uint32_t maxInstances, 
		const eastl::vector<eastl::tuple<VkFormat, uint32_t>>& vertexAttributeOverride) {

		auto handle = resourceManager_->pipelines_.GetNextHandle();
		resourceManager_->pipelines_[handle] = eastl::make_unique<QbVkPipeline>(*renderer_->context_, vertexPath, vertexEntry,
			fragmentPath, fragmentEntry, pipelineDescription, renderPass == VkRenderPass(-1) ? renderer_->context_->mainRenderPass : renderPass, maxInstances, vertexAttributeOverride);
		
		auto& pipeline = resourceManager_->pipelines_[handle];
		pipeline->Rebuild();

		return handle;
	}

	void Graphics::BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name,
		const QbVkBufferHandle bufferHandle, const QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
			pipeline->BindResource(descriptorsHandle, name, bufferHandle);
		}
		else {
			pipeline->BindResource(name, bufferHandle);
		}
	}

	void Graphics::BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name,
		const QbVkTextureHandle textureHandle, const QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
			pipeline->BindResource(descriptorsHandle, name, textureHandle);
		}
		else {
			pipeline->BindResource(name, textureHandle);
		}
	}

	void Graphics::BindResourceArray(const QbVkPipelineHandle pipelineHandle, const eastl::string name,
		const eastl::vector<QbVkBufferHandle> bufferHandles, const QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
			pipeline->BindResourceArray(descriptorsHandle, name, bufferHandles);
		}
		else {
			pipeline->BindResourceArray(name, bufferHandles);
		}
	}

	void Graphics::BindResourceArray(const QbVkPipelineHandle pipelineHandle, const eastl::string name,
		const eastl::vector<QbVkTextureHandle> textureHandles, const QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
			pipeline->BindResourceArray(descriptorsHandle, name, textureHandles);
		}
		else {
			pipeline->BindResourceArray(name, textureHandles);
		}
	}

	PBRSceneComponent Graphics::LoadPBRModel(const char* path) {
		return renderer_->pbrPipeline_->LoadModel(path);
	}

	void Graphics::DestroyMesh(const Entity& entity) {
		const auto& entityManager = renderer_->context_->entityManager;
		QB_ASSERT(entityManager->HasComponent<CustomMeshComponent>(entity));
		const auto& mesh = entityManager->GetComponentPtr<CustomMeshComponent>(entity);
		entityManager->AddComponent<CustomMeshDeleteComponent>(
			entityManager->Create(),
			{ mesh->vertexHandle, mesh->indexHandle }
		);
		entityManager->RemoveComponent<CustomMeshComponent>(entity);
	}
#pragma endregion
}