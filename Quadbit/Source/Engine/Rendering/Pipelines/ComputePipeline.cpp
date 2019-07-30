#include <PCH.h>

#include "ComputePipeline.h"
#include "../Common/QbVkUtils.h"

namespace Quadbit {
	ComputePipeline::ComputePipeline(std::shared_ptr<QbVkContext> context) {
		this->context_ = context;

		std::vector<VkDescriptorPoolSize> poolSizes{
			VkUtils::Init::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			VkUtils::Init::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2),
			VkUtils::Init::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 5),
		};

		// Create descriptor pool
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.poolSizeCount = 1;
		descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
		descriptorPoolCreateInfo.maxSets = 10;
		VK_CHECK(vkCreateDescriptorPool(context_->device, &descriptorPoolCreateInfo, nullptr, &descriptorPool_));

		// Create command pool
		VkCommandPoolCreateInfo cmdPoolCreateInfo = VkUtils::Init::CommandPoolCreateInfo();
		cmdPoolCreateInfo.queueFamilyIndex = context_->gpu->computeFamilyIdx;
		cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK(vkCreateCommandPool(context_->device, &cmdPoolCreateInfo, nullptr, &commandPool_));

		// Fence for compute sync
		VkFenceCreateInfo fenceCreateInfo = VkUtils::Init::FenceCreateInfo();
		VK_CHECK(vkCreateFence(context_->device, &fenceCreateInfo, nullptr, &computeFence_));
	}

	ComputePipeline::~ComputePipeline() {
		vkDestroyDescriptorPool(context_->device, descriptorPool_, nullptr);
		vkDestroyCommandPool(context_->device, commandPool_, nullptr);
		vkDestroyFence(context_->device, computeFence_, nullptr);
	}

	void ComputePipeline::Dispatch(QbVkComputeInstance& computeInstance) {
		VkSubmitInfo computeSubmitInfo = VkSubmitInfo{};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &computeInstance.commandBuffer;

		VK_CHECK(vkQueueSubmit(context_->computeQueue, 1, &computeSubmitInfo, computeFence_));

		VK_CHECK(vkWaitForFences(context_->device, 1, &computeFence_, VK_TRUE, std::numeric_limits<uint64_t>().max()));
		VK_CHECK(vkResetFences(context_->device, 1, &computeFence_));
	}
	
	QbVkComputeInstance ComputePipeline::CreateInstance(std::vector<std::tuple<VkDescriptorType, std::vector<void*>>> descriptors,  const char* shader, 
		const char* shaderFunc, const VkSpecializationInfo* specInfo, const uint32_t pushConstantRangeSize) {
		QbVkComputeInstance computeInstance;

		// Create descriptor sets
		std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindings;
		for (auto i = 0; i < descriptors.size(); i++) {
			descSetLayoutBindings.push_back(VkUtils::Init::DescriptorSetLayoutBinding(i, std::get<0>(descriptors[i]), VK_SHADER_STAGE_COMPUTE_BIT, static_cast<uint32_t>(std::get<1>(descriptors[i]).size())));
		}

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo();
		descSetLayoutCreateInfo.pBindings = descSetLayoutBindings.data();
		descSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descSetLayoutBindings.size());

		VK_CHECK(vkCreateDescriptorSetLayout(context_->device, &descSetLayoutCreateInfo, nullptr, &computeInstance.descriptorSetLayout));

		VkDescriptorSetAllocateInfo descSetAllocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
		descSetAllocInfo.descriptorPool = descriptorPool_;
		descSetAllocInfo.pSetLayouts = &computeInstance.descriptorSetLayout;
		descSetAllocInfo.descriptorSetCount = 1;
		VK_CHECK(vkAllocateDescriptorSets(context_->device, &descSetAllocInfo, &computeInstance.descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescSets;
		for (auto i = 0; i < descriptors.size(); i++) {
			if (std::get<0>(descriptors[i]) == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || std::get<0>(descriptors[i]) == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
				writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(computeInstance.descriptorSet, std::get<0>(descriptors[i]), i, 
					static_cast<VkDescriptorBufferInfo*>(std::get<1>(descriptors[i])[0]), static_cast<uint32_t>(std::get<1>(descriptors[i]).size())));
			}
			else if (std::get<0>(descriptors[i]) == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
				writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(computeInstance.descriptorSet, std::get<0>(descriptors[i]), i, 
					static_cast<VkDescriptorImageInfo*>(std::get<1>(descriptors[i])[0]), static_cast<uint32_t>(std::get<1>(descriptors[i]).size())));
			}
		}

		vkUpdateDescriptorSets(context_->device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);

		// Prepare shader
		std::vector<char> computeShaderBytecode = VkUtils::ReadShader(shader);
		VkShaderModule computeShaderModule = VkUtils::CreateShaderModule(computeShaderBytecode, context_->device);
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageInfo.pSpecializationInfo = specInfo;
		shaderStageInfo.module = computeShaderModule;
		// Entrypoint of a given shader module
		shaderStageInfo.pName = shaderFunc;

		// Create pipeline layout and pipeline
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VkUtils::Init::PipelineLayoutCreateInfo();
		pipelineLayoutCreateInfo.pSetLayouts = &computeInstance.descriptorSetLayout;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.offset = 0;
		pushConstantRange.size = pushConstantRangeSize;
		pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		// Push constant ranges are only part of the main pipeline layout
		pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRangeSize > 0 ? 1 : 0;
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRangeSize > 0 ? &pushConstantRange : nullptr;
		VK_CHECK(vkCreatePipelineLayout(context_->device, &pipelineLayoutCreateInfo, nullptr, &computeInstance.pipelineLayout));

		VkComputePipelineCreateInfo computePipelineCreateInfo = VkUtils::Init::ComputePipelineCreateInfo();
		computePipelineCreateInfo.layout = computeInstance.pipelineLayout;
		computePipelineCreateInfo.stage = shaderStageInfo;
		VK_CHECK(vkCreateComputePipelines(context_->device, nullptr, 1, &computePipelineCreateInfo, nullptr, &computeInstance.pipeline));

		// Create command buffer
		VkCommandBufferAllocateInfo cmdBufferAllocateInfo = VkUtils::Init::CommandBufferAllocateInfo(commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
		VK_CHECK(vkAllocateCommandBuffers(context_->device, &cmdBufferAllocateInfo, &computeInstance.commandBuffer));

		vkDestroyShaderModule(context_->device, computeShaderModule, nullptr);

		return computeInstance;
	}

	void ComputePipeline::DestroyInstance(QbVkComputeInstance& computeInstance) {
		vkDestroyDescriptorSetLayout(context_->device, computeInstance.descriptorSetLayout, nullptr);
		vkDestroyPipelineLayout(context_->device, computeInstance.pipelineLayout, nullptr);
		vkDestroyPipeline(context_->device, computeInstance.pipeline, nullptr);
		vkFreeCommandBuffers(context_->device, commandPool_, 1, &computeInstance.commandBuffer);
	}

	void ComputePipeline::UpdateDescriptors(std::vector<std::tuple<VkDescriptorType, void*>> descriptors, QbVkComputeInstance& instance) {
		std::vector<VkWriteDescriptorSet> writeDescSets;
		for (auto i = 0; i < descriptors.size(); i++) {
			if (std::get<0>(descriptors[i]) == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || std::get<0>(descriptors[i]) == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
				writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(instance.descriptorSet, std::get<0>(descriptors[i]), i, static_cast<VkDescriptorBufferInfo*>(std::get<1>(descriptors[i]))));
			}
			else if (std::get<0>(descriptors[i]) == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
				writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(instance.descriptorSet, std::get<0>(descriptors[i]), i, static_cast<VkDescriptorImageInfo*>(std::get<1>(descriptors[i]))));
			}
		}
		vkUpdateDescriptorSets(context_->device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
	}
}
