#pragma once

#include <EASTL/functional.h>
#include <EASTL/tuple.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class ComputePipeline {
	public:
		ComputePipeline(QbVkContext& context);
		~ComputePipeline();

		void RecordCommands(const QbVkComputeInstance* instance, eastl::function<void()> func);
		void Dispatch(QbVkComputeInstance* instance);
		QbVkComputeInstance* CreateInstance(eastl::vector<QbVkComputeDescriptor>& descriptors, const char* shader,
			const char* shaderFunc, const VkSpecializationInfo* specInfo = nullptr, const uint32_t pushConstantRangeSize = 0);
		void DestroyInstance(const QbVkComputeInstance* computeInstance);
		void ImGuiDrawState();

	private:
		eastl::vector<eastl::tuple<const char*, eastl::unique_ptr<QbVkComputeInstance>>> activeInstances_;

		QbVkContext& context_;

		VkCommandPool commandPool_ = VK_NULL_HANDLE;
		VkFence computeFence_ = VK_NULL_HANDLE;
	};
}