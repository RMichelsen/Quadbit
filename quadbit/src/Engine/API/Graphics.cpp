#include "Graphics.h"

#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/Renderer.h"
#include "Engine/Rendering/ShaderInstance.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Pipelines/MeshPipeline.h"
#include "Engine/Rendering/Memory/ResourceManager.h"

namespace Quadbit {
	Graphics::Graphics(QbVkRenderer* const renderer) : renderer_(renderer) { }

#pragma region General
	float Graphics::GetAspectRatio() {
		return static_cast<float>(renderer_->context_->swapchain.extent.width) / static_cast<float>(renderer_->context_->swapchain.extent.height);
	}

	void Graphics::LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour) {
		renderer_->meshPipeline_->LoadSkyGradient(botColour, topColour);
	}

	VkDescriptorBufferInfo* Graphics::GetDescriptorPtr(QbVkBufferHandle handle) {
		return renderer_->context_->resourceManager->GetDescriptorPtr<QbVkBuffer, VkDescriptorBufferInfo>(handle);
	}

	VkDescriptorImageInfo* Graphics::GetDescriptorPtr(QbVkTextureHandle handle) {
		return renderer_->context_->resourceManager->GetDescriptorPtr<QbVkTexture, VkDescriptorImageInfo>(handle);
	}
#pragma endregion

#pragma region GPU
	QbVkBufferHandle Graphics::CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage) {
		return renderer_->context_->resourceManager->CreateGPUBuffer(size, bufferUsage, memoryUsage);
	}

	void Graphics::TransferDataToGPUBuffer(const void* data, VkDeviceSize size, QbVkBufferHandle destination) {
		auto& buffer = renderer_->context_->resourceManager->buffers_[destination];
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
		return renderer_->context_->resourceManager->CreateTexture(width, height, samplerInfo);
	}

	QbVkTextureHandle Graphics::CreateStorageTexture(uint32_t width, uint32_t height, VkFormat format, VkSamplerCreateInfo* samplerInfo) {
		return renderer_->context_->resourceManager->CreateStorageTexture(width, height, format, samplerInfo);
	}

	QbVkTextureHandle Graphics::LoadTexture(const char* imagePath, VkSamplerCreateInfo* samplerInfo) {
		return renderer_->context_->resourceManager->LoadTexture(imagePath, samplerInfo);
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

	const QbVkRenderMeshInstance* Graphics::CreateRenderMeshInstance(eastl::vector<QbVkRenderDescriptor>& descriptors,
		eastl::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance, int pushConstantStride, VkShaderStageFlags pushConstantShaderStage) {
		return renderer_->meshPipeline_->CreateInstance(descriptors, vertexAttribs, shaderInstance, pushConstantStride, pushConstantShaderStage);
	}

	const QbVkRenderMeshInstance* Graphics::CreateRenderMeshInstance(eastl::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance,
		int pushConstantStride, VkShaderStageFlags pushConstantShaderStage) {
		eastl::vector<QbVkRenderDescriptor> empty{};
		return renderer_->meshPipeline_->CreateInstance(empty, vertexAttribs, shaderInstance, pushConstantStride, pushConstantShaderStage);
	}

	void Graphics::DestroyMesh(RenderMeshComponent& renderMeshComponent) {
		renderer_->meshPipeline_->DestroyMesh(renderMeshComponent);
	}

	QbVkBufferHandle Graphics::CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount) {
		return renderer_->context_->resourceManager->CreateVertexBuffer(vertices, vertexStride, vertexCount);
	}

	QbVkBufferHandle Graphics::CreateIndexBuffer(const eastl::vector<uint32_t>& indices) {
		return renderer_->context_->resourceManager->CreateIndexBuffer(indices);
	}

	QbVkRenderDescriptor Graphics::CreateRenderDescriptor(VkDescriptorType type, void* descriptor, VkShaderStageFlagBits shaderStage) {
		return { type, 1, descriptor, shaderStage };
	}

	PBRModelComponent Graphics::LoadPBRModel(const char* path) {
		return renderer_->context_->resourceManager->LoadModel(path);
	}

	void* Graphics::GetMappedGPUData(QbVkBufferHandle handle) {
		assert(renderer_->context_->resourceManager->buffers_[handle].alloc.data != nullptr && "GPUBuffer data not mapped!");
		return renderer_->context_->resourceManager->buffers_[handle].alloc.data;
	}
#pragma endregion

}