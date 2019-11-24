#include "Pipeline.h"

#include <SPIRV-Cross/spirv_cross.hpp>

#include "Engine/Rendering/ShaderBytecode.h"
#include "Engine/Rendering/ShaderInstance.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Memory/ResourceManager.h"

namespace Quadbit {
    // Max instances here refers to the maximum number shader resource instances
	QbVkPipeline::QbVkPipeline(QbVkContext& context, const uint32_t* vertexBytecode, uint32_t vertexSize,
        const uint32_t* fragmentBytecode, uint32_t fragmentSize, const QbVkPipelineDescription pipelineDescription, 
        const uint32_t maxInstances, const eastl::vector<eastl::tuple<VkFormat, uint32_t>>& vertexAttributeOverride) : context_(context) {

        QbVkShaderInstance shaderInstance(context_);
        shaderInstance.AddShader(vertexBytecode, vertexSize, "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderInstance.AddShader(fragmentBytecode, fragmentSize, "main", VK_SHADER_STAGE_FRAGMENT_BIT);

        spirv_cross::Compiler vertexCompiler(vertexBytecode, vertexSize);
        spirv_cross::Compiler fragmentCompiler(fragmentBytecode, fragmentSize);
        const auto vertexResources = vertexCompiler.get_shader_resources();
        const auto fragmentResources = fragmentCompiler.get_shader_resources();
        
		// Build the descriptor set layout
		eastl::vector<eastl::vector<VkDescriptorSetLayoutBinding>> setLayoutBindings;
        eastl::vector<VkDescriptorPoolSize> poolSizes;

        ParseShader(vertexCompiler, vertexResources, setLayoutBindings, poolSizes, VK_SHADER_STAGE_VERTEX_BIT);
        ParseShader(fragmentCompiler, fragmentResources, setLayoutBindings, poolSizes, VK_SHADER_STAGE_FRAGMENT_BIT);

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
        const auto compare = [](const VertexInput & a, const VertexInput & b) {
            return a.location < b.location;
        };
        eastl::sort(vertexInput.begin(), vertexInput.end(), compare);

        eastl::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        auto offset = 0;
        for (int i = 0; i < vertexInput.size(); i++) {
            // Override the VkFormat in case necessary
            if (!vertexAttributeOverride.empty()) {
                auto [format, size] = vertexAttributeOverride[i];
                auto attribute = VkUtils::Init::VertexInputAttributeDescription(0, vertexInput[i].location, format, offset);
                attributeDescriptions.push_back(attribute);
                offset += size;
                continue;
            }
            auto attribute = 
                VkUtils::Init::VertexInputAttributeDescription(0, vertexInput[i].location, VkUtils::GetVertexFormatFromSize(vertexInput[i].size), offset);
            attributeDescriptions.push_back(attribute);
            offset += vertexInput[i].size;
        }
        VkVertexInputBindingDescription inputBindingDescription = VkUtils::Init::VertexInputBindingDescription(0, offset, VK_VERTEX_INPUT_RATE_VERTEX);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo =
            VkUtils::Init::PipelineVertexInputStateCreateInfo(attributeDescriptions, inputBindingDescription);

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo =
            VkUtils::Init::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        VkViewport viewport{};
        VkRect2D scissor{};
        viewport.width = static_cast<float>(context_.swapchain.extent.width);
        viewport.height = static_cast<float>(context_.swapchain.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        scissor.extent = context_.swapchain.extent;

        VkPipelineViewportStateCreateInfo viewportInfo = VkUtils::Init::PipelineViewportStateCreateInfo();
        viewportInfo.viewportCount = 1;
        viewportInfo.pViewports = &viewport;
        viewportInfo.scissorCount = 1;
        viewportInfo.pScissors = &scissor;

        VkPipelineMultisampleStateCreateInfo multisampleInfo =
            VkUtils::Init::PipelineMultisampleStateCreateInfo();
        multisampleInfo.minSampleShading = 1.0f;
        multisampleInfo.rasterizationSamples = pipelineDescription.enableMSAA ? context_.multisamplingResources.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

        auto colorBlendingInfo = Presets::GetColorBlending(pipelineDescription.colorBlending);
        VkPipelineColorBlendStateCreateInfo colorBlendInfo = 
            VkUtils::Init::PipelineColorBlendStateCreateInfo(1, &colorBlendingInfo);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkUtils::Init::PipelineLayoutCreateInfo();
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts_.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts_.data();

        eastl::fixed_vector<VkPushConstantRange, 2> pushConstantRanges;
        // Check for push constants
        if (!vertexResources.push_constant_buffers.empty()) {
            VkPushConstantRange range{};
            range.size = static_cast<uint32_t>(vertexCompiler.get_declared_struct_size(
                vertexCompiler.get_type(vertexResources.push_constant_buffers.front().base_type_id)));
            range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            pushConstantRanges.push_back(range);
        }
        if (!fragmentResources.push_constant_buffers.empty()) {
            VkPushConstantRange range{};
            range.size = static_cast<uint32_t>(fragmentCompiler.get_declared_struct_size(
                fragmentCompiler.get_type(fragmentResources.push_constant_buffers.front().base_type_id)));
            range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pushConstantRanges.push_back(range);
        }
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = (!pushConstantRanges.empty()) ? pushConstantRanges.data() : nullptr;
        VK_CHECK(vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &pipelineLayout_));

        auto rasterizationInfo = Presets::GetRasterization(pipelineDescription.rasterization);
        auto depthInfo = Presets::GetDepth(pipelineDescription.depth);
        auto dynamicStateInfo = Presets::GetDynamicState(pipelineDescription.dynamicState);;

        VkGraphicsPipelineCreateInfo pipelineInfo = VkUtils::Init::GraphicsPipelineCreateInfo();
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderInstance.stages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineInfo.pViewportState = &viewportInfo;
        pipelineInfo.pRasterizationState = &rasterizationInfo;
        pipelineInfo.pMultisampleState = &multisampleInfo;
        pipelineInfo.pDepthStencilState = &depthInfo;
        pipelineInfo.pColorBlendState = &colorBlendInfo;
        pipelineInfo.pDynamicState = &dynamicStateInfo;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = context_.mainRenderPass;

        VK_CHECK(vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_));
	}

    QbVkPipeline::~QbVkPipeline() {
        vkDestroyPipelineLayout(context_.device, pipelineLayout_, nullptr);
        vkDestroyPipeline(context_.device, pipeline_, nullptr);

        if (!descriptorSetLayouts_.empty()) {
            for (auto& descriptorSetLayout : descriptorSetLayouts_) {
                vkDestroyDescriptorSetLayout(context_.device, descriptorSetLayout, nullptr);
            }
        }
    }

    void QbVkPipeline::Bind(VkCommandBuffer& commandBuffer) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    }

    QbVkDescriptorSetsHandle QbVkPipeline::GetNextDescriptorSetsHandle() {
        return context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances.GetNextHandle();
    }

    void QbVkPipeline::BindResource(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const QbVkBufferHandle bufferHandle) {
        assert(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[descriptorSetsHandle];

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[i][resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, &context_.resourceManager->buffers_[bufferHandle].descriptor, resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResource(const QbVkDescriptorSetsHandle descriptorSetsHandle, const eastl::string name, const QbVkTextureHandle textureHandle) {
        assert(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[descriptorSetsHandle];

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[i][resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, &context_.resourceManager->textures_[textureHandle].descriptor, resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResource(const eastl::string name, const QbVkBufferHandle bufferHandle) {
        assert(mainDescriptors_ != QBVK_DESCRIPTOR_SETS_NULL_HANDLE && 
            "The pipeline has more than one set of shader resources, call GetNextDescriptorSetsHandle to manually retrieve one!");
        assert(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[mainDescriptors_];

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[i][resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, &context_.resourceManager->buffers_[bufferHandle].descriptor, resourceInfo.descriptorCount)
            };
            vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    void QbVkPipeline::BindResource(const eastl::string name, const QbVkTextureHandle textureHandle) {
        assert(mainDescriptors_ != QBVK_DESCRIPTOR_SETS_NULL_HANDLE &&
            "The pipeline has more than one set of shader resources, call GetNextDescriptorSetsHandle to manually retrieve one!");
        assert(resourceInfo_.find(name) != resourceInfo_.end() && "Resource name not found in pipeline shaders!");
        const auto resourceInfo = resourceInfo_[name];
        const auto& descriptorSets = context_.resourceManager->descriptorAllocators_[descriptorAllocator_].setInstances[mainDescriptors_];

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
                VkUtils::Init::WriteDescriptorSet(descriptorSets[i][resourceInfo.dstSet], resourceInfo.dstBinding,
                resourceInfo.descriptorType, &context_.resourceManager->textures_[textureHandle].descriptor, resourceInfo.descriptorCount)
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
                    assert(type.array.size() == 1 &&
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
        for (const auto& uniformBuffer : resources.uniform_buffers) ParseResource(uniformBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        for (const auto& storageBuffer : resources.storage_buffers) ParseResource(storageBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        for (const auto& storageImage : resources.storage_images) ParseResource(storageImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }
}