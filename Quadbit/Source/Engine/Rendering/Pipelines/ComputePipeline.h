#pragma once

#include "Engine/Core/QbRenderDefs.h"
#include "Engine/Core/QbVulkanDefs.h"

namespace Quadbit {
	class ComputePipeline {
	public:
		ComputePipeline(std::shared_ptr<QbVkContext> context);
		~ComputePipeline();

		void RecordCommands(std::shared_ptr<QbVkComputeInstance> instance, std::function<void()> func);
		void Dispatch(std::shared_ptr<QbVkComputeInstance> instance);
		std::shared_ptr<QbVkComputeInstance> CreateInstance(std::vector<QbVkComputeDescriptor>& descriptors, const char* shader, 
			const char* shaderFunc, const VkSpecializationInfo* specInfo = nullptr, const uint32_t pushConstantRangeSize = 0);
		void DestroyInstance(std::shared_ptr<QbVkComputeInstance> computeInstance);
		void ImGuiDrawState();

	private:
		std::vector<std::tuple<const char*, std::shared_ptr<QbVkComputeInstance>>> activeInstances_;

		std::shared_ptr<QbVkContext> context_ = nullptr;

		VkCommandPool commandPool_ = VK_NULL_HANDLE;
		VkFence computeFence_ = VK_NULL_HANDLE;
	};
}