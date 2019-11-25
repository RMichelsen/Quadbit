#include "ShaderCompiler.h"

#include "Engine/Core/Logging.h"
#include "Engine/Rendering/VulkanUtils.h"

namespace Quadbit {
    QbVkShaderCompiler::QbVkShaderCompiler(QbVkContext& context) {
        glslang::InitializeProcess();

        resourceLimits_ = GetResourceLimits(context.gpu->deviceProps.limits);
    }

    std::vector<uint32_t> QbVkShaderCompiler::CompileShader(const char* path, QbVkShaderType shaderType) {
        FILE* pFile = fopen(path, "rb");
        QB_ASSERT(pFile != nullptr && "Couldn't open file!");

        // Get file size
        fseek(pFile, 0, SEEK_END);
        uint32_t fileSize = ftell(pFile);
        rewind(pFile);

        // Read data into a vector
        eastl::vector<char> bytecode;
        bytecode.resize(fileSize);
        size_t result = fread(bytecode.data(), sizeof(char), fileSize, pFile);
        QB_ASSERT(result == fileSize && "Error reading shader file");
        fclose(pFile);

        // Add null terminator
        bytecode.push_back('\00');

        EShLanguage language; 
        if (shaderType == QbVkShaderType::QBVK_SHADER_TYPE_VERTEX) {
            language = EShLangVertex;
        }
        else if (shaderType == QbVkShaderType::QBVK_SHADER_TYPE_FRAGMENT) {
            language = EShLangFragment;
        }
        else if (shaderType == QbVkShaderType::QBVK_SHADER_TYPE_COMPUTE) {
            language = EShLangCompute;
        }

        // Load the shader string into the program
        glslang::TShader shader(language);
        char const* const shaderString = bytecode.data();
        shader.setStrings(&shaderString, 1);

        // Prepare the environment
        const int VERSION = 460;
        shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, VERSION);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

        // Prepare for preprocessing
        auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
        if (!shader.parse(&resourceLimits_, VERSION, ECoreProfile, false, true, messages)) {
            QB_LOG_WARN("Failed to parse shader %s! %s\n%s", path, shader.getInfoLog(), shader.getInfoDebugLog());
            return {};
        }

        glslang::TProgram program;
        program.addShader(&shader);

        if (!program.link(messages)) {
            QB_LOG_WARN("Failed to parse shader %s! %s\n%s", path, program.getInfoLog(), program.getInfoDebugLog());
            return {};
        }

        std::vector<uint32_t> spirvBytecode;
        glslang::GlslangToSpv(*program.getIntermediate(language), spirvBytecode);

        return spirvBytecode;
    }

    const TBuiltInResource QbVkShaderCompiler::GetResourceLimits(const VkPhysicalDeviceLimits& limits) {
        return TBuiltInResource {
            /* .MaxLights = */ 32,
            /* .MaxClipPlanes = */ 6,
            /* .MaxTextureUnits = */ 32,
            /* .MaxTextureCoords = */ 32,
            /* .MaxVertexAttribs = */ static_cast<int>(limits.maxVertexInputAttributes),
            /* .MaxVertexUniformComponents = */ 4096,
            /* .MaxVaryingFloats = */ 64,
            /* .MaxVertexTextureImageUnits = */ 32,
            /* .MaxCombinedTextureImageUnits = */ 80,
            /* .MaxTextureImageUnits = */ 32,
            /* .MaxFragmentUniformComponents = */ 4096,
            /* .MaxDrawBuffers = */ 32,
            /* .MaxVertexUniformVectors = */ 128,
            /* .MaxVaryingVectors = */ 8,
            /* .MaxFragmentUniformVectors = */ 16,
            /* .MaxVertexOutputVectors = */ 16,
            /* .MaxFragmentInputVectors = */ 15,
            /* .MinProgramTexelOffset = */ -8,
            /* .MaxProgramTexelOffset = */ 7,
            /* .MaxClipDistances = */ static_cast<int>(limits.maxClipDistances),
            /* .MaxComputeWorkGroupCountX = */ static_cast<int>(limits.maxComputeWorkGroupCount[0]),
            /* .MaxComputeWorkGroupCountY = */ static_cast<int>(limits.maxComputeWorkGroupCount[1]),
            /* .MaxComputeWorkGroupCountZ = */ static_cast<int>(limits.maxComputeWorkGroupCount[2]),
            /* .MaxComputeWorkGroupSizeX = */ static_cast<int>(limits.maxComputeWorkGroupSize[0]),
            /* .MaxComputeWorkGroupSizeY = */ static_cast<int>(limits.maxComputeWorkGroupSize[1]),
            /* .MaxComputeWorkGroupSizeZ = */ static_cast<int>(limits.maxComputeWorkGroupSize[2]),
            /* .MaxComputeUniformComponents = */ 1024,
            /* .MaxComputeTextureImageUnits = */ 16,
            /* .MaxComputeImageUniforms = */ 8,
            /* .MaxComputeAtomicCounters = */ 8,
            /* .MaxComputeAtomicCounterBuffers = */ 1,
            /* .MaxVaryingComponents = */ 60,
            /* .MaxVertexOutputComponents = */ 64,
            /* .MaxGeometryInputComponents = */ static_cast<int>(limits.maxGeometryInputComponents),
            /* .MaxGeometryOutputComponents = */ static_cast<int>(limits.maxGeometryOutputComponents),
            /* .MaxFragmentInputComponents = */ static_cast<int>(limits.maxFragmentInputComponents),
            /* .MaxImageUnits = */ 8,
            /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
            /* .MaxCombinedShaderOutputResources = */ 8,
            /* .MaxImageSamples = */ 0,
            /* .MaxVertexImageUniforms = */ 0,
            /* .MaxTessControlImageUniforms = */ 0,
            /* .MaxTessEvaluationImageUniforms = */ 0,
            /* .MaxGeometryImageUniforms = */ 0,
            /* .MaxFragmentImageUniforms = */ 8,
            /* .MaxCombinedImageUniforms = */ 8,
            /* .MaxGeometryTextureImageUnits = */ 16,
            /* .MaxGeometryOutputVertices = */ static_cast<int>(limits.maxGeometryOutputVertices),
            /* .MaxGeometryTotalOutputComponents = */ static_cast<int>(limits.maxGeometryTotalOutputComponents),
            /* .MaxGeometryUniformComponents = */ 1024,
            /* .MaxGeometryVaryingComponents = */ 64,
            /* .MaxTessControlInputComponents = */ 128,
            /* .MaxTessControlOutputComponents = */ 128,
            /* .MaxTessControlTextureImageUnits = */ 16,
            /* .MaxTessControlUniformComponents = */ 1024,
            /* .MaxTessControlTotalOutputComponents = */ 4096,
            /* .MaxTessEvaluationInputComponents = */ 128,
            /* .MaxTessEvaluationOutputComponents = */ 128,
            /* .MaxTessEvaluationTextureImageUnits = */ 16,
            /* .MaxTessEvaluationUniformComponents = */ 1024,
            /* .MaxTessPatchComponents = */ 120,
            /* .MaxPatchVertices = */ 32,
            /* .MaxTessGenLevel = */ 64,
            /* .MaxViewports = */ 16,
            /* .MaxVertexAtomicCounters = */ 0,
            /* .MaxTessControlAtomicCounters = */ 0,
            /* .MaxTessEvaluationAtomicCounters = */ 0,
            /* .MaxGeometryAtomicCounters = */ 0,
            /* .MaxFragmentAtomicCounters = */ 8,
            /* .MaxCombinedAtomicCounters = */ 8,
            /* .MaxAtomicCounterBindings = */ 1,
            /* .MaxVertexAtomicCounterBuffers = */ 0,
            /* .MaxTessControlAtomicCounterBuffers = */ 0,
            /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
            /* .MaxGeometryAtomicCounterBuffers = */ 0,
            /* .MaxFragmentAtomicCounterBuffers = */ 1,
            /* .MaxCombinedAtomicCounterBuffers = */ 1,
            /* .MaxAtomicCounterBufferSize = */ 16384,
            /* .MaxTransformFeedbackBuffers = */ 4,
            /* .MaxTransformFeedbackInterleavedComponents = */ 64,
            /* .MaxCullDistances = */ static_cast<int>(limits.maxCullDistances),
            /* .MaxCombinedClipAndCullDistances = */ static_cast<int>(limits.maxCombinedClipAndCullDistances),
            /* .MaxSamples = */ 4,
            /* .maxMeshOutputVerticesNV = */ 256,
            /* .maxMeshOutputPrimitivesNV = */ 512,
            /* .maxMeshWorkGroupSizeX_NV = */ 32,
            /* .maxMeshWorkGroupSizeY_NV = */ 1,
            /* .maxMeshWorkGroupSizeZ_NV = */ 1,
            /* .maxTaskWorkGroupSizeX_NV = */ 32,
            /* .maxTaskWorkGroupSizeY_NV = */ 1,
            /* .maxTaskWorkGroupSizeZ_NV = */ 1,
            /* .maxMeshViewCountNV = */ 4,

            /* .limits = */ {
            /* .nonInductiveForLoops = */ 1,
            /* .whileLoops = */ 1,
            /* .doWhileLoops = */ 1,
            /* .generalUniformIndexing = */ 1,
            /* .generalAttributeMatrixVectorIndexing = */ 1,
            /* .generalVaryingIndexing = */ 1,
            /* .generalSamplerIndexing = */ 1,
            /* .generalVariableIndexing = */ 1,
            /* .generalConstantMatrixVectorIndexing = */ 1,
        } };
	}
}