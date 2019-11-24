#pragma once

#include <EASTL/vector.h>
#include <vulkan/vulkan.h>

#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class QbVkShaderInstance {
	public:
		eastl::vector<VkPipelineShaderStageCreateInfo> stages;

		QbVkShaderInstance(QbVkContext& context) : context_(context) {}
		~QbVkShaderInstance();
		void AddShader(const eastl::vector<uint32_t>& shaderBytecode, const char* shaderEntry, VkShaderStageFlagBits shaderStage);
		void AddShader(const uint32_t* shaderBytecode, const uint32_t shaderSize, const char* shaderEntry, VkShaderStageFlagBits shaderStage);
		void AddShader(const char* shaderPath, const char* shaderEntry, VkShaderStageFlagBits shaderStage);

	private:
		QbVkContext& context_;
	};
}
