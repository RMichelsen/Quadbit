#include "Compute.h"

#include "Engine/Rendering/Renderer.h"
#include "Engine/Rendering/Pipelines/ComputePipeline.h"

namespace Quadbit {
	Compute::Compute(QbVkRenderer* const renderer) : renderer_(renderer) { }

	QbVkComputeInstance* Compute::CreateComputeInstance(std::vector<QbVkComputeDescriptor>& descriptors,
		const char* shader, const char* shaderFunc, const VkSpecializationInfo* specInfo, uint32_t pushConstantRangeSize) {
		return renderer_->computePipeline_->CreateInstance(descriptors, shader, shaderFunc, specInfo, pushConstantRangeSize);
	}

	void Compute::ComputeDispatch(QbVkComputeInstance* instance) {
		renderer_->computePipeline_->Dispatch(instance);
	}

	void Compute::ComputeRecord(const QbVkComputeInstance* instance, std::function<void()> func) {
		renderer_->computePipeline_->RecordCommands(instance, func);
	}

	QbVkComputeDescriptor Compute::CreateComputeDescriptor(VkDescriptorType type, void* descriptor) {
		return { type, 1, descriptor };
	}
}