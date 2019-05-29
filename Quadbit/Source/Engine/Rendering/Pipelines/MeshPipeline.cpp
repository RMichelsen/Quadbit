#include <PCH.h>
#include "MeshPipeline.h"

#include "../../Core/Time.h"
#include "../../Core/InputHandler.h"

#include "../Common/QbVkUtils.h"

#include "../../Entities/ComponentSystem.h"
#include "../../Entities/SystemDispatch.h"

#include "../Systems/NoClipCameraSystem.h"

namespace Quadbit {
	MeshPipeline::MeshPipeline(std::shared_ptr<QbVkContext> context) {
		this->context_ = context;
		this->entityManager_ = EntityManager::GetOrCreate();

		// Register the mesh component to be used by the ECS
		entityManager_->RegisterComponents<RenderMeshComponent, RenderTransformComponent, RenderCamera, CameraUpdateAspectRatioTag>();

		fallbackCamera_ = entityManager_->Create();
		fallbackCamera_.AddComponent<RenderCamera>(Quadbit::RenderCamera(305.0f, -50.0f, glm::vec3(-24.0f, 122.0f, 93.0f), 16.0f / 9.0f, 10000.0f));

		//CreateUniformBuffers();
		//CreateDescriptorPool();
		//CreateDescriptorSetLayout();
		//CreateDescriptorSets();

		CreatePipeline();
	}

	MeshPipeline::~MeshPipeline() {
		// Since both pipelines and layouts are destroyed by the swapchain we should check before deleting
		if(pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(context_->device, pipeline_, nullptr);
		if(pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(context_->device, pipelineLayout_, nullptr);

		vkDestroyDescriptorPool(context_->device, descriptorPool_, nullptr);
		vkDestroyDescriptorSetLayout(context_->device, descriptorSetLayout_, nullptr);

		for(auto&& vertexBuffer : meshBufs_.vertexBuffers_) {
			context_->allocator->DestroyBuffer(vertexBuffer);
		}
		for(auto&& indexBuffer : meshBufs_.indexBuffers_) {
			context_->allocator->DestroyBuffer(indexBuffer);
		}
	}

	void MeshPipeline::RebuildPipeline() {
		// Destroy old pipelines
		vkDestroyPipelineLayout(context_->device, pipelineLayout_, nullptr);
		vkDestroyPipeline(context_->device, pipeline_, nullptr);

		fallbackCamera_.AddComponent<CameraUpdateAspectRatioTag>();
		if(userCamera_.IsValid()) userCamera_.AddComponent<CameraUpdateAspectRatioTag>();

		// Create new pipeline
		CreatePipeline();
	}

	void MeshPipeline::DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer) {
		entityManager_->ForEach<RenderCamera, CameraUpdateAspectRatioTag>([&](Entity entity, auto& camera, auto& tag) noexcept {
			camera.perspective =
				glm::perspective(glm::radians(45.0f), static_cast<float>(context_->swapchain.extent.width) / static_cast<float>(context_->swapchain.extent.height), 0.1f, camera.viewDistance);
			camera.perspective[1][1] *= -1;
		});

		UpdateUniformBuffers(resourceIndex);

		if(!userCamera_.IsValid()) {
			entityManager_->systemDispatch_->RunSystem<NoClipCameraSystem>(Time::deltaTime);
		}

		Quadbit::RenderCamera* camera;
		userCamera_.IsValid() ? camera = userCamera_.GetComponentPtr<RenderCamera>() : camera = fallbackCamera_.GetComponentPtr<RenderCamera>();

		VkDeviceSize offsets[]{ 0 };
		entityManager_->ForEach<RenderMeshComponent, RenderTransformComponent>([&](Entity entity, RenderMeshComponent& mesh, RenderTransformComponent& transform) noexcept {
			vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

			mesh.dynamicData.model = transform.model;
			mesh.dynamicData.mvp = camera->perspective * camera->view * transform.model;
			vkCmdPushConstants(commandbuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(RenderMeshPushConstants), &mesh.dynamicData);

			vkCmdBindVertexBuffers(commandbuffer, 0, 1, &meshBufs_.vertexBuffers_[mesh.vertexHandle].buf, offsets);
			vkCmdBindIndexBuffer(commandbuffer, meshBufs_.indexBuffers_[mesh.indexHandle].buf, 0, VK_INDEX_TYPE_UINT32);

			//vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSets_[resourceIndex], 0, nullptr);
			vkCmdDrawIndexed(commandbuffer, mesh.indexCount, 1, 0, 0, 0);
		});
	}

	void MeshPipeline::CreateDescriptorPool() {
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

		VK_CHECK(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &descriptorPool_));
	}

	void MeshPipeline::CreateDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo();
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &uboLayoutBinding;

		VK_CHECK(vkCreateDescriptorSetLayout(context_->device, &layoutInfo, nullptr, &descriptorSetLayout_));
	}

	void MeshPipeline::CreateDescriptorSets() {
		std::vector<VkDescriptorSetLayout> layouts(context_->swapchain.images.size(), descriptorSetLayout_);
		VkDescriptorSetAllocateInfo descSetallocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
		descSetallocInfo.descriptorPool = descriptorPool_;
		descSetallocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
		descSetallocInfo.pSetLayouts = layouts.data();

		VK_CHECK(vkAllocateDescriptorSets(context_->device, &descSetallocInfo, descriptorSets_.data()));

		for(auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = uniformBuffers_[i].buf;
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(MainUBO);

			VkWriteDescriptorSet writeDesc = VkUtils::Init::WriteDescriptorSet();
			writeDesc.dstSet = descriptorSets_[i];
			writeDesc.dstBinding = 0;
			writeDesc.dstArrayElement = 0;
			writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDesc.descriptorCount = 1;
			writeDesc.pBufferInfo = &bufferInfo;
			writeDesc.pImageInfo = nullptr;
			writeDesc.pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(context_->device, 1, &writeDesc, 0, nullptr);
		}
	}

	void MeshPipeline::CreatePipeline() {
		// We start off by reading the shader bytecode from disk and creating the modules subsequently
		std::vector<char> vertexShaderBytecode = VkUtils::ReadShader("Resources/Shaders/Compiled/mesh_vert.spv");
		std::vector<char> fragmentShaderBytecode = VkUtils::ReadShader("Resources/Shaders/Compiled/mesh_frag.spv");
		VkShaderModule vertShaderModule = VkUtils::CreateShaderModule(vertexShaderBytecode, context_->device);
		VkShaderModule fragShaderModule = VkUtils::CreateShaderModule(fragmentShaderBytecode, context_->device);

		// This part specifies the two shader types used in the pipeline
		VkPipelineShaderStageCreateInfo shaderStageInfo[2] = {
			VkUtils::Init::PipelineShaderStageCreateInfo(),
			VkUtils::Init::PipelineShaderStageCreateInfo()
		};
		// Specifies type of shader
		shaderStageInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		// Specifies shader module to load
		shaderStageInfo[0].module = vertShaderModule;
		// "main" refers to the Entrypoint of a given shader module
		shaderStageInfo[0].pName = "main";

		// Similarly for the fragment shader
		shaderStageInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStageInfo[1].module = fragShaderModule;
		shaderStageInfo[1].pName = "main";

		// This part specifies the format of the vertex data passed to the vertex shader
		auto bindingDescription = MeshVertex::GetBindingDescription();
		auto attributeDescriptions = MeshVertex::GetAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo =
			VkUtils::Init::PipelineVertexInputStateCreateInfo();
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		// This part specifies the geometry drawn
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = VkUtils::Init::PipelineInputAssemblyStateCreateInfo();
		inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// This setting makes it possible to break up lines and triangles when using _STRIP topology modes
		inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		// This part creates the viewport and scissor and combines them in a struct holding the data for creation
		VkViewport viewport{};
		VkRect2D scissor{};
		// In our case the viewport covers the entire window
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(context_->swapchain.extent.width);
		viewport.height = static_cast<float>(context_->swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// And so does the scissor
		scissor.offset = { 0, 0 };
		scissor.extent = context_->swapchain.extent;

		// Onto the create info struct as usual
		VkPipelineViewportStateCreateInfo viewportInfo = VkUtils::Init::PipelineViewportStateCreateInfo();
		viewportInfo.viewportCount = 1;
		viewportInfo.pViewports = &viewport;
		viewportInfo.scissorCount = 1;
		viewportInfo.pScissors = &scissor;

		// This part specifies the rasterization stage
		VkPipelineRasterizationStateCreateInfo rasterizerInfo =
			VkUtils::Init::PipelineRasterizationStateCreateInfo();

		// Requires a GPU feature to be enabled, useful for special cases like shadow maps
		rasterizerInfo.depthClampEnable = VK_FALSE;

		// We will allow geometry to pass through the rasterizer
		rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;

		// Specifies how fragments are generated,
		// using any other option than FILL requires an appropriate GPU feature to be enabled
		// VK_POLYGON_MODE_FILL: fills the areas of the polygon with fragments
		// VK_POLYGON_MODE_LINE: draws the polygon edges as lines
		// VK_POLYGON_MODE_POINT: draws the polygon vertices as points
		rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;

		// Thickness of lines (in terms of fragments)
		rasterizerInfo.lineWidth = 1.0f;

		// Culling options and direction
		rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		// We won't be doing any altering of depth values in the rasterizer
		rasterizerInfo.depthBiasEnable = VK_FALSE;

		// This part specifies the multisampling stage
		VkPipelineMultisampleStateCreateInfo multisampleInfo =
			VkUtils::Init::PipelineMultisampleStateCreateInfo();
		// For now everything is default or disabled
		multisampleInfo.sampleShadingEnable = VK_FALSE;
		multisampleInfo.minSampleShading = 1.0f;
		multisampleInfo.rasterizationSamples = context_->multisamplingResources.msaaSamples;
		multisampleInfo.pSampleMask = nullptr;
		multisampleInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleInfo.alphaToOneEnable = VK_FALSE;

		// This part specifies the color blending 
		// We won't be blending colours for now
		VkPipelineColorBlendAttachmentState colorBlendAttachment =
			VkUtils::Init::PipelineColorBlendAttachmentState(
				VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
				VK_FALSE
			);

		VkPipelineColorBlendStateCreateInfo colorBlendInfo =
			VkUtils::Init::PipelineColorBlendStateCreateInfo(
				1, &colorBlendAttachment);

		// This part specifies the depth and stencil testing
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo =
			VkUtils::Init::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilInfo.minDepthBounds = 0.0f;
		depthStencilInfo.maxDepthBounds = 1.0f;
		depthStencilInfo.stencilTestEnable = VK_FALSE;
		depthStencilInfo.front = {};
		depthStencilInfo.back = {};

		// This part specifies the pipeline layout, its mainly used to specify the layout of uniform values used in shaders
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkUtils::Init::PipelineLayoutCreateInfo();
		// ENABLE UBO
		// pipelineLayoutInfo.setLayoutCount = 1;
		// pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;

		// ENABLE PUSHCONSTANTS
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(RenderMeshPushConstants);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		// Push constant ranges are only part of the main pipeline layout
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK(vkCreatePipelineLayout(context_->device, &pipelineLayoutInfo, nullptr, &pipelineLayout_));

		// It's finally time to create the actual pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo = VkUtils::Init::GraphicsPipelineCreateInfo();
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStageInfo;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineInfo.pViewportState = &viewportInfo;
		pipelineInfo.pRasterizationState = &rasterizerInfo;
		pipelineInfo.pMultisampleState = &multisampleInfo;
		pipelineInfo.pDepthStencilState = &depthStencilInfo;
		pipelineInfo.pColorBlendState = &colorBlendInfo;
		// IF USING DYNAMIC STATE REMEMBER TO CALL vkCmdSetViewport
		pipelineInfo.pDynamicState = nullptr;
		pipelineInfo.layout = pipelineLayout_;
		pipelineInfo.renderPass = context_->mainRenderPass;

		// It's possible to make derivatives of pipelines, 
		// we won't be doing that but these are the variables that need to be set
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		// Finally we can create the pipeline
		VK_CHECK(vkCreateGraphicsPipelines(context_->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_));
		vkDestroyShaderModule(context_->device, vertShaderModule, nullptr);
		vkDestroyShaderModule(context_->device, fragShaderModule, nullptr);
	}

	void MeshPipeline::DestroyVertexBuffer(VertexBufHandle handle) {
		context_->allocator->DestroyBuffer(meshBufs_.vertexBuffers_[handle]);
		meshBufs_.vertexBufferFreeList_.push_front(handle);
	}

	void MeshPipeline::DestroyIndexBuffer(IndexBufHandle handle) {
		context_->allocator->DestroyBuffer(meshBufs_.indexBuffers_[handle]);
		meshBufs_.indexBufferFreeList_.push_front(handle);
	}

	VertexBufHandle MeshPipeline::CreateVertexBuffer(const std::vector<MeshVertex>& vertices) {
		VertexBufHandle handle = meshBufs_.GetNextVertexHandle();

		VkDeviceSize bufferSize = static_cast<uint32_t>(vertices.size()) * sizeof(MeshVertex);

		QbVkBuffer stagingBuffer;
		context_->allocator->CreateStagingBuffer(stagingBuffer, bufferSize, vertices.data());

		VkBufferCreateInfo bufferInfo = VkUtils::Init::BufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		context_->allocator->CreateBuffer(meshBufs_.vertexBuffers_[handle], bufferInfo, QBVK_MEMORY_USAGE_GPU_ONLY);

		VkUtils::CopyBuffer(context_, stagingBuffer.buf, meshBufs_.vertexBuffers_[handle].buf, bufferSize);

		context_->allocator->DestroyBuffer(stagingBuffer);

		return handle;
	}

	IndexBufHandle MeshPipeline::CreateIndexBuffer(const std::vector<uint32_t>& indices) {
		IndexBufHandle handle = meshBufs_.GetNextIndexHandle();

		VkDeviceSize bufferSize = static_cast<uint32_t>(indices.size()) * sizeof(uint32_t);

		QbVkBuffer stagingBuffer;
		context_->allocator->CreateStagingBuffer(stagingBuffer, bufferSize, indices.data());

		VkBufferCreateInfo bufferInfo = VkUtils::Init::BufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		context_->allocator->CreateBuffer(meshBufs_.indexBuffers_[handle], bufferInfo, QBVK_MEMORY_USAGE_GPU_ONLY);

		VkUtils::CopyBuffer(context_, stagingBuffer.buf, meshBufs_.indexBuffers_[handle].buf, bufferSize);

		context_->allocator->DestroyBuffer(stagingBuffer);

		return handle;
	}

	void MeshPipeline::CreateUniformBuffers() {
		VkDeviceSize bufferSize = sizeof(MainUBO);
		VkBufferCreateInfo bufferInfo = VkUtils::Init::BufferCreateInfo(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		for(auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			context_->allocator->CreateBuffer(uniformBuffers_[i], bufferInfo, QBVK_MEMORY_USAGE_CPU_TO_GPU);
		}
	}

	void MeshPipeline::UpdateUniformBuffers(uint32_t resourceIndex) {
		RenderCamera* camera = userCamera_.GetComponentPtr<RenderCamera>();
		MainUBO* ubo = reinterpret_cast<MainUBO*>(uniformBuffers_[resourceIndex].alloc.data);
	}
}