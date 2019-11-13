#pragma once

#include <functional>

#include "Engine/Core/QbRenderDefs.h"
#include "Engine/Core/QbVulkanDefs.h"

namespace Quadbit {
	class ComputePipeline {
	public:
		ComputePipeline(QbVkContext& context);
		~ComputePipeline();

		void RecordCommands(const QbVkComputeInstance* instance, std::function<void()> func);
		void Dispatch(QbVkComputeInstance* instance);
		QbVkComputeInstance* CreateInstance(std::vector<QbVkComputeDescriptor>& descriptors, const char* shader,
			const char* shaderFunc, const VkSpecializationInfo* specInfo = nullptr, const uint32_t pushConstantRangeSize = 0);
		void DestroyInstance(const QbVkComputeInstance* computeInstance);
		void ImGuiDrawState();

	private:
		std::vector<std::tuple<const char*, std::unique_ptr<QbVkComputeInstance>>> activeInstances_;

		QbVkContext& context_;

		VkCommandPool commandPool_ = VK_NULL_HANDLE;
		VkFence computeFence_ = VK_NULL_HANDLE;
	};
}