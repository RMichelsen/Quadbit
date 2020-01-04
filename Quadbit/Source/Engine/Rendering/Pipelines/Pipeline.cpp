#include "Pipeline.h"

#include <SPIRV-Cross/spirv_cross.hpp>

#include "Engine/Core/Logging.h"
#include "Engine/Rendering/Shaders/ShaderInstance.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Memory/ResourceManager.h"
#include "Engine/Rendering/Shaders/ShaderCompiler.h"

namespace Quadbit {
    // Max instances here refers to the maximum number shader resource instances
    // Note: if fragmentPath is nullptr, the pipeline will be created with a vertex shader only
	QbVkPipeline::QbVkPipeline(QbVkContext& context, const char* vertexPath, const char* vertexEntry,
        const char* fragmentPath, const char* fragmentEntry, const QbVkPipelineDescription pipelineDescription,
        const VkRenderPass renderPass, const uint32_t maxInstances, 
        const eastl::vector<eastl::tuple<VkFormat, uint32_t>>& vertexAttributeOverride) : context_(context) {

        QB_ASSERT(vertexPath != nullptr && "Can't create pipeline, no vertex path specified");

        graphicsResources_ = eastl::make_unique<GraphicsResources>();
        graphicsResources_->vertexPath = vertexPath;
        graphicsResources_->vertexEntry = vertexEntry;
        graphicsResources_->fragmentPath = fragmentPath != nullptr ? fragmentPath : "";
        graphicsResources_->fragmentEntry = fragmentEntry != nullptr ? fragmentEntry : "";
        graphicsResources_->renderPass = renderPass;

        QbVkShaderInstance shaderInstance(context_);
        eastl::vector<eastl::vector<VkDescriptorSetLayoutBinding>> setLayoutBindings;
        eastl::vector<VkDescriptorPoolSize> poolSizes;

        // Parse shader descriptor set layouts and push constants
        // TODO: WHAT IF PUSH CONSTANTS ARE THE SAME??
        eastl::fixed_vector<VkPushConstantRange, 2> pushConstantRanges;

        auto vertexBytecode = context_.shaderCompiler->CompileShader(vertexPath, QbVkShaderType::QBVK_SHADER_TYPE_VERTEX);
        QB_ASSERT(!vertexBytecode.empty() && "Can't continue pipeline creation, shader compilation failed!");
        shaderInstance.AddShader(vertexBytecode.data(), vertexBytecode.size(), vertexEntry, VK_SHADER_STAGE_VERTEX_BIT);
        spirv_cross::Compiler vertexCompiler(vertexBytecode.data(), vertexBytecode.size());
        const auto vertexResources = vertexCompiler.get_shader_resources();
        ParseShader(vertexCompiler, vertexResources, setLayoutBindings, poolSizes, VK_SHADER_STAGE_VERTEX_BIT);
        if (!vertexResources.push_constant_buffers.empty()) {
            VkPushConstantRange range{};
            range.size = static_cast<uint32_t>(vertexCompiler.get_declared_struct_size(
                vertexCompiler.get_type(vertexResources.push_constant_buffers.front().base_type_id)));
            range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            pushConstantRanges.push_back(range);
        }

        if (fragmentPath != nullptr) {
            auto fragmentBytecode = context_.shaderCompiler->CompileShader(fragmentPath, QbVkShaderType::QBVK_SHADER_TYPE_FRAGMENT);
            QB_ASSERT(!fragmentBytecode.empty() && "Can't continue pipeline creation, shader compilation failed!");
            shaderInstance.AddShader(fragmentBytecode.data(), fragmentBytecode.size(), fragmentEntry, VK_SHADER_STAGE_FRAGMENT_BIT);
            spirv_cross::Compiler fragmentCompiler(fragmentBytecode.data(), fragmentBytecode.size());
            const auto fragmentResources = fragmentCompiler.get_shader_resources();
            ParseShader(fragmentCompiler, fragmentResources, setLayoutBindings, poolSizes, VK_SHADER_STAGE_FRAGMENT_BIT);
            if (!fragmentResources.push_constant_buffers.empty()) {
                VkPushConstantRange range{};
                range.size = static_cast<uint32_t>(fragmentCompiler.get_declared_struct_size(
                    fragmentCompiler.get_type(fragmentResources.push_constant_buffers.front().base_type_id)));
                range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                pushConstantRanges.push_back(range);
            }
        }

        // Create descriptor set layouts
        descriptorSetLayouts_.resize(setLayoutBindings.size());
        for (int i = 0; i < descriptorSetLayouts_.size(); i++) {
            auto setLayoutInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo(setLayoutBindings[i]);
            VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &setLayoutInfo, nullptr, &descriptorSetLayouts_[i]));
        }

        // Let the resource manager take care of the actual allocations
        if (!setLayoutBindings.empty()) {
            descriptorAllocator_ = context_.resourceManager->CreateDescriptorAllocator(descriptorSetLayouts_, poolSizes, maxInstances);

            // Get the descriptor set handle if there is only one instance of the shader resources
            if (maxInstances == 1) {
                mainDescriptors_ = GetNextDescriptorSetsHandle();
            }
        }

        if (!vertexResources.stage_inputs.empty()) {
            // Now we will parse the vertex attributes. Here we make the assumption that
            // all vectors are in 32-bit floating point format. That means a vec2 is an R32G32_SFLOAT,
            // a vec3 is an R32G32B32_SFLOAT etc. The user can override the vertex attribute formats
            // by placing the VkFormats in order, in the vertexAttributeOverride vector
            eastl::vector<VertexInput> vertexInput;
            for (const auto& input : vertexResources.stage_inputs) {
                auto location = vertexCompiler.get_decoration(input.id, spv::DecorationLocation);
                auto type = vertexCompiler.get_type(input.type_id);
                vertexInput.push_back({ location, type.vecsize * static_cast<uint32_t>(sizeof(float)) });
            }
            // We will sort the inputs so the first location is the first entry, 
            // this makes it easier to build the attribute descriptions
            const auto compare = [](const VertexInput& a, const VertexInput& b) {
                return a.location < b.location;
            };
            eastl::sort(vertexInput.begin(), vertexInput.end(), compare);

            auto offset = 0;
            for (int i = 0; i < vertexInput.size(); i++) {
                // Override the VkFormat in case necessary
                if (!vertexAttributeOverride.empty()) {
                    auto format = eastl::get<VkFormat>(vertexAttributeOverride[i]);
                    auto size = eastl::get<uint32_t>(vertexAttributeOverride[i]);
                    auto attribute = VkUtils::Init::VertexInputAttributeDescription(0, vertexInput[i].location, format, offset);
                    persistentPipelineInfo_.attributeDescriptions.push_back(attribute);
                    offset += size;
                    continue;
                }
                auto attribute =
                    VkUtils::Init::VertexInputAttributeDescription(0, vertexInput[i].location, VkUtils::GetVertexFormatFromSize(vertexInput[i].size), offset);
                persistentPipelineInfo_.attributeDescriptions.push_back(attribute);
                offset += vertexInput[i].size;
            }
            persistentPipelineInfo_.inputBindingDescription = VkUtils::Init::VertexInputBindingDescription(0, offset, VK_VERTEX_INPUT_RATE_VERTEX);

            persistentPipelineInfo_.vertexInputInfo =
                VkUtils::Init::PipelineVertexInputStateCreateInfo(persistentPipelineInfo_.attributeDescriptions, persistentPipelineInfo_.inputBindingDescription);
        }
        else {
            persistentPipelineInfo_.vertexInputInfo = VkPipelineVertexInputStateCreateInfo{};
            persistentPipelineInfo_.vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        }

        persistentPipelineInfo_.inputAssemblyInfo =
            VkUtils::Init::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        
        const bool defaultExtents = (pipelineDescription.viewportExtents.width == 0.0f && pipelineDescription.viewportExtents.height == 0.0f);
        float width = defaultExtents ? static_cast<float>(context_.swapchain.extent.width) : static_cast<float>(pipelineDescription.viewportExtents.width);
        float height = defaultExtents ? static_cast<float>(context_.swapchain.extent.height) : static_cast<float>(pipelineDescription.viewportExtents.height);

        persistentPipelineInfo_.viewport = pipelineDescription.viewportFlipped ?
            VkUtils::Init::FlippedViewport(width, height, 0.0f, 1.0f) :
            VkUtils::Init::Viewport(width, height, 0.0f, 1.0f);

        persistentPipelineInfo_.scissor = VkRect2D{};
        persistentPipelineInfo_.scissor.extent = context_.swapchain.extent;

        persistentPipelineInfo_.viewportInfo = VkUtils::Init::PipelineViewportStateCreateInfo();
        persistentPipelineInfo_.viewportInfo.viewportCount = 1;
        persistentPipelineInfo_.viewportInfo.pViewports = &persistentPipelineInfo_.viewport;
        persistentPipelineInfo_.viewportInfo.scissorCount = 1;
        persistentPipelineInfo_.viewportInfo.pScissors = &persistentPipelineInfo_.scissor;

        persistentPipelineInfo_.multisampleInfo = VkUtils::Init::PipelineMultisampleStateCreateInfo();
        persistentPipelineInfo_.multisampleInfo.minSampleShading = 1.0f;
        persistentPipelineInfo_.multisampleInfo.rasterizationSamples = pipelineDescription.enableMSAA ? context_.multisamplingResources.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

        if (pipelineDescription.colourBlending != QbVkPipelineColourBlending::QBVK_COLOURBLENDING_NOATTACHMENT) {
            persistentPipelineInfo_.colourBlendingAttachment = Presets::GetColorBlending(pipelineDescription.colourBlending);
            persistentPipelineInfo_.colourBlendInfo =
                VkUtils::Init::PipelineColorBlendStateCreateInfo(1, &persistentPipelineInfo_.colourBlendingAttachment);
        }
        else {
            persistentPipelineInfo_.colourBlendInfo = VkUtils::Init::PipelineColorBlendStateCreateInfo(0, nullptr);
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkUtils::Init::PipelineLayoutCreateInfo();
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts_.size());
        pipelineLayoutInfo.pSetLayouts = (!descriptorSetLayouts_.empty()) ? descriptorSetLayouts_.data() : nullptr;

        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = (!pushConstantRanges.empty()) ? pushConstantRanges.data() : nullptr;

        VK_CHECK(vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &pipelineLayout_));

        persistentPipelineInfo_.rasterizationInfo = Presets::GetRasterization(pipelineDescription.rasterization);
        persistentPipelineInfo_.depthStencilInfo = Presets::GetDepth(pipelineDescription.depth);
        persistentPipelineInfo_.dynamicStateInfo = Presets::GetDynamicState(pipelineDescription.dynamicState);;

        VkGraphicsPipelineCreateInfo pipelineInfo = VkUtils::Init::GraphicsPipelineCreateInfo();
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderInstance.stages.size());
        pipelineInfo.pStages = shaderInstance.stages.data();
        pipelineInfo.pVertexInputState = &persistentPipelineInfo_.vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &persistentPipelineInfo_.inputAssemblyInfo;
        pipelineInfo.pViewportState = &persistentPipelineInfo_.viewportInfo;
        pipelineInfo.pRasterizationState = &persistentPipelineInfo_.rasterizationInfo;
        pipelineInfo.pMultisampleState = &persistentPipelineInfo_.multisampleInfo;
        pipelineInfo.pDepthStencilState = &persistentPipelineInfo_.depthStencilInfo;
        pipelineInfo.pColorBlendState = &persistentPipelineInfo_.colourBlendInfo;
        pipelineInfo.pDynamicState = &persistentPipelineInfo_.dynamicStateInfo;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = renderPass;

        VK_CHECK(vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_));
	}

    QbVkPipeline::QbVkPipeline(QbVkContext& context, const char* computePath, const char* computeEntry,
        const void* specConstants, const uint32_t maxInstances) : context_(context) {
        compute_ = true;

        computeResources_ = eastl::make_unique<ComputeResources>();
        computeResources_->computePath = computePath;
        computeResources_->computeEntry = computeEntry;

        // Fence for compute sync
        VkFenceCreateInfo fenceCreateInfo = VkUtils::Init::FenceCreateInfo();
        VK_CHECK(vkCreateFence(context_.device, &fenceCreateInfo, nullptr, &computeResources_->computeFence));

        // Create command pool
        VkCommandPoolCreateInfo cmdPoolCreateInfo = VkUtils::Init::CommandPoolCreateInfo();
        cmdPoolCreateInfo.queueFamilyIndex = context_.gpu->computeFamilyIdx;
        cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(context_.device, &cmdPoolCreateInfo, nullptr, &computeResources_->commandPool));

        // Create command buffer
        VkCommandBufferAllocateInfo cmdBufferAllocateInfo = VkUtils::Init::CommandBufferAllocateInfo(computeResources_->commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
        VK_CHECK(vkAllocateCommandBuffers(context_.device, &cmdBufferAllocateInfo, &computeResources_->commandBuffer));

        // Create query pool for timestamp statistics
        VkQueryPoolCreateInfo queryPoolCreateInfo{};
        queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolCreateInfo.queryCount = 128;
        VK_CHECK(vkCreateQueryPool(context_.device, &queryPoolCreateInfo, nullptr, &computeResources_->queryPool));


        auto bytecode = context_.shaderCompiler->CompileShader(computePath, QbVkShaderType::QBVK_SHADER_TYPE_COMPUTE);
        QB_ASSERT(!bytecode.empty() && "Can't continue pipeline creation, shader compilation failed!");
        QbVkShaderInstance shaderInstance(context_);
        shaderInstance.AddShader(bytecode.data(), bytecode.size(), computeEntry, VK_SHADER_STAGE_COMPUTE_BIT);

        spirv_cross::Compiler compiler(bytecode.data(), bytecode.size());
        const auto resources = compiler.get_shader_resources();

        // Build the descriptor set layout
        eastl::vector<eastl::vector<VkDescriptorSetLayoutBinding>> setLayoutBindings;
        eastl::vector<VkDescriptorPoolSize> poolSizes;

        ParseShader(compiler, resources, setLayoutBindings, poolSizes, VK_SHADER_STAGE_COMPUTE_BIT);

        // Create descriptor set layouts
        descriptorSetLayouts_.resize(setLayoutBindings.size());
        for (int i = 0; i < descriptorSetLayouts_.size(); i++) {
            auto setLayoutInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo(setLayoutBindings[i]);
            VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &setLayoutInfo, nullptr, &descriptorSetLayouts_[i]));
        }

        // Let the resource manager take care of the actual allocations
        if (!setLayoutBindings.empty()) {
            descriptorAllocator_ = context_.resourceManager->CreateDescriptorAllocator(descriptorSetLayouts_, poolSizes, maxInstances);

            // Get the descriptor set handle if there is only one instance of the shader resources
            if (maxInstances == 1) {
                mainDescriptors_ = GetNextDescriptorSetsHandle();
            }
        }

        // Check for push constants
        eastl::fixed_vector<VkPushConstantRange, 1> pushConstantRanges;
        if (!resources.push_constant_buffers.empty()) {
            VkPushConstantRange range{};
            range.size = static_cast<uint32_t>(compiler.get_declared_struct_size(
                compiler.get_type(resources.push_constant_buffers.front().base_type_id)));
            range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            pushConstantRanges.push_back(range);
        }

        // Check for specialization constants
        // Only supports 4-byte specialization constants
        if (specConstants != nullptr) {
            auto constants = compiler.get_specialization_constants();
            for (const auto& sc : constants) {
                auto constant = compiler.get_constant(sc.id);
                auto type = compiler.get_type(constant.constant_type);
                QB_ASSERT((type.basetype == spirv_cross::SPIRType::Int || type.basetype == spirv_cross::SPIRType::UInt ||
                        type.basetype == spirv_cross::SPIRType::Boolean || type.basetype == spirv_cross::SPIRType::Float)
                        && "Unsupported specialization constant type!");

                VkSpecializationMapEntry entry;
                entry.constantID = sc.constant_id;
                entry.offset = sizeof(uint32_t) * sc.constant_id;
                entry.size = sizeof(uint32_t);
                persistentPipelineInfo_.specializationConstants.push_back(entry);
            }

            // We copy the specialization constant data to a local buffer to be
            // reused when rebuilding the pipeline
            auto byteSize = persistentPipelineInfo_.specializationConstants.size() * sizeof(uint32_t);
            persistentPipelineInfo_.specConstantsRawData.resize(byteSize);
            memcpy(persistentPipelineInfo_.specConstantsRawData.data(), specConstants, byteSize);

            persistentPipelineInfo_.specInfo.dataSize = byteSize;
            persistentPipelineInfo_.specInfo.mapEntryCount = static_cast<uint32_t>(persistentPipelineInfo_.specializationConstants.size());
            persistentPipelineInfo_.specInfo.pData = persistentPipelineInfo_.specConstantsRawData.data();
            persistentPipelineInfo_.specInfo.pMapEntries = persistentPipelineInfo_.specializationConstants.data();

            shaderInstance.stages[0].pSpecializationInfo = &persistentPipelineInfo_.specInfo;
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkUtils::Init::PipelineLayoutCreateInfo();
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts_.size());
        pipelineLayoutInfo.pSetLayouts = (!descriptorSetLayouts_.empty()) ? descriptorSetLayouts_.data() : nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = (!pushConstantRanges.empty()) ? pushConstantRanges.data() : nullptr;
        VK_CHECK(vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &pipelineLayout_));

        VkComputePipelineCreateInfo computePipelineCreateInfo = VkUtils::Init::ComputePipelineCreateInfo();
        computePipelineCreateInfo.layout = pipelineLayout_;
        computePipelineCreateInfo.stage = shaderInstance.stages[0];
        VK_CHECK(vkCreateComputePipelines(context_.device, nullptr, 1, &computePipelineCreateInfo, nullptr, &pipeline_));
    }

    QbVkPipeline::~QbVkPipeline() {
        vkDestroyPipelineLayout(context_.device, pipelineLayout_, nullptr);
        vkDestroyPipeline(context_.device, pipeline_, nullptr);

        if (!descriptorSetLayouts_.empty()) {
            for (auto& descriptorSetLayout : descriptorSetLayouts_) {
                vkDestroyDescriptorSetLayout(context_.device, descriptorSetLayout, nullptr);
            }
        }

        if (compute_) {
            vkFreeCommandBuffers(context_.device, computeResources_->commandPool, 1, &computeResources_->commandBuffer);
            vkDestroyCommandPool(context_.device, computeResources_->commandPool, nullptr);
            vkDestroyFence(context_.device, computeResources_->computeFence, nullptr);
            vkDestroyQueryPool(context_.device, computeResources_->queryPool, nullptr);
        }
    }

    void QbVkPipeline::Rebuild() {
        if (!compute_) {
            QbVkShaderInstance shaderInstance(context_);

            if (graphicsResources_->fragmentPath.compare("") != 0) {
                auto fragmentBytecode = context_.shaderCompiler->CompileShader(graphicsResources_->fragmentPath.c_str(),
                    QbVkShaderType::QBVK_SHADER_TYPE_FRAGMENT);
                if (fragmentBytecode.empty()) return;
                shaderInstance.AddShader(fragmentBytecode.data(), fragmentBytecode.size(), graphicsResources_->fragmentEntry.c_str(), VK_SHADER_STAGE_FRAGMENT_BIT);
            }
            auto vertexBytecode = context_.shaderCompiler->CompileShader(graphicsResources_->vertexPath.c_str(), 
                QbVkShaderType::QBVK_SHADER_TYPE_VERTEX);
            if (vertexBytecode.empty()) return;
            shaderInstance.AddShader(vertexBytecode.data(), vertexBytecode.size(), graphicsResources_->vertexEntry.c_str(), VK_SHADER_STAGE_VERTEX_BIT);

            VkGraphicsPipelineCreateInfo pipelineInfo = VkUtils::Init::GraphicsPipelineCreateInfo();
            pipelineInfo.stageCount = static_cast<uint32_t>(shaderInstance.stages.size());
            pipelineInfo.pStages = shaderInstance.stages.data();
            pipelineInfo.pVertexInputState = &persistentPipelineInfo_.vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &persistentPipelineInfo_.inputAssemblyInfo;
            pipelineInfo.pViewportState = &persistentPipelineInfo_.viewportInfo;
            pipelineInfo.pRasterizationState = &persistentPipelineInfo_.rasterizationInfo;
            pipelineInfo.pMultisampleState = &persistentPipelineInfo_.multisampleInfo;
            pipelineInfo.pDepthStencilState = &persistentPipelineInfo_.depthStencilInfo;
            pipelineInfo.pColorBlendState = &persistentPipelineInfo_.colourBlendInfo;
            pipelineInfo.pDynamicState = &persistentPipelineInfo_.dynamicStateInfo;
            pipelineInfo.layout = pipelineLayout_;
            pipelineInfo.renderPass = graphicsResources_->renderPass;

            vkDestroyPipeline(context_.device, pipeline_, nullptr);
            VK_CHECK(vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_));
        }
        else {
            auto bytecode = context_.shaderCompiler->CompileShader(computeResources_->computePath.c_str(), QbVkShaderType::QBVK_SHADER_TYPE_COMPUTE);
            if (bytecode.empty()) {
                return;
            }
            QbVkShaderInstance shaderInstance(context_);
            shaderInstance.AddShader(bytecode.data(), bytecode.size(), computeResources_->computeEntry.c_str(), VK_SHADER_STAGE_COMPUTE_BIT);

            VkComputePipelineCreateInfo computePipelineCreateInfo = VkUtils::Init::ComputePipelineCreateInfo();
            computePipelineCreateInfo.layout = pipelineLayout_;
            shaderInstance.stages[0].pSpecializationInfo = &persistentPipelineInfo_.specInfo;
            computePipelineCreateInfo.stage = shaderInstance.stages[0];

            vkDestroyPipeline(context_.device, pipeline_, nullptr);
            VK_CHECK(vkCreateComputePipelines(context_.device, nullptr, 1, &computePipelineCreateInfo, nullptr, &pipeline_));
        }
    }

    void QbVkPipeline::Bind(VkCommandBuffer& commandBuffer) {
        VkPipelineBindPoint bindPoint = compute_ ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
        vkCmdBindPipeline(commandBuffer, bindPoint, pipeline_);
    }

    void QbVkPipeline::BindDescriptorSets(VkCommandBuffer& commandBuffer, QbVkDescriptorSetsHandle descriptorSets) {
        // Allows us to write generic drawing code but if we try to bind descriptor sets for
        // a pipeline that doesn't have any, it just returns
        if (descriptorAllocator_ == QBVK_DESCRIPTOR_ALLOCATOR_NULL_HANDLE) return;

        QB_ASSERT(descriptorSets != QBVK_DESCRIPTOR_SETS_NULL_HANDLE || mainDescriptors_ != QBVK_DESCRIPTOR_SETS_NULL_HANDLE
            && "No bindable descriptor sets given!");

        VkPipelineBindPoint bindPoint = compute_ ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

        if (descriptorSets != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
            const auto& sets = context_.resourceManager->GetDescriptorSets(descriptorAllocator_, descriptorSets);
            vkCmdBindDescriptorSets(commandBuffer, bindPoint, pipelineLayout_, 0,
                static_cast<uint32_t>(sets.size()), sets.data(), static_cast<uint32_t>(uboSizes_.size()), uboSizes_.empty() ? 0 : uboSizes_.data());
        }
        else {
            const auto& sets = context_.resourceManager->GetDescriptorSets(descriptorAllocator_, mainDescriptors_);
            vkCmdBindDescriptorSets(commandBuffer, bindPoint, pipelineLayout_, 0,
                static_cast<uint32_t>(sets.size()), sets.data(),  static_cast<uint32_t>(uboSizes_.size()), uboSizes_.empty() ? 0 : uboSizes_.data());
        }
    }

    QbVkDescriptorSetsHandle QbVkPipeline::GetNextDescriptorSetsHandle() {
        return context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances.GetNextHandle();
    }

    void QbVkPipeline::Dispatch(uint32_t xGroups, uint32_t yGroups, uint32_t zGroups, const void* pushConstants,
        uint32_t pushConstantSize, QbVkDescriptorSetsHandle descriptorsHandle) {
        static VkMemoryBarrier memoryBarrier = VkUtils::Init::MemoryBarrierVk(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        // Make sure this is a compute pipeline..
        QB_ASSERT(compute_);

        VkCommandBufferBeginInfo cmdBufInfo = VkUtils::Init::CommandBufferBeginInfo();
        VK_CHECK(vkBeginCommandBuffer(computeResources_->commandBuffer, &cmdBufInfo));
        vkCmdResetQueryPool(computeResources_->commandBuffer, computeResources_->queryPool, 0, 128);
        vkCmdWriteTimestamp(computeResources_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, computeResources_->queryPool, 0);

        Bind(computeResources_->commandBuffer);
        BindDescriptorSets(computeResources_->commandBuffer, descriptorsHandle);
        if (pushConstantSize > 0 && pushConstants != nullptr) {
            vkCmdPushConstants(computeResources_->commandBuffer, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize, pushConstants);
        }

        vkCmdDispatch(computeResources_->commandBuffer, xGroups, yGroups, zGroups);

        if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE || mainDescriptors_ != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
            vkCmdPipelineBarrier(computeResources_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
        }

        vkCmdWriteTimestamp(computeResources_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, computeResources_->queryPool, 1);
        VK_CHECK(vkEndCommandBuffer(computeResources_->commandBuffer));

        VkSubmitInfo computeSubmitInfo = VkSubmitInfo{};
        computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers = &computeResources_->commandBuffer;

        VK_CHECK(vkQueueSubmit(context_.computeQueue, 1, &computeSubmitInfo, computeResources_->computeFence));

        VK_CHECK(vkWaitForFences(context_.device, 1, &computeResources_->computeFence, VK_TRUE, eastl::numeric_limits<uint64_t>().max()));
        VK_CHECK(vkResetFences(context_.device, 1, &computeResources_->computeFence));

        uint64_t timestamps[2]{};
        VK_CHECK(vkGetQueryPoolResults(context_.device, computeResources_->queryPool, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));

        double computeStart = double(timestamps[0]) * context_.gpu->deviceProps.limits.timestampPeriod * 1e-6;
        double computeEnd = double(timestamps[1]) * context_.gpu->deviceProps.limits.timestampPeriod * 1e-6;
        computeResources_->msAvgTime = computeResources_->msAvgTime * 0.95 + (computeEnd - computeStart) * 0.05;
        context_.computeAvgTimes[computeResources_->computePath] = computeResources_->msAvgTime;
    }

    // DispatchX is meant to solve a performance issue where multiple passes are necessary,
    // here we only bind the pipeline and descriptors once but dispatch X times in the same
    // commandbuffer. A push constant array can be populated with X push constant instances for
    // each dispatch
    void QbVkPipeline::DispatchX(uint32_t X, uint32_t xGroups, uint32_t yGroups, uint32_t zGroups, eastl::vector<const void*> pushConstantArray,
        uint32_t pushConstantSize, QbVkDescriptorSetsHandle descriptorsHandle) {
        static VkMemoryBarrier memoryBarrier = VkUtils::Init::MemoryBarrierVk(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        // Make sure this is a compute pipeline..
        QB_ASSERT(compute_);

        VkCommandBufferBeginInfo cmdBufInfo = VkUtils::Init::CommandBufferBeginInfo();
        VK_CHECK(vkBeginCommandBuffer(computeResources_->commandBuffer, &cmdBufInfo));
        vkCmdResetQueryPool(computeResources_->commandBuffer, computeResources_->queryPool, 0, 128);
        vkCmdWriteTimestamp(computeResources_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, computeResources_->queryPool, 0);

        Bind(computeResources_->commandBuffer);
        BindDescriptorSets(computeResources_->commandBuffer, descriptorsHandle);

        for (uint32_t i = 0; i < X; i++) {
            if (pushConstantSize > 0 && !pushConstantArray.empty()) {
                QB_ASSERT(pushConstantArray.size() == X);
                vkCmdPushConstants(computeResources_->commandBuffer, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize, pushConstantArray[i]);
            }
            vkCmdDispatch(computeResources_->commandBuffer, xGroups, yGroups, zGroups);
        }

        if (descriptorsHandle != QBVK_DESCRIPTOR_SETS_NULL_HANDLE || mainDescriptors_ != QBVK_DESCRIPTOR_SETS_NULL_HANDLE) {
            vkCmdPipelineBarrier(computeResources_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
        }

        vkCmdWriteTimestamp(computeResources_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, computeResources_->queryPool, 1);
        VK_CHECK(vkEndCommandBuffer(computeResources_->commandBuffer));

        VkSubmitInfo computeSubmitInfo = VkSubmitInfo{};
        computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers = &computeResources_->commandBuffer;

        VK_CHECK(vkQueueSubmit(context_.computeQueue, 1, &computeSubmitInfo, computeResources_->computeFence));

        VK_CHECK(vkWaitForFences(context_.device, 1, &computeResources_->computeFence, VK_TRUE, eastl::numeric_limits<uint64_t>().max()));
        VK_CHECK(vkResetFences(context_.device, 1, &computeResources_->computeFence));

        uint64_t timestamps[2]{};
        VK_CHECK(vkGetQueryPoolResults(context_.device, computeResources_->queryPool, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));

        double computeStart = double(timestamps[0]) * context_.gpu->deviceProps.limits.timestampPeriod * 1e-6;
        double computeEnd = double(timestamps[1]) * context_.gpu->deviceProps.limits.timestampPeriod * 1e-6;
        computeResources_->msAvgTime = computeResources_->msAvgTime * 0.95 + (computeEnd - computeStart) * 0.05;
        context_.computeAvgTimes[computeResources_->computePath] = computeResources_->msAvgTime;
    }

    void QbVkPipeline::BindResource(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const QbVkBufferHandle bufferHandle) {
        QB_ASSERT(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[descriptorSetsHandle];

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, &context_.resourceManager->buffers_[bufferHandle].descriptor, resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResource(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const QbVkTextureHandle textureHandle) {
        QB_ASSERT(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[descriptorSetsHandle];

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, &context_.resourceManager->textures_[textureHandle].descriptor, resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResource(const eastl::string name, const QbVkBufferHandle bufferHandle) {
        QB_ASSERT(mainDescriptors_ != QBVK_DESCRIPTOR_SETS_NULL_HANDLE && 
            "The pipeline has more than one set of shader resources, call GetNextDescriptorSetsHandle to manually retrieve one!");
        QB_ASSERT(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[mainDescriptors_];

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, &context_.resourceManager->buffers_[bufferHandle].descriptor, resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResource(const eastl::string name, const QbVkTextureHandle textureHandle) {
        QB_ASSERT(mainDescriptors_ != QBVK_DESCRIPTOR_SETS_NULL_HANDLE &&
            "The pipeline has more than one set of shader resources, call GetNextDescriptorSetsHandle to manually retrieve one!");
        QB_ASSERT(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[mainDescriptors_];

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, &context_.resourceManager->textures_[textureHandle].descriptor, resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResourceArray(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const eastl::vector<QbVkBufferHandle> bufferHandles) {
        QB_ASSERT(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[descriptorSetsHandle];

        eastl::vector<VkDescriptorBufferInfo> descriptors;
        for (const auto& handle : bufferHandles) {
            descriptors.push_back(context_.resourceManager->buffers_[handle].descriptor);
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, descriptors.data(), resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResourceArray(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const eastl::vector<QbVkTextureHandle> textureHandles) {
        QB_ASSERT(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[descriptorSetsHandle];

        eastl::vector<VkDescriptorImageInfo> descriptors;
        for (const auto& handle : textureHandles) {
            descriptors.push_back(context_.resourceManager->textures_[handle].descriptor);
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, descriptors.data(), resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResourceArray(const eastl::string name, const eastl::vector<QbVkBufferHandle> bufferHandles) {
        QB_ASSERT(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[mainDescriptors_];

        eastl::vector<VkDescriptorBufferInfo> descriptors;
        for (const auto& handle : bufferHandles) {
            descriptors.push_back(context_.resourceManager->buffers_[handle].descriptor);
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, descriptors.data(), resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResourceArray(const eastl::string name, const eastl::vector<QbVkTextureHandle> textureHandles) {
        QB_ASSERT(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[mainDescriptors_];

        eastl::vector<VkDescriptorImageInfo> descriptors;
        for (const auto& handle : textureHandles) {
            descriptors.push_back(context_.resourceManager->textures_[handle].descriptor);
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, descriptors.data(), resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::ParseShader(const spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& resources,
        eastl::vector<eastl::vector<VkDescriptorSetLayoutBinding>>& setLayoutBindings,
        eastl::vector<VkDescriptorPoolSize>& poolSizes, VkShaderStageFlags shaderStage) {

        const auto ParseResource = [&](const spirv_cross::Resource& resource, VkDescriptorType descriptorType) {
                uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                uint64_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

                eastl::string name = resource.name.c_str();

                uint32_t descriptorCount = 1;
                const spirv_cross::SPIRType type = compiler.get_type(resource.type_id);
                if (!type.array.empty()) {
                    QB_ASSERT(type.array.size() == 1 &&
                        "There is currently no support for multidimensional arrays in shaders!");
                    descriptorCount = type.array[0];
                }

                // Since we are allocating a descriptor set for each frame in flight
                // we are multiplying the amount of descriptor counts up front
                poolSizes.push_back(VkUtils::Init::DescriptorPoolSize(descriptorType, descriptorCount));

                if ((set + 1) > setLayoutBindings.size()) {
                    setLayoutBindings.resize(set + 1);
                }

                // If the resource has already been parsed but is used by multiple shaders,
                // just add the shader to the shaderflags and return
                for (auto& setBinding : setLayoutBindings[set]) {
                    if (setBinding.binding == binding) {
                        setBinding.stageFlags |= shaderStage;
                        return;
                    }
                }

                auto setBinding = VkUtils::Init::DescriptorSetLayoutBinding(binding,
                    descriptorType, shaderStage, descriptorCount);
                setLayoutBindings[set].push_back(setBinding);

                resourceInfo_[name] = ResourceInformation { descriptorType, static_cast<uint32_t>(set), binding, descriptorCount };
        };

        for (const auto& sampler : resources.sampled_images) ParseResource(sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        for (const auto& uniformBuffer : resources.uniform_buffers) {
            // For dynamic UBO's we store the aligned sizes in an array of uint32_t's used 
            // by the rendering systems for indexing when rendering. Each UBO needs N buffers
            // allocated, where N is the number of frames in flight. This way if we write directly
            // to a mapped UBO we don't write while the GPU is reading the same data
            const spirv_cross::SPIRType& type = compiler.get_type(uniformBuffer.base_type_id);
            uint32_t size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
            uboSizes_.push_back(VkUtils::GetDynamicUBOAlignment(context_, size));

            ParseResource(uniformBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
        }
        for (const auto& storageBuffer : resources.storage_buffers) ParseResource(storageBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        for (const auto& storageImage : resources.storage_images) ParseResource(storageImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }
}