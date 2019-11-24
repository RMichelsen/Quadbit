#include "Graphics.h"

#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/Renderer.h"
#include "Engine/Rendering/ShaderInstance.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Pipeline.h"
#include "Engine/Rendering/Pipelines/MeshPipeline.h"
#include "Engine/Rendering/Memory/ResourceManager.h"

namespace Quadbit {
	Graphics::Graphics(QbVkRenderer* const renderer) : renderer_(renderer), resourceManager_(renderer->context_->resourceManager.get()) { }

#pragma region General
	float Graphics::GetAspectRatio() {
		return static_cast<float>(renderer_->context_->swapchain.extent.width) / static_cast<float>(renderer_->context_->swapchain.extent.height);
	}

	void Graphics::LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour) {
		renderer_->meshPipeline_->LoadSkyGradient(botColour, topColour);
	}

	VkDescriptorBufferInfo* Graphics::GetDescriptorPtr(QbVkBufferHandle handle) {
		return resourceManager_->GetDescriptorPtr<QbVkBuffer, VkDescriptorBufferInfo>(handle);
	}

	VkDescriptorImageInfo* Graphics::GetDescriptorPtr(QbVkTextureHandle handle) {
		return resourceManager_->GetDescriptorPtr<QbVkTexture, VkDescriptorImageInfo>(handle);
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

	VkMemoryBarrier Graphics::CreateMemoryBarrier(VkAccessFlags srcMask, VkAccessFlags dstMask) {
		return VkUtils::Init::MemoryBarrierVk(srcMask, dstMask);
	}

	void Graphics::DestroyBuffer(QbVkBuffer buffer) {
		renderer_->DestroyResource(buffer);
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

	void Graphics::DestroyTexture(QbVkTexture texture) {
		renderer_->DestroyResource(texture);
	}

	VkSamplerCreateInfo Graphics::CreateImageSamplerInfo(VkFilter samplerFilter, VkSamplerAddressMode addressMode, VkBool32 enableAnisotropy,
		float maxAnisotropy, VkCompareOp compareOperation, VkSamplerMipmapMode samplerMipmapMode, float maxLod) {
		auto samplerInfo = VkUtils::Init::SamplerCreateInfo(samplerFilter, addressMode, enableAnisotropy, maxAnisotropy, compareOperation, samplerMipmapMode, maxLod);
		return samplerInfo;
	}
#pragma endregion

#pragma region Objects and Meshes
	Entity Graphics::GetActiveCamera() {
		return renderer_->meshPipeline_->GetActiveCamera();
	}

	void Graphics::RegisterCamera(Entity entity) {
		if (!renderer_->context_->entityManager->HasComponent<RenderCamera>(entity)) {
			QB_LOG_ERROR("Cannot register camera: Entity must have the Quadbit::RenderCamera component\n");
			return;
		}
		renderer_->meshPipeline_->SetCamera(entity);
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
	QbVkPipelineHandle Graphics::CreatePipeline(const uint32_t* vertexBytecode, uint32_t vertexSize,
		const uint32_t* fragmentBytecode, uint32_t fragmentSize, const QbVkPipelineDescription pipelineDescription, 
		const uint32_t maxInstances, const eastl::vector<eastl::tuple<VkFormat, uint32_t>>& vertexAttributeOverride) {

		auto handle = resourceManager_->pipelines_.GetNextHandle();
		resourceManager_->pipelines_[handle] = eastl::make_unique<QbVkPipeline>(*renderer_->context_, vertexBytecode, vertexSize,
			fragmentBytecode, fragmentSize, pipelineDescription, maxInstances, vertexAttributeOverride);

		return handle;
	}

	QbVkPipelineHandle Graphics::CreatePipeline(const char* vertexPath, const char* fragmentPath, const QbVkPipelineDescription pipelineDescription, 
		const uint32_t maxInstances, const eastl::vector<eastl::tuple<VkFormat, uint32_t>>& vertexAttributeOverride) {
		auto vertexBytecode = VkUtils::ReadShader(vertexPath);
		auto fragmentBytecode = VkUtils::ReadShader(fragmentPath);

		return CreatePipeline(reinterpret_cast<uint32_t*>(vertexBytecode.data()), vertexBytecode.size() / sizeof(uint32_t),
			reinterpret_cast<uint32_t*>(fragmentBytecode.data()), fragmentBytecode.size() / sizeof(uint32_t), pipelineDescription,
			maxInstances, vertexAttributeOverride);
	}

	void Graphics::BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name, const QbVkBufferHandle bufferHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		pipeline->BindResource(name, bufferHandle);
	}

	void Graphics::BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name, const QbVkTextureHandle textureHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		pipeline->BindResource(name, textureHandle);
	}

	PBRSceneComponent Graphics::LoadPBRModel(const char* path) {
		return renderer_->meshPipeline_->LoadModel(path);
	}

	void Graphics::DestroyMesh(const Entity& entity) {
		const auto& entityManager = renderer_->context_->entityManager;
		assert(entityManager->HasComponent<CustomMeshComponent>(entity));
		const auto& mesh = entityManager->GetComponentPtr<CustomMeshComponent>(entity);
		entityManager->AddComponent<CustomMeshDeleteComponent>(
			entityManager->Create(),
			{ mesh->vertexHandle, mesh->indexHandle }
		);
		entityManager->RemoveComponent<CustomMeshComponent>(entity);
	}

	void* Graphics::GetMappedGPUData(QbVkBufferHandle handle) {
		assert(resourceManager_->buffers_[handle].alloc.data != nullptr && "GPUBuffer data not mapped!");
		return resourceManager_->buffers_[handle].alloc.data;
	}
#pragma endregion

}