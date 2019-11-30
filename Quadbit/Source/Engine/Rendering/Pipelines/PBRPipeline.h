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
		float sunAzimuth;
		float sunAltitude;
	};

	class PBRPipeline {
	public:
		PBRPipeline(QbVkContext& context);

		void DrawShadows(uint32_t resourceIndex, VkCommandBuffer commandBuffer);
		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandBuffer);

		PBRSceneComponent LoadModel(const char* path);

		QbVkPipelineHandle pipeline_;
		QbVkPipelineHandle skyPipeline_;

	private:
		void SetViewportAndScissor(VkCommandBuffer& commandBuffer);


		// PBR Loader Helpers
		QbVkPBRMaterial ParseMaterial(const tinygltf::Model& model, const tinygltf::Material& material);
		void ParseNode(const tinygltf::Model& model, const tinygltf::Node& node, PBRSceneComponent& scene,
			eastl::vector<QbVkVertex>& vertices, eastl::vector<uint32_t>& indices, const eastl::vector<QbVkPBRMaterial> materials, glm::mat4 parentTransform);
		QbVkPBRMesh ParseMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh,
			eastl::vector<QbVkVertex>& vertices, eastl::vector<uint32_t>& indices, const eastl::vector<QbVkPBRMaterial>& materials);
		QbVkDescriptorSetsHandle WriteMaterialDescriptors(const QbVkPBRMaterial& material);
		QbVkTextureHandle CreateTextureFromResource(const tinygltf::Model& model, const tinygltf::Texture& texture);
		VkSamplerCreateInfo GetSamplerInfo(const tinygltf::Model& model, int samplerIndex);

		friend class QbVkRenderer;

		QbVkContext& context_;

		QbVkUniformBuffer<SunUBO> sunUBO_;
	};
}