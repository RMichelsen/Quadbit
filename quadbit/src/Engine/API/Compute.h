#pragma once

#include <EASTL/functional.h>
#include <EASTL/vector.h>

#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class QbVkRenderer;

	class Compute {
	public:
		Compute(QbVkRenderer* const renderer);

		QbVkComputeInstance* CreateComputeInstance(eastl::vector<QbVkComputeDescriptor>& descriptors, const char* shader, const char* shaderFunc,
			const VkSpecializationInfo* specInfo = nullptr, uint32_t pushConstantRangeSize = 0);
		void ComputeDispatch(QbVkComputeInstance* instance);
		void ComputeRecord(const QbVkComputeInstance* instance, eastl::function<void()> func);

		QbVkComputeDescriptor CreateComputeDescriptor(VkDescriptorType type, void* descriptor);

		template<typename T>
		QbVkComputeDescriptor CreateComputeDescriptor(VkDescriptorType type, eastl::vector<T>& descriptors) {
			return { type, static_cast<uint32_t>(descriptors.size()), descriptors.data() };
		}

	private:
		QbVkRenderer* const renderer_;

	};
}