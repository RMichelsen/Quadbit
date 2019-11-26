#pragma once
#include <EASTL/array.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <tinygltf/tiny_gltf.h>

#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Rendering/Pipelines/Pipeline.h"
#include "Engine/Rendering/Shaders/ShaderInstance.h"
#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class PBRPipeline {
	public:
		PBRPipeline(QbVkContext& context);

		void RebuildPipeline();
		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandBuffer);

		PBRSceneComponent LoadModel(const char* path);

		Entity GetActiveCamera();
		void SetCamera(Entity entity);
		void LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour);

		QbVkPipelineHandle pipeline_;

	private:
		void SetViewportAndScissor(VkCommandBuffer& commandBuffer);


		// PBR Loader Helpers
		QbVkPBRMaterial ParseMaterial(const tinygltf::Model& model, const tinygltf::Material& material);
		void ParseNode(const tinygltf::Model& model, const tinygltf::Node& node, PBRSceneComponent& scene,
			eastl::vector<QbVkVertex>& vertices, eastl::vector<uint32_t>& indices, const eastl::vector<QbVkPBRMaterial> materials, glm::mat4 parentTransform);
		QbVkPBRMesh ParseMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh,
			eastl::vector<QbVkVertex>& vertices, eastl::vector<uint32_t>& indices, const eastl::vector<QbVkPBRMaterial>& materials);
		QbVkDescriptorSetsHandle WriteMaterialDescriptors(const QbVkPBRMaterial& material);
		QbVkTextureHandle CreateTextureFromResource(const tinygltf::Model& model, const tinygltf::Material& material, const char* textureName);
		VkSamplerCreateInfo GetSamplerInfo(const tinygltf::Model& model, int samplerIndex);

		friend class QbVkRenderer;

		QbVkContext& context_;

		Entity fallbackCamera_ = NULL_ENTITY;
		Entity userCamera_ = NULL_ENTITY;

		Entity environmentMap_ = NULL_ENTITY;
	};
}