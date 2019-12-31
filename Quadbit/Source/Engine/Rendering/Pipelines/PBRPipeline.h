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
	struct SunUBO {
		glm::mat4 lightViewProj;
		float sunAzimuth;
		float sunAltitude;
	};

	class PBRPipeline {
	public:
		PBRPipeline(QbVkContext& context);

		void DrawShadows(uint32_t resourceIndex, VkCommandBuffer commandBuffer);
		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandBuffer);

		QbVkPBRMaterial CreateMaterial(QbVkTextureHandle baseColourTexture, QbVkTextureHandle metallicRoughnessTexture = QBVK_TEXTURE_NULL_HANDLE,
			QbVkTextureHandle normalTexture = QBVK_TEXTURE_NULL_HANDLE, QbVkTextureHandle occlusionTexture = QBVK_TEXTURE_NULL_HANDLE, 
			QbVkTextureHandle emissiveTexture = QBVK_TEXTURE_NULL_HANDLE);
		PBRSceneComponent LoadModel(const char* path);
		PBRSceneComponent CreatePlane(uint32_t xSize, uint32_t zSize, const QbVkPBRMaterial& material);
		void DestroyModel(const Entity& entity);

		QbVkPipelineHandle pipeline_;
		QbVkPipelineHandle skyPipeline_;

	private:
		void SetViewportAndScissor(VkCommandBuffer& commandBuffer);

		// PBR Loader Helpers
		QbVkPBRMaterial ParseMaterial(const tinygltf::Model& model, const tinygltf::Material& material);
		void ParseNode(const tinygltf::Model& model, const tinygltf::Node& node, PBRSceneComponent& scene,
			eastl::vector<QbVkVertex>& vertices, eastl::vector<uint32_t>& indices, glm::mat4 parentTransform);
		QbVkPBRMesh ParseMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh,
			eastl::vector<QbVkVertex>& vertices, eastl::vector<uint32_t>& indices, const eastl::vector<QbVkPBRMaterial>& materials);
		QbVkDescriptorSetsHandle WriteMaterialDescriptors(QbVkPBRMaterial& material);
		QbVkTextureHandle CreateTextureFromResource(const tinygltf::Model& model, const tinygltf::Texture& texture);
		VkSamplerCreateInfo GetSamplerInfo(const tinygltf::Model& model, int samplerIndex);

		friend class QbVkRenderer;

		QbVkContext& context_;

		QbVkUniformBuffer<SunUBO> sunUBO_;
	};
}