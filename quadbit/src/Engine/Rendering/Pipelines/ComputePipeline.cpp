#include "ComputePipeline.h"

#include <imgui/imgui.h>

#include "Engine/Core/QbVulkanDefs.h"
#include "Engine/Core/QbVulkanInternalDefs.h"
#include "Engine/Rendering/QbVulkanUtils.h"

namespace Quadbit {
	ComputePipeline::ComputePipeline(QbVkContext& context) : context_(context) {

		// Create command pool
		VkCommandPoolCreateInfo cmdPoolCreateInfo = VkUtils::Init::CommandPoolCreateInfo();
		cmdPoolCreateInfo.queueFamilyIndex = context_.gpu->computeFamilyIdx;
		cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK(vkCreateCommandPool(context_.device, &cmdPoolCreateInfo, nullptr, &commandPool_));

		// Fence for compute sync
		VkFenceCreateInfo fenceCreateInfo = VkUtils::Init::FenceCreateInfo();
		VK_CHECK(vkCreateFence(context_.device, &fenceCreateInfo, nullptr, &computeFence_));
	}

	ComputePipeline::~ComputePipeline() {
		for (auto instance : activeInstances_) {
			DestroyInstance(std::get<std::shared_ptr<QbVkComputeInstance>>(instance));
		}

		vkDestroyCommandPool(context_.device, commandPool_, nullptr);
		vkDestroyFence(context_.device, computeFence_, nullptr);
	}

	void ComputePipeline::RecordCommands(std::shared_ptr<QbVkComputeInstance> instance, std::function<void()> func) {
		VkCommandBufferBeginInfo cmdBufInfo = VkUtils::Init::CommandBufferBeginInfo();
		VK_CHECK(vkBeginCommandBuffer(instance->commandBuffer, &cmdBufInfo));
		vkCmdResetQueryPool(instance->commandBuffer, instance->queryPool, 0, 128);
		vkCmdWriteTimestamp(instance->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, instance->queryPool, 0);

		func();

		vkCmdWriteTimestamp(instance->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, instance->queryPool, 1);
		VK_CHECK(vkEndCommandBuffer(instance->commandBuffer));
	}

	void ComputePipeline::Dispatch(std::shared_ptr<QbVkComputeInstance> computeInstance) {
		VkSubmitInfo computeSubmitInfo = VkSubmitInfo{};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &computeInstance->commandBuffer;

		VK_CHECK(vkQueueSubmit(context_.computeQueue, 1, &computeSubmitInfo, computeFence_));

		VK_CHECK(vkWaitForFences(context_.device, 1, &computeFence_, VK_TRUE, std::numeric_limits<uint64_t>().max()));
		VK_CHECK(vkResetFences(context_.device, 1, &computeFence_));

		uint64_t timestamps[2]{};
		VK_CHECK(vkGetQueryPoolResults(context_.device, computeInstance->queryPool, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));

		double computeStart = double(timestamps[0]) * context_.gpu->deviceProps.limits.timestampPeriod * 1e-6;
		double computeEnd = double(timestamps[1]) * context_.gpu->deviceProps.limits.timestampPeriod * 1e-6;
		computeInstance->msAvgTime = computeInstance->msAvgTime * 0.95 + (computeEnd - computeStart) * 0.05;
	}

	std::shared_ptr<QbVkComputeInstance> ComputePipeline::CreateInstance(std::vector<QbVkComputeDescriptor>& descriptors, const char* shader,
		const char* shaderFunc, const VkSpecializationInfo* specInfo, const uint32_t pushConstantRangeSize) {
		auto computeInstance = std::make_shared<QbVkComputeInstance>();

		std::vector<VkDescriptorPoolSize> poolSizes;
		for (auto i = 0; i < descriptors.size(); i++) {
			poolSizes.push_back(VkUtils::Init::DescriptorPoolSize(descriptors[i].type, descriptors[i].count));
		}

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;

		VK_CHECK(vkCreateDescriptorPool(context_.device, &poolInfo, nullptr, &computeInstance->descriptorPool));

		// Create descriptor sets
		std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindings;
		for (auto i = 0; i < descriptors.size(); i++) {
			descSetLayoutBindings.push_back(VkUtils::Init::DescriptorSetLayoutBinding(i, descriptors[i].type, VK_SHADER_STAGE_COMPUTE_BIT, descriptors[i].count));
		}

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo();
		descSetLayoutCreateInfo.pBindings = descSetLayoutBindings.data();
		descSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descSetLayoutBindings.size());

		VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &descSetLayoutCreateInfo, nullptr, &computeInstance->descriptorSetLayout));

		VkDescriptorSetAllocateInfo descSetAllocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
		descSetAllocInfo.descriptorPool = computeInstance->descriptorPool;
		descSetAllocInfo.pSetLayouts = &computeInstance->descriptorSetLayout;
		descSetAllocInfo.descriptorSetCount = 1;
		VK_CHECK(vkAllocateDescriptorSets(context_.device, &descSetAllocInfo, &computeInstance->descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescSets;
		for (auto i = 0; i < descriptors.size(); i++) {
			if (descriptors[i].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || descriptors[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
				writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(computeInstance->descriptorSet, descriptors[i].type, i,
					static_cast<VkDescriptorBufferInfo*>(descriptors[i].data), descriptors[i].count));
			}
			else if (descriptors[i].type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || descriptors[i].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
				writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(computeInstance->descriptorSet, descriptors[i].type, i,
					static_cast<VkDescriptorImageInfo*>(descriptors[i].data), descriptors[i].count));
			}
		}

		vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);

		// Prepare shader
		std::vector<char> computeShaderBytecode = VkUtils::ReadShader(shader);
		VkShaderModule computeShaderModule = VkUtils::CreateShaderModule(computeShaderBytecode, context_.device);
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageInfo.pSpecializationInfo = specInfo;
		shaderStageInfo.module = computeShaderModule;
		// Entrypoint of a given shader module
		shaderStageInfo.pName = shaderFunc;

		// Create pipeline layout and pipeline
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VkUtils::Init::PipelineLayoutCreateInfo();
		pipelineLayoutCreateInfo.pSetLayouts = &computeInstance->descriptorSetLayout;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.offset = 0;
		pushConstantRange.size = pushConstantRangeSize;
		pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		// Push constant ranges are only part of the main pipeline layout
		pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRangeSize > 0 ? 1 : 0;
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRangeSize > 0 ? &pushConstantRange : nullptr;
		VK_CHECK(vkCreatePipelineLayout(context_.device, &pipelineLayoutCreateInfo, nullptr, &computeInstance->pipelineLayout));

		VkComputePipelineCreateInfo computePipelineCreateInfo = VkUtils::Init::ComputePipelineCreateInfo();
		computePipelineCreateInfo.layout = computeInstance->pipelineLayout;
		computePipelineCreateInfo.stage = shaderStageInfo;
		VK_CHECK(vkCreateComputePipelines(context_.device, nullptr, 1, &computePipelineCreateInfo, nullptr, &computeInstance->pipeline));

		// Create command buffer
		VkCommandBufferAllocateInfo cmdBufferAllocateInfo = VkUtils::Init::CommandBufferAllocateInfo(commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
		VK_CHECK(vkAllocateCommandBuffers(context_.device, &cmdBufferAllocateInfo, &computeInstance->commandBuffer));

		// Create query pool for timestamp statistics
		VkQueryPoolCreateInfo queryPoolCreateInfo{};
		queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolCreateInfo.queryCount = 128;
		VK_CHECK(vkCreateQueryPool(context_.device, &queryPoolCreateInfo, nullptr, &computeInstance->queryPool));

		vkDestroyShaderModule(context_.device, computeShaderModule, nullptr);

		activeInstances_.push_back(std::make_tuple(shader, computeInstance));
		return computeInstance;
	}

	void ComputePipeline::DestroyInstance(std::shared_ptr<QbVkComputeInstance> computeInstance) {
		vkDestroyDescriptorSetLayout(context_.device, computeInstance->descriptorSetLayout, nullptr);
		vkFreeDescriptorSets(context_.device, computeInstance->descriptorPool, 1, &computeInstance->descriptorSet);
		vkDestroyDescriptorPool(context_.device, computeInstance->descriptorPool, nullptr);
		vkDestroyPipelineLayout(context_.device, computeInstance->pipelineLayout, nullptr);
		vkDestroyPipeline(context_.device, computeInstance->pipeline, nullptr);
		vkFreeCommandBuffers(context_.device, commandPool_, 1, &computeInstance->commandBuffer);
	}

	void ComputePipeline::ImGuiDrawState() {
		ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
		ImGui::Begin("Quadbit Compute Shader Performance", nullptr);
		for (auto instance : activeInstances_) {
			auto computeInstance = std::get<std::shared_ptr<QbVkComputeInstance>>(instance);
			auto label = std::get<const char*>(instance);
			ImGui::Text("%s %.5fms", label, computeInstance->msAvgTime);
		}
		ImGui::End();
	}
}
