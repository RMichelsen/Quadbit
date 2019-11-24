#pragma once

#include <EASTL/functional.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class QbVkRenderer;

	class Compute {
	public:
		Compute(QbVkRenderer* const renderer);

		QbVkPipelineHandle CreatePipeline(const uint32_t* computeBytecode, uint32_t computeSize,
			const char* kernel, const void* specConstants = nullptr, const uint32_t maxInstances = 1);
		QbVkPipelineHandle CreatePipeline(const char* computePath, const char* kernel,
			const void* specConstants = nullptr, const uint32_t maxInstances = 1); 

		void BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name, 
			const QbVkBufferHandle bufferHandle, const QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);
		void BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name, 
			const QbVkTextureHandle textureHandle, QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);
		void BindResourceArray(const QbVkPipelineHandle pipelineHandle, const eastl::string name, 
			const eastl::vector<QbVkBufferHandle> bufferHandles, const QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);
		void BindResourceArray(const QbVkPipelineHandle pipelineHandle, const eastl::string name, 
			const eastl::vector<QbVkTextureHandle> textureHandles, const QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);

		void Dispatch(const QbVkPipelineHandle pipelineHandle, uint32_t xGroups, uint32_t yGroups, uint32_t zGroups, 
			const void* pushConstants = nullptr, uint32_t pushConstantSize = 0, QbVkDescriptorSetsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);
		void DispatchX(uint32_t X, const QbVkPipelineHandle pipelineHandle, uint32_t xGroups, uint32_t yGroups, uint32_t zGroups,
			eastl::vector<const void*> pushConstantArray = {}, uint32_t pushConstantSize = 0, 
			QbVkDescriptorSetsHandle descriptorsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE);

	private:
		QbVkRenderer* const renderer_;
		QbVkResourceManager* const resourceManager_;

	};
}