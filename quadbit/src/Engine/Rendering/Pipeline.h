#pragma once

#include <EASTL/fixed_vector.h>
#include <EASTL/hash_map.h>
#include <EASTL/string.h>

#include <vulkan/vulkan.h>
#include <SPIRV-Cross/spirv_cross.hpp>

#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Rendering/PipelinePresets.h"

namespace Quadbit {
	struct ResourceInformation {
		VkDescriptorType descriptorType;
		uint32_t dstSet;
		uint32_t dstBinding;
		uint32_t descriptorCount;
	};

	struct VertexInput {
		uint32_t location;
		uint32_t size;
	};

	class QbVkPipeline {
	public:
		VkPipeline pipeline_ = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

		QbVkDescriptorAllocatorHandle descriptorAllocator_ = QBVK_DESCRIPTOR_ALLOCATOR_NULL_HANDLE;
		eastl::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
		QbVkDescriptorSetsHandle mainDescriptors_ = QBVK_DESCRIPTOR_SETS_NULL_HANDLE;

		QbVkPipeline(QbVkContext& context, const uint32_t* vertexBytecode, uint32_t vertexSize, 
			const uint32_t* fragmentBytecode, uint32_t fragmentSize, const QbVkPipelineDescription pipelineDescription, 
			const uint32_t maxInstances = 1, const eastl::vector<eastl::tuple<VkFormat, uint32_t>>& vertexAttributeOverride = {});
		~QbVkPipeline();

		QbVkDescriptorSetsHandle GetNextDescriptorSetsHandle();

		void Bind(VkCommandBuffer& commandBuffer);
		void BindResource(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const QbVkBufferHandle bufferHandle);
		void BindResource(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const QbVkTextureHandle textureHandle);
		void BindResource(const eastl::string name, const QbVkBufferHandle bufferHandle);
		void BindResource(const eastl::string name, const QbVkTextureHandle textureHandle);

	private:
		QbVkContext& context_;
		eastl::hash_map<eastl::string, ResourceInformation> resourceInfo_;

		void ParseShader(const spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& resources,
			eastl::vector<eastl::vector<VkDescriptorSetLayoutBinding>>& setLayoutBindings,
			eastl::vector<VkDescriptorPoolSize>& poolSizes, VkShaderStageFlags shaderStage);

	};
}