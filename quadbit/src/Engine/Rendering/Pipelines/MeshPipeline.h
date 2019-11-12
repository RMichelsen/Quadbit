#pragma once
#include <memory>
#include <array>
#include <vector>
#include <deque>

#include "Engine/Core/QbRenderDefs.h"
#include "Engine/Core/QbVulkanDefs.h"
#include "Engine/Entities/EntityManager.h"

namespace Quadbit {
	class MeshPipeline {
	public:
		MeshPipeline(QbVkContext& context);
		~MeshPipeline();
		void RebuildPipeline();

		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer);

	private:
		friend class QbVkRenderer;

		QbVkContext& context_;
		EntityManager& entityManager_;

		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool_;
		VkDescriptorSetLayout descriptorSetLayout_;
		std::array<std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>, MAX_TEXTURES> descriptorSets_{};
		uint32_t textureCount = 0;

		MeshBuffers meshBuffers_{};

		std::vector<std::shared_ptr<QbVkRenderMeshInstance>> externalInstances_;

		Entity fallbackCamera_ = NULL_ENTITY;
		Entity userCamera_ = NULL_ENTITY;

		std::vector<VertexBufHandle> vertexHandleDeleteQueue_;
		std::vector<IndexBufHandle> indexHandleDeleteQueue_;

		void CreateDescriptorPoolAndLayout();
		void CreatePipeline();

		void DestroyMesh(RenderMeshComponent& renderMeshComponent);
		void DestroyVertexBuffer(VertexBufHandle handle);
		void DestroyIndexBuffer(IndexBufHandle handle);
		VertexBufHandle CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount);
		IndexBufHandle CreateIndexBuffer(const std::vector<uint32_t>& indices);

		std::shared_ptr<QbVkRenderMeshInstance> CreateInstance(std::vector<QbVkRenderDescriptor>& descriptors, std::vector<QbVkVertexInputAttribute> vertexAttribs,
			const char* vertexShader, const char* vertexEntry, const char* fragmentShader, const char* fragmentEntry, int pushConstantStride = -1,
			VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT, VkBool32 depthTestingEnabled = VK_TRUE);
		void DestroyInstance(std::shared_ptr<QbVkRenderMeshInstance> instance);

		Entity GetActiveCamera();
		RenderTexturedObjectComponent CreateObject(const char* objPath, const char* texturePath, VkFormat textureFormat);
	};
}