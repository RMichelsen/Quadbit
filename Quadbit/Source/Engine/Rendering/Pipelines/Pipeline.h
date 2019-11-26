#pragma once

#include <EASTL/fixed_vector.h>
#include <EASTL/functional.h>
#include <EASTL/hash_map.h>
#include <EASTL/string.h>

#include <vulkan/vulkan.h>
#include <SPIRV-Cross/spirv_cross.hpp>

#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Rendering/Pipelines/PipelinePresets.h"

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

	struct GraphicsResources {
		eastl::string vertexPath;
		eastl::string vertexEntry;
		eastl::string fragmentPath;
		eastl::string fragmentEntry;
	};
	
	struct ComputeResources {
		eastl::string computePath;
		eastl::string computeEntry;
		VkCommandBuffer commandBuffer;
		VkFence computeFence;
		VkCommandPool commandPool;
		VkQueryPool queryPool;
		double msAvgTime;
	};

	struct PersistentPipelineInfo {
		eastl::vector<VkVertexInputAttributeDescription> attributeDescriptions;
		VkVertexInputBindingDescription inputBindingDescription;
		VkPipelineVertexInputStateCreateInfo vertexInputInfo;
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
		VkPipelineViewportStateCreateInfo viewportInfo;
		VkPipelineMultisampleStateCreateInfo multisampleInfo;
		VkPipelineColorBlendAttachmentState colorBlendingAttachment;
		VkPipelineColorBlendStateCreateInfo colorBlendInfo;
		VkPipelineRasterizationStateCreateInfo rasterizationInfo;
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
		VkPipelineDynamicStateCreateInfo dynamicStateInfo;
		eastl::vector<VkSpecializationMapEntry> specializationConstants;
		eastl::vector<unsigned char> specConstantsRawData;
		VkSpecializationInfo specInfo{};
	};

	class QbVkPipeline {
	public:
		VkPipeline pipeline_ = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

		QbVkDescriptorAllocatorHandle descriptorAllocator_ = QBVK_DESCRIPTOR_ALLOCATOR_NULL_HANDLE;
		eastl::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
		QbVkDescriptorSetsHandle mainDescriptors_ = QBVK_DESCRIPTOR_SETS_NULL_HANDLE;

		QbVkPipeline(QbVkContext& context, const char* vertexPath, const char* vertexEntry,
			const char* fragmentPath, const char* fragmentEntry, const QbVkPipelineDescription pipelineDescription, 
			const uint32_t maxInstances = 1, const eastl::vector<eastl::tuple<VkFormat, uint32_t>>& vertexAttributeOverride = {});
		QbVkPipeline(QbVkContext& context, const char* computePath, const char* computeEntry, 
			const void* specConstants = nullptr, const uint32_t maxInstances = 1);
		~QbVkPipeline();

		QbVkDescriptorSetsHandle GetNextDescriptorSetsHandle();

		// General purpose actions
		void Rebuild();
		void Bind(VkCommandBuffer& commandBuffer);
		void BindDescriptorSets(VkCommandBuffer& commandBuffer, QbVkDescriptorSetsHandle descriptorSets = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);

		// Compute specific actions
		void Dispatch(uint32_t xGroups, uint32_t yGroups, uint32_t zGroups, const void* pushConstants = nullptr,
			uint32_t pushConstantSize = 0, QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);
		void DispatchX(uint32_t X, uint32_t xGroups, uint32_t yGroups, uint32_t zGroups, eastl::vector<const void*> pushConstantArray = {},
			uint32_t pushConstantSize = 0, QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);

		void BindResource(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const QbVkBufferHandle bufferHandle);
		void BindResource(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const QbVkTextureHandle textureHandle);
		void BindResource(const eastl::string name, const QbVkBufferHandle bufferHandle);
		void BindResource(const eastl::string name, const QbVkTextureHandle textureHandle);

		void BindResourceArray(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const eastl::vector<QbVkBufferHandle> bufferHandles);
		void BindResourceArray(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const eastl::vector<QbVkTextureHandle> textureHandles);
		void BindResourceArray(const eastl::string name, const eastl::vector<QbVkBufferHandle> bufferHandles);
		void BindResourceArray(const eastl::string name, const eastl::vector<QbVkTextureHandle> textureHandles);

	private:
		QbVkContext& context_;
		eastl::hash_map<eastl::string, ResourceInformation> resourceInfo_;

		PersistentPipelineInfo persistentPipelineInfo_;

		eastl::unique_ptr<GraphicsResources> graphicsResources_;
		bool compute_ = false;
		eastl::unique_ptr<ComputeResources> computeResources_;

		// Aligned sizes of dynamic UBO's used in the pipeline
		eastl::vector<uint32_t> uboSizes_;

		void ParseShader(const spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& resources,
			eastl::vector<eastl::vector<VkDescriptorSetLayoutBinding>>& setLayoutBindings,
			eastl::vector<VkDescriptorPoolSize>& poolSizes, VkShaderStageFlags shaderStage);

	};
}