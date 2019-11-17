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
#pragma endregion

#pragma region GPU
	QbGPUBuffer Graphics::CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage) {
		return VkUtils::CreateGPUBuffer(*renderer_->context_, size, bufferUsage, memoryUsage);
	}

	void Graphics::TransferDataToGPUBuffer(const void* data, VkDeviceSize size, QbGPUBuffer destination) {
		auto& buffer = renderer_->context_->resourceManager->buffers[destination.handle];
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
	QbTexture Graphics::CreateTexture(uint32_t width, uint32_t height, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage,
		VkImageLayout imageLayout, VkImageAspectFlags imageAspectFlags, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage, QbVkMemoryUsage memoryUsage,
		VkSampler sampler, VkSampleCountFlagBits numSamples) {
		return VkUtils::CreateTexture(*renderer_->context_, width, height, imageFormat, imageTiling, imageUsage,
			imageLayout, imageAspectFlags, srcStage, dstStage, memoryUsage, sampler, numSamples);
	}

	QbTexture Graphics::LoadTexture(const char* imagePath, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
		VkImageAspectFlags imageAspectFlags, QbVkMemoryUsage memoryUsage, VkSamplerCreateInfo* samplerCreateInfo, VkSampleCountFlagBits numSamples) {
		return VkUtils::LoadTexture(*renderer_->context_, imagePath, imageFormat, imageTiling, imageUsage, 
			imageLayout, imageAspectFlags, memoryUsage, samplerCreateInfo, numSamples);
	}

	void Graphics::DestroyTexture(QbVkTexture texture) {
		renderer_->DestroyResource(texture);
	}

	VkSampler Graphics::CreateImageSampler(VkFilter samplerFilter, VkSamplerAddressMode addressMode, VkBool32 enableAnisotropy,
		float maxAnisotropy, VkCompareOp compareOperation, VkSamplerMipmapMode samplerMipmapMode, float maxLod) {
		VkSampler sampler;
		auto createInfo = VkUtils::Init::SamplerCreateInfo(samplerFilter, addressMode, enableAnisotropy, maxAnisotropy, compareOperation, samplerMipmapMode, maxLod);
		VK_CHECK(vkCreateSampler(renderer_->context_->device, &createInfo, nullptr, &sampler));
		return sampler;
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

	const QbVkRenderMeshInstance* Graphics::CreateRenderMeshInstance(std::vector<QbVkRenderDescriptor>& descriptors,
		std::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance, int pushConstantStride, VkShaderStageFlags pushConstantShaderStage) {
		return renderer_->meshPipeline_->CreateInstance(descriptors, vertexAttribs, shaderInstance, pushConstantStride, pushConstantShaderStage);
	}

	const QbVkRenderMeshInstance* Graphics::CreateRenderMeshInstance(std::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance,
		int pushConstantStride, VkShaderStageFlags pushConstantShaderStage) {
		std::vector<QbVkRenderDescriptor> empty{};
		return renderer_->meshPipeline_->CreateInstance(empty, vertexAttribs, shaderInstance, pushConstantStride, pushConstantShaderStage);
	}

	RenderTexturedObjectComponent Graphics::CreateObject(const char* objPath, const char* texturePath, VkFormat textureFormat) {
		return renderer_->meshPipeline_->CreateObject(objPath, texturePath, textureFormat);
	}

	RenderMeshComponent Graphics::CreateMesh(const char* objPath, std::vector<QbVkVertexInputAttribute> vertexModel,
		const QbVkRenderMeshInstance* externalInstance, int pushConstantStride) {
		QbVkModel model = VkUtils::LoadModel(objPath, vertexModel);

		return RenderMeshComponent{
			renderer_->meshPipeline_->CreateVertexBuffer(model.vertices.data(), model.vertexStride, static_cast<uint32_t>(model.vertices.size())),
			renderer_->meshPipeline_->CreateIndexBuffer(model.indices),
			static_cast<uint32_t>(model.indices.size()),
			std::array<float, 32>(),
			pushConstantStride,
			externalInstance
		};
	}

	void Graphics::DestroyMesh(RenderMeshComponent& renderMeshComponent) {
		renderer_->meshPipeline_->DestroyMesh(renderMeshComponent);
	}

	QbVkBufferHandle Graphics::CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount) {
		return renderer_->meshPipeline_->CreateVertexBuffer(vertices, vertexStride, vertexCount);
	}

	QbVkBufferHandle Graphics::CreateIndexBuffer(const std::vector<uint32_t>& indices)
	{
		return renderer_->meshPipeline_->CreateIndexBuffer(indices);
	}

	QbVkRenderDescriptor Graphics::CreateRenderDescriptor(VkDescriptorType type, void* descriptor, VkShaderStageFlagBits shaderStage) {
		return { type, 1, descriptor, shaderStage };
	}

	void* Graphics::GetMappedGPUData(QbVkBufferHandle handle) {
		assert(renderer_->context_->resourceManager->buffers[handle].alloc.data != nullptr && "GPUBuffer data not mapped!");
		return renderer_->context_->resourceManager->buffers[handle].alloc.data;
	}
#pragma endregion

}