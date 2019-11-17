#pragma once

#include <functional>

#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class QbVkRenderer;

	class Compute {
	public:
		Compute(QbVkRenderer* const renderer);

		QbVkComputeInstance* CreateComputeInstance(std::vector<QbVkComputeDescriptor>& descriptors, const char* shader, const char* shaderFunc,
			const VkSpecializationInfo* specInfo = nullptr, uint32_t pushConstantRangeSize = 0);
		void ComputeDispatch(QbVkComputeInstance* instance);
		void ComputeRecord(const QbVkComputeInstance* instance, std::function<void()> func);

		QbVkComputeDescriptor CreateComputeDescriptor(VkDescriptorType type, void* descriptor);

		template<typename T>
		QbVkComputeDescriptor CreateComputeDescriptor(VkDescriptorType type, std::vector<T>& descriptors) {
			return { type, static_cast<uint32_t>(descriptors.size()), descriptors.data() };
		}

	private:
		QbVkRenderer* const renderer_;

	};
}