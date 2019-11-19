#pragma once
#include <EASTL/array.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Rendering/ShaderInstance.h"
#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class MeshPipeline {
	public:
		MeshPipeline(QbVkContext& context);
		~MeshPipeline();
		void RebuildPipeline();

		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer);

		Entity GetActiveCamera();
		void SetCamera(Entity entity);
		void LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour);

		const QbVkRenderMeshInstance* CreateInstance(eastl::vector<QbVkRenderDescriptor>& descriptors, eastl::vector<QbVkVertexInputAttribute> vertexAttribs,
			QbVkShaderInstance& shaderInstance, int pushConstantStride = -1, VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT,
			VkBool32 depthTestingEnabled = VK_TRUE);

		void DestroyMesh(RenderMeshComponent& renderMeshComponent);
		void DestroyInstance(const QbVkRenderMeshInstance* instance);

		//RenderTexturedObjectComponent CreateObject(const char* objPath, const char* texturePath, VkFormat textureFormat);

	private:
		friend class QbVkRenderer;

		QbVkContext& context_;

		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool_;
		VkDescriptorSetLayout descriptorSetLayout_;
		eastl::array<eastl::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>, 512> descriptorSets_{};
		uint32_t textureCount = 0;

		eastl::vector<eastl::unique_ptr<QbVkRenderMeshInstance>> externalInstances_;

		Entity fallbackCamera_ = NULL_ENTITY;
		Entity userCamera_ = NULL_ENTITY;

		Entity environmentMap_ = NULL_ENTITY;

		void CreateDescriptorPoolAndLayout();
		void CreatePipeline();
	};
}