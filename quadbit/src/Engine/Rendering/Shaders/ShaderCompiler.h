#pragma once

#include <vector>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class QbVkShaderCompiler {
	public:
		QbVkShaderCompiler(QbVkContext& context);

		std::vector<uint32_t> CompileShader(const char* path, QbVkShaderType shaderType);

	private:
		TBuiltInResource resourceLimits_;

		const TBuiltInResource GetResourceLimits(const VkPhysicalDeviceLimits& limits);
	};
}