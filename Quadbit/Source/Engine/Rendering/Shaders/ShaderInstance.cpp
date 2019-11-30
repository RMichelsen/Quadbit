#include "ShaderInstance.h"

#include "Engine/Rendering/VulkanUtils.h"

namespace Quadbit {
	QbVkShaderInstance::~QbVkShaderInstance() {
		for (const auto& stage : stages) {
			vkDestroyShaderModule(context_.device, stage.module, nullptr);
		}
	}

	void QbVkShaderInstance::AddShader(const uint32_t* shaderBytecode, const size_t shaderSize, const char* shaderEntry, VkShaderStageFlagBits shaderStage) {
		VkShaderModule shaderModule = VkUtils::CreateShaderModule(shaderBytecode, static_cast<uint32_t>(shaderSize), context_.device);

		VkPipelineShaderStageCreateInfo shader = VkUtils::Init::PipelineShaderStageCreateInfo();
		shader.stage = shaderStage;
		shader.module = shaderModule;
		shader.pName = shaderEntry;
		
		stages.push_back(shader);
	}

	void QbVkShaderInstance::AddShader(const char* shaderPath, const char* shaderEntry, VkShaderStageFlagBits shaderStage) {
		eastl::vector<char> shaderBytecode = VkUtils::ReadShader(shaderPath);
		VkShaderModule shaderModule = VkUtils::CreateShaderModule(shaderBytecode, context_.device);

		VkPipelineShaderStageCreateInfo shader = VkUtils::Init::PipelineShaderStageCreateInfo();
		shader.stage = shaderStage;
		shader.module = shaderModule;
		shader.pName = shaderEntry;

		stages.push_back(shader);
	}
}
