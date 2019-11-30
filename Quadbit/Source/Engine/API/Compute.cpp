#include "Compute.h"

#include <EASTL/string.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>

#include "Engine/Rendering/Renderer.h"
#include "Engine/Rendering/Memory/ResourceManager.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Shaders/ShaderCompiler.h"

namespace Quadbit {
	Compute::Compute(QbVkRenderer* const renderer) : renderer_(renderer), resourceManager_(renderer->context_->resourceManager.get()) { }

	QbVkPipelineHandle Compute::CreatePipeline(const char* computePath, const char* kernel, 
		const void* specConstants, const uint32_t maxInstances) {
		auto handle = resourceManager_->pipelines_.GetNextHandle();
		resourceManager_->pipelines_[handle] = eastl::make_unique<QbVkPipeline>(*renderer_->context_, computePath, kernel, specConstants, maxInstances);

		return handle;
	}

	void Compute::BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name, 
		const QbVkBufferHandle bufferHandle, const QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
			pipeline->BindResource(descriptorsHandle, name, bufferHandle);
		}
		else {
			pipeline->BindResource(name, bufferHandle);
		}
	}

	void Compute::BindResource(const QbVkPipelineHandle pipelineHandle, const eastl::string name, 
		const QbVkTextureHandle textureHandle, const QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
			pipeline->BindResource(descriptorsHandle, name, textureHandle);
		}
		else {
			pipeline->BindResource(name, textureHandle);
		}
	}

	void Compute::BindResourceArray(const QbVkPipelineHandle pipelineHandle, const eastl::string name, 
		const eastl::vector<QbVkBufferHandle> bufferHandles, const QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
			pipeline->BindResourceArray(descriptorsHandle, name, bufferHandles);
		}
		else {
			pipeline->BindResourceArray(name, bufferHandles);
		}
	}

	void Compute::BindResourceArray(const QbVkPipelineHandle pipelineHandle, const eastl::string name,
		const eastl::vector<QbVkTextureHandle> textureHandles, const QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
			pipeline->BindResourceArray(descriptorsHandle, name, textureHandles);
		}
		else {
			pipeline->BindResourceArray(name, textureHandles);
		}
	}

	void Compute::Dispatch(const QbVkPipelineHandle pipelineHandle, uint32_t xGroups, uint32_t yGroups, uint32_t zGroups,
		const void* pushConstants, uint32_t pushConstantSize, QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		pipeline->Dispatch(xGroups, yGroups, zGroups, pushConstants, pushConstantSize, descriptorsHandle);
	}

	void Compute::DispatchX(uint32_t X, const QbVkPipelineHandle pipelineHandle, uint32_t xGroups, uint32_t yGroups, uint32_t zGroups,
		eastl::vector<const void*> pushConstantArray, uint32_t pushConstantSize, QbVkDescriptorSetsHandle descriptorsHandle) {
		auto& pipeline = resourceManager_->pipelines_[pipelineHandle];
		pipeline->DispatchX(X, xGroups, yGroups, zGroups, pushConstantArray, pushConstantSize, descriptorsHandle);
	}
}