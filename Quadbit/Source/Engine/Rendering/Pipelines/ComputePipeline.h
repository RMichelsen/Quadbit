#pragma once

#include "../Common/QbVkUtils.h"

namespace Quadbit {
	struct QbVkComputeInstance {
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkQueryPool queryPool = VK_NULL_HANDLE;

		double msAvgTime = 0.0;
	};

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