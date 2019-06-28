#pragma once

#include "../Common/QbVkDefines.h"

namespace Quadbit {
	struct QbVkComputeInstance {
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	};

	class ComputePipeline {
	public:
		ComputePipeline(std::shared_ptr<QbVkContext> context);
		~ComputePipeline();
		void Dispatch(QbVkComputeInstance& instance);
		void RunJobs();
		QbVkComputeInstance CreateInstance(std::vector<std::tuple<VkDescriptorType, void*>> descriptors, const char* shader, const char* shaderFunc);
		void DestroyInstance(QbVkComputeInstance& computeInstance);

	private:
		std::shared_ptr<QbVkContext> context_ = nullptr;
		std::deque<QbVkComputeInstance> dispatchQueue_;

		VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
		VkCommandPool commandPool_ = VK_NULL_HANDLE;
		VkFence computeFence_ = VK_NULL_HANDLE;
	};
}