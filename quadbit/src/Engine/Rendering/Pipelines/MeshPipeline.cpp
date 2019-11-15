#include "MeshPipeline.h"

#include <tiny_obj_loader.h>

#include "Engine/Core/Time.h"
#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Entities/SystemDispatch.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Systems/NoClipCameraSystem.h"
#include "Engine/Rendering/ShaderBytecode.h"

namespace Quadbit {
	MeshPipeline::MeshPipeline(QbVkContext& context) :
		context_(context) {

		// Register the mesh component to be used by the ECS
		context.entityManager->RegisterComponents<RenderMeshComponent, RenderTexturedObjectComponent, RenderTransformComponent,
			RenderMeshDeleteComponent, RenderCamera, CameraUpdateAspectRatioTag>();

		fallbackCamera_ = context.entityManager->Create();
		context.entityManager->AddComponent<RenderCamera>(fallbackCamera_, Quadbit::RenderCamera(145.0f, -42.0f, glm::vec3(130.0f, 190.0f, 25.0f), 16.0f / 9.0f, 10000.0f));

		CreateDescriptorPoolAndLayout();
		CreatePipeline();
	}

	MeshPipeline::~MeshPipeline() {
		// Since both pipelines and layouts are destroyed by the swapchain we should check before deleting
		if (pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(context_.device, pipeline_, nullptr);
		if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(context_.device, pipelineLayout_, nullptr);

		vkDestroyDescriptorPool(context_.device, descriptorPool_, nullptr);
		vkDestroyDescriptorSetLayout(context_.device, descriptorSetLayout_, nullptr);

		for (auto i = 0; i < meshBuffers_.vertexBufferIdx_; i++) {
			if (std::find(meshBuffers_.vertexBufferFreeList_.begin(), meshBuffers_.vertexBufferFreeList_.end(), i) == meshBuffers_.vertexBufferFreeList_.end()) {
				DestroyVertexBuffer(i);
			}
		}
		for (auto i = 0; i < meshBuffers_.indexBufferIdx_; i++) {
			if (std::find(meshBuffers_.indexBufferFreeList_.begin(), meshBuffers_.indexBufferFreeList_.end(), i) == meshBuffers_.indexBufferFreeList_.end()) {
				DestroyIndexBuffer(i);
			}
		}
		for (auto&& instance : externalInstances_) {
			DestroyInstance(instance.get());
		}
	}

	void MeshPipeline::RebuildPipeline() {
		// Destroy old pipelines
		vkDestroyPipelineLayout(context_.device, pipelineLayout_, nullptr);
		vkDestroyPipeline(context_.device, pipeline_, nullptr);

		context_.entityManager->AddComponent<CameraUpdateAspectRatioTag>(fallbackCamera_);
		if (userCamera_ != NULL_ENTITY && context_.entityManager->IsValid(userCamera_)) context_.entityManager->AddComponent<CameraUpdateAspectRatioTag>(userCamera_);

		// Create new pipeline
		CreatePipeline();
	}

	void MeshPipeline::DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer) {
		// Here we clean up meshes that are due for removal
		context_.entityManager->ForEachWithCommandBuffer<RenderMeshDeleteComponent>([&](Entity entity, EntityCommandBuffer* cmdBuf, RenderMeshDeleteComponent& mesh) noexcept {
			if (mesh.count == 0) {
				DestroyVertexBuffer(mesh.vertexHandle);
				DestroyIndexBuffer(mesh.indexHandle);
				cmdBuf->DestroyEntity(entity);
			}
			else {
				mesh.count--;
			}
			});

		context_.entityManager->ForEach<RenderCamera, CameraUpdateAspectRatioTag>([&](Entity entity, auto& camera, auto& tag) noexcept {
			camera.perspective =
				glm::perspective(glm::radians(45.0f), static_cast<float>(context_.swapchain.extent.width) / static_cast<float>(context_.swapchain.extent.height), 0.1f, camera.viewDistance);
			camera.perspective[1][1] *= -1;
			});

		if (userCamera_ == NULL_ENTITY || !context_.entityManager->IsValid(userCamera_)) {
			context_.entityManager->systemDispatch_->RunSystem<NoClipCameraSystem>(Time::deltaTime, context_.inputHandler);
		}

		Quadbit::RenderCamera* camera;
		(userCamera_ != NULL_ENTITY && context_.entityManager->IsValid(userCamera_)) ? 
			camera = context_.entityManager->GetComponentPtr<RenderCamera>(userCamera_) : 
			camera = context_.entityManager->GetComponentPtr<RenderCamera>(fallbackCamera_);

		// Update environment map if applicable, use camera stuff to do this
		if (environmentMap_ != NULL_ENTITY) {
			auto mesh = context_.entityManager->GetComponentPtr<Quadbit::RenderMeshComponent>(environmentMap_);
			EnvironmentMapPushConstants* pc = mesh->GetSafePushConstPtr<EnvironmentMapPushConstants>();
			pc->proj = camera->perspective;
			pc->proj[1][1] *= -1;

			// Invert pitch??
			camera->front.x = cos(glm::radians(camera->yaw)) * cos(glm::radians(-camera->pitch));
			camera->front.y = sin(glm::radians(-camera->pitch));
			camera->front.z = sin(glm::radians(camera->yaw)) * cos(glm::radians(-camera->pitch));
			camera->front = glm::normalize(camera->front);
			camera->view = glm::lookAt(camera->position, camera->position + camera->front, camera->up);
			pc->view = glm::mat4(glm::mat3(camera->view));

			// Reset
			camera->front.x = cos(glm::radians(camera->yaw)) * cos(glm::radians(camera->pitch));
			camera->front.y = sin(glm::radians(camera->pitch));
			camera->front.z = sin(glm::radians(camera->yaw)) * cos(glm::radians(camera->pitch));
			camera->front = glm::normalize(camera->front);
			camera->view = glm::lookAt(camera->position, camera->position + camera->front, camera->up);
		}

		VkDeviceSize offsets[]{ 0 };

		context_.entityManager->ForEach<RenderTexturedObjectComponent, RenderTransformComponent>(
			[&](Entity entity, RenderTexturedObjectComponent& obj, RenderTransformComponent& transform) noexcept {
			if (!VkUtils::QueryAsyncBuffer(context_, meshBuffers_.vertexBuffers_[obj.vertexHandle]) ||
				!VkUtils::QueryAsyncBuffer(context_, meshBuffers_.indexBuffers_[obj.indexHandle])) return;

			vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

			if (obj.pushConstantStride == -1) {
				RenderMeshPushConstants* pushConstants = obj.GetSafePushConstPtr<RenderMeshPushConstants>();
				pushConstants->model = transform.model;
				pushConstants->mvp = camera->perspective * camera->view * transform.model;
				vkCmdPushConstants(commandbuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(RenderMeshPushConstants), pushConstants);
			}
			else if (obj.pushConstantStride > 0) {
				vkCmdPushConstants(commandbuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, obj.pushConstantStride, obj.pushConstants.data());
			}

			vkCmdBindVertexBuffers(commandbuffer, 0, 1, &meshBuffers_.vertexBuffers_[obj.vertexHandle].buf, offsets);
			vkCmdBindIndexBuffer(commandbuffer, meshBuffers_.indexBuffers_[obj.indexHandle].buf, 0, VK_INDEX_TYPE_UINT32);

			vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSets_[obj.descriptorIndex][resourceIndex], 0, nullptr);
			vkCmdDrawIndexed(commandbuffer, obj.indexCount, 1, 0, 0, 0);
			});

		context_.entityManager->ForEach<RenderMeshComponent, RenderTransformComponent>(
			[&](Entity entity, RenderMeshComponent& mesh, RenderTransformComponent& transform) noexcept {
			if (!VkUtils::QueryAsyncBuffer(context_, meshBuffers_.vertexBuffers_[mesh.vertexHandle]) ||
				!VkUtils::QueryAsyncBuffer(context_, meshBuffers_.indexBuffers_[mesh.indexHandle])) return;

			vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh.instance->pipeline);
			// Dynamic update viewport and scissor for user-defined pipelines (also doesn't necessitate rebuilding the pipeline on window resize)
			VkViewport viewport{};
			VkRect2D scissor{};
			// In our case the viewport covers the entire window
			viewport.width = static_cast<float>(context_.swapchain.extent.width);
			viewport.height = static_cast<float>(context_.swapchain.extent.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
			// And so does the scissor
			scissor.offset = { 0, 0 };
			scissor.extent = context_.swapchain.extent;
			vkCmdSetScissor(commandbuffer, 0, 1, &scissor);

			if (mesh.pushConstantStride == -1) {
				RenderMeshPushConstants* pushConstants = mesh.GetSafePushConstPtr<RenderMeshPushConstants>();
				pushConstants->model = transform.model;
				pushConstants->mvp = camera->perspective * camera->view * transform.model;
				vkCmdPushConstants(commandbuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(RenderMeshPushConstants), pushConstants);
			}
			else if (mesh.pushConstantStride > 0) {
				vkCmdPushConstants(commandbuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, mesh.pushConstantStride, mesh.pushConstants.data());
			}

			vkCmdBindVertexBuffers(commandbuffer, 0, 1, &meshBuffers_.vertexBuffers_[mesh.vertexHandle].buf, offsets);
			vkCmdBindIndexBuffer(commandbuffer, meshBuffers_.indexBuffers_[mesh.indexHandle].buf, 0, VK_INDEX_TYPE_UINT32);

			if (mesh.instance->descriptorSetLayout != VK_NULL_HANDLE) {
				vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh.instance->pipelineLayout, 0, 1, &mesh.instance->descriptorSets[resourceIndex], 0, nullptr);
			}
			vkCmdDrawIndexed(commandbuffer, mesh.indexCount, 1, 0, 0, 0);
			});
	}

	void MeshPipeline::CreateDescriptorPoolAndLayout() {
		VkDescriptorPoolSize poolSize = VkUtils::Init::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = MAX_TEXTURES;

		VK_CHECK(vkCreateDescriptorPool(context_.device, &poolInfo, nullptr, &descriptorPool_));


		std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindings;
		descSetLayoutBindings.push_back(VkUtils::Init::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1));

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo();
		descSetLayoutCreateInfo.pBindings = descSetLayoutBindings.data();
		descSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descSetLayoutBindings.size());

		VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &descSetLayoutCreateInfo, nullptr, &descriptorSetLayout_));
	}

	void MeshPipeline::CreatePipeline() {
		QbVkShaderInstance shaderInstance = QbVkShaderInstance(context_);
		shaderInstance.AddShader(defaultVert.data(), static_cast<uint32_t>(defaultVert.size()), "main", VK_SHADER_STAGE_VERTEX_BIT);
		shaderInstance.AddShader(defaultFrag.data(), static_cast<uint32_t>(defaultFrag.size()), "main", VK_SHADER_STAGE_FRAGMENT_BIT);

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
		viewport.width = static_cast<float>(context_.swapchain.extent.width);
		viewport.height = static_cast<float>(context_.swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// And so does the scissor
		scissor.offset = { 0, 0 };
		scissor.extent = context_.swapchain.extent;

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
		multisampleInfo.sampleShadingEnable = VK_FALSE;
		multisampleInfo.minSampleShading = 1.0f;
		multisampleInfo.rasterizationSamples = context_.multisamplingResources.msaaSamples;
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
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;

		// ENABLE PUSHCONSTANTS
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(RenderMeshPushConstants);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		// Push constant ranges are only part of the main pipeline layout
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK(vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &pipelineLayout_));

		// It's finally time to create the actual pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo = VkUtils::Init::GraphicsPipelineCreateInfo();
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderInstance.stages.data();
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
		pipelineInfo.renderPass = context_.mainRenderPass;

		// It's possible to make derivatives of pipelines, 
		// we won't be doing that but these are the variables that need to be set
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		// Finally we can create the pipeline
		VK_CHECK(vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_));
	}

	void MeshPipeline::DestroyMesh(RenderMeshComponent& renderMeshComponent) {
		context_.entityManager->AddComponent<RenderMeshDeleteComponent>(context_.entityManager->Create(), 
			{ renderMeshComponent.vertexHandle, renderMeshComponent.indexHandle, MAX_FRAMES_IN_FLIGHT });
	}

	void MeshPipeline::DestroyVertexBuffer(VertexBufHandle handle) {
		context_.allocator->DestroyBuffer(meshBuffers_.vertexBuffers_[handle], context_.commandPool);
		meshBuffers_.vertexBuffers_[handle] = QbVkAsyncBuffer{};
		meshBuffers_.vertexBufferFreeList_.push_front(handle);
	}

	void MeshPipeline::DestroyIndexBuffer(IndexBufHandle handle) {
		context_.allocator->DestroyBuffer(meshBuffers_.indexBuffers_[handle], context_.commandPool);
		meshBuffers_.indexBuffers_[handle] = QbVkAsyncBuffer{};
		meshBuffers_.indexBufferFreeList_.push_front(handle);
	}

	VertexBufHandle MeshPipeline::CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount) {
		VertexBufHandle handle = meshBuffers_.GetNextVertexHandle();
		VkDeviceSize bufferSize = static_cast<uint64_t>(vertexCount) * vertexStride;

		VkUtils::CreateAsyncBuffer(context_, meshBuffers_.vertexBuffers_[handle], bufferSize, vertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		return handle;
	}

	IndexBufHandle MeshPipeline::CreateIndexBuffer(const std::vector<uint32_t>& indices) {
		IndexBufHandle handle = meshBuffers_.GetNextIndexHandle();
		VkDeviceSize bufferSize = static_cast<uint32_t>(indices.size()) * sizeof(uint32_t);

		VkUtils::CreateAsyncBuffer(context_, meshBuffers_.indexBuffers_[handle], bufferSize, indices.data(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

		return handle;
	}

	const QbVkRenderMeshInstance* MeshPipeline::CreateInstance(std::vector<QbVkRenderDescriptor>& descriptors, std::vector<QbVkVertexInputAttribute> vertexAttribs,
		QbVkShaderInstance& shaderInstance, int pushConstantStride, VkShaderStageFlags pushConstantShaderStage, VkBool32 depthTestingEnabled) {

		externalInstances_.push_back(std::make_unique<QbVkRenderMeshInstance>());
		QbVkRenderMeshInstance* renderMeshInstance = externalInstances_.back().get();

		if (!descriptors.empty()) {
			std::vector<VkDescriptorPoolSize> poolSizes;
			for (auto i = 0; i < descriptors.size(); i++) {
				poolSizes.push_back(VkUtils::Init::DescriptorPoolSize(descriptors[i].type, descriptors[i].count));
			}

			VkDescriptorPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
			poolInfo.pPoolSizes = poolSizes.data();
			poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

			VK_CHECK(vkCreateDescriptorPool(context_.device, &poolInfo, nullptr, &renderMeshInstance->descriptorPool));


			std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindings;
			for (auto i = 0; i < descriptors.size(); i++) {
				descSetLayoutBindings.push_back(VkUtils::Init::DescriptorSetLayoutBinding(i, descriptors[i].type, descriptors[i].shaderStage, descriptors[i].count));
			}

			VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo();
			descSetLayoutCreateInfo.pBindings = descSetLayoutBindings.data();
			descSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descSetLayoutBindings.size());

			VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &descSetLayoutCreateInfo, nullptr, &renderMeshInstance->descriptorSetLayout));


			std::vector<VkDescriptorSetLayout> layouts(context_.swapchain.images.size(), renderMeshInstance->descriptorSetLayout);
			VkDescriptorSetAllocateInfo descSetallocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
			descSetallocInfo.descriptorPool = renderMeshInstance->descriptorPool;
			descSetallocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
			descSetallocInfo.pSetLayouts = layouts.data();

			VK_CHECK(vkAllocateDescriptorSets(context_.device, &descSetallocInfo, renderMeshInstance->descriptorSets.data()));

			for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				std::vector<VkWriteDescriptorSet> writeDescSets;
				for (auto j = 0; j < descriptors.size(); j++) {
					if (descriptors[j].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || descriptors[j].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
						writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(renderMeshInstance->descriptorSets[i], descriptors[j].type, j,
							static_cast<VkDescriptorBufferInfo*>(descriptors[j].data), descriptors[j].count));

					}
					else if (descriptors[j].type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || descriptors[j].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
						writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(renderMeshInstance->descriptorSets[i], descriptors[j].type, j,
							static_cast<VkDescriptorImageInfo*>(descriptors[j].data), descriptors[j].count));
					}
				}

				vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
			}
		}

		// This part specifies the format of the vertex data passed to the vertex shader
		auto bindingDescription = VkUtils::GetVertexBindingDescription(vertexAttribs);
		auto attributeDescriptions = VkUtils::CreateVertexInputAttributeDescription(vertexAttribs);
		//MeshVertex::GetAttributeDescriptions();
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

		// Onto the create info struct as usual
		VkPipelineViewportStateCreateInfo viewportInfo = VkUtils::Init::PipelineViewportStateCreateInfo();
		viewportInfo.viewportCount = 1;
		viewportInfo.scissorCount = 1;

		// Dynamic states will be viewport and scissor for user-defined pipelines
		std::array<VkDynamicState, 2> dynamicStates{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicStateInfo = VkUtils::Init::PipelineDynamicStateCreateInfo();
		dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicStateInfo.pDynamicStates = dynamicStates.data();

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
		//rasterizerInfo.polygonMode = VK_POLYGON_MODE_LINE;

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
		multisampleInfo.sampleShadingEnable = VK_FALSE;
		multisampleInfo.minSampleShading = 1.0f;
		multisampleInfo.rasterizationSamples = context_.multisamplingResources.msaaSamples;
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
			VkUtils::Init::PipelineDepthStencilStateCreateInfo(depthTestingEnabled, depthTestingEnabled, VK_COMPARE_OP_LESS_OR_EQUAL);
		depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilInfo.minDepthBounds = 0.0f;
		depthStencilInfo.maxDepthBounds = 1.0f;
		depthStencilInfo.stencilTestEnable = VK_FALSE;
		depthStencilInfo.front = {};
		depthStencilInfo.back = {};

		// This part specifies the pipeline layout, its mainly used to specify the layout of uniform values used in shaders
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkUtils::Init::PipelineLayoutCreateInfo();
		// ENABLE UBO
		pipelineLayoutInfo.setLayoutCount = (descriptors.size() > 0) ? 1 : 0;
		pipelineLayoutInfo.pSetLayouts = (descriptors.size() > 0) ? &renderMeshInstance->descriptorSetLayout : nullptr;

		// ENABLE PUSHCONSTANTS
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.offset = 0;
		pushConstantRange.size = (pushConstantStride == -1) ? sizeof(RenderMeshPushConstants) : pushConstantStride;
		pushConstantRange.stageFlags = pushConstantShaderStage;
		// Push constant ranges are only part of the main pipeline layout
		pipelineLayoutInfo.pushConstantRangeCount = (pushConstantStride != 0) ? 1 : 0;
		pipelineLayoutInfo.pPushConstantRanges = (pushConstantStride != 0) ? &pushConstantRange : nullptr;
		VK_CHECK(vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &renderMeshInstance->pipelineLayout));

		// It's finally time to create the actual pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo = VkUtils::Init::GraphicsPipelineCreateInfo();
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderInstance.stages.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineInfo.pViewportState = &viewportInfo;
		pipelineInfo.pRasterizationState = &rasterizerInfo;
		pipelineInfo.pMultisampleState = &multisampleInfo;
		pipelineInfo.pDepthStencilState = &depthStencilInfo;
		pipelineInfo.pColorBlendState = &colorBlendInfo;
		// IF USING DYNAMIC STATE REMEMBER TO CALL vkCmdSetViewport
		pipelineInfo.pDynamicState = &dynamicStateInfo;
		pipelineInfo.layout = renderMeshInstance->pipelineLayout;
		pipelineInfo.renderPass = context_.mainRenderPass;

		// It's possible to make derivatives of pipelines, 
		// we won't be doing that but these are the variables that need to be set
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		// Finally we can create the pipeline
		VK_CHECK(vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &renderMeshInstance->pipeline));

		return renderMeshInstance;
	}

	void MeshPipeline::DestroyInstance(const QbVkRenderMeshInstance* instance) {
		// Destroy pipeline
		vkDestroyPipelineLayout(context_.device, instance->pipelineLayout, nullptr);
		vkDestroyPipeline(context_.device, instance->pipeline, nullptr);

		// Destroy descriptors
		if (instance->descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(context_.device, instance->descriptorPool, nullptr);
		if (instance->descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(context_.device, instance->descriptorSetLayout, nullptr);
	}

	Entity MeshPipeline::GetActiveCamera() {
		return (userCamera_ == NULL_ENTITY) ? fallbackCamera_ : userCamera_;
	}

	RenderTexturedObjectComponent MeshPipeline::CreateObject(const char* objPath, const char* texturePath, VkFormat textureFormat) {
		// First load the texture
		VkSamplerCreateInfo samplerInfo = VkUtils::Init::SamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_TRUE, 16.0f, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR);

		QbVkTexture texture = VkUtils::LoadTexture(context_, texturePath, textureFormat, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY, &samplerInfo);

		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout_);
		VkDescriptorSetAllocateInfo descSetallocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
		descSetallocInfo.descriptorPool = descriptorPool_;
		descSetallocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
		descSetallocInfo.pSetLayouts = layouts.data();

		VK_CHECK(vkAllocateDescriptorSets(context_.device, &descSetallocInfo, descriptorSets_[textureCount].data()));

		for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			std::vector<VkWriteDescriptorSet> writeDescSets;
			writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(descriptorSets_[textureCount][i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &texture.descriptor));
			vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
		}


		// Now load the model
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objPath)) {
			QB_LOG_ERROR("Failed to load object from file: %s\n", objPath);
			return {};
		}

		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;

		for (auto& shape : shapes) {
			for (auto& index : shape.mesh.indices) {
				MeshVertex v;
				v.position = {
					attrib.vertices[3 * static_cast<uint64_t>(index.vertex_index) + 0],
					attrib.vertices[3 * static_cast<uint64_t>(index.vertex_index) + 1],
					attrib.vertices[3 * static_cast<uint64_t>(index.vertex_index) + 2],
				};
				if (attrib.normals.empty()) {
					v.normal = {
						1.0f, 1.0f, 1.0f
					};
				}
				else {
					v.normal = {
						attrib.normals[3 * static_cast<uint64_t>(index.normal_index) + 0],
						attrib.normals[3 * static_cast<uint64_t>(index.normal_index) + 1],
						attrib.normals[3 * static_cast<uint64_t>(index.normal_index) + 2],
					};
				}
				v.uv = {
					attrib.texcoords[2 * static_cast<uint64_t>(index.texcoord_index) + 0],
					1.0f - attrib.texcoords[2 * static_cast<uint64_t>(index.texcoord_index) + 1]
				};

				vertices.push_back(v);
				indices.push_back(static_cast<uint32_t>(indices.size()));
			}
		}

		return RenderTexturedObjectComponent{
			CreateVertexBuffer(vertices.data(), sizeof(MeshVertex), static_cast<uint32_t>(vertices.size())),
			CreateIndexBuffer(indices),
			static_cast<uint32_t>(indices.size()),
			textureCount++,
			std::array<float, 32>(),
			-1
		};
	}

	void MeshPipeline::LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour) {
		std::vector<Quadbit::QbVkVertexInputAttribute> vertexModel{
			Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_POSITION,
			Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_COLOUR
		};

		QbVkShaderInstance shaderInstance(context_);
		shaderInstance.AddShader(gradientSkyboxVert.data(), static_cast<uint32_t>(gradientSkyboxVert.size()), "main", VK_SHADER_STAGE_VERTEX_BIT);
		shaderInstance.AddShader(gradientSkyboxFrag.data(), static_cast<uint32_t>(gradientSkyboxFrag.size()), "main", VK_SHADER_STAGE_FRAGMENT_BIT);

		std::vector<QbVkRenderDescriptor> empty{};
		const QbVkRenderMeshInstance* instance = CreateInstance(empty, vertexModel, shaderInstance, sizeof(EnvironmentMapPushConstants), VK_SHADER_STAGE_VERTEX_BIT, VK_FALSE);

		const std::vector<SkyGradientVertex> cubeVertices = {
			{{-1.0f, -1.0f, 1.0f},	topColour},
			{{1.0f, -1.0f, 1.0f},	topColour},
			{{1.0f, 1.0f, 1.0f},	botColour},
			{{-1.0f, 1.0f, 1.0f},	botColour},

			{{-1.0f, -1.0f, -1.0f}, topColour},
			{{1.0f, -1.0f, -1.0f},	topColour},
			{{1.0f, 1.0f, -1.0f},	botColour},
			{{-1.0f, 1.0f, -1.0f},	botColour}
		};

		const std::vector<uint32_t> cubeIndices = {
			0, 1, 2,
			2, 3, 0,
			1, 5, 6,
			6, 2, 1,
			7, 6, 5,
			5, 4, 7,
			4, 0, 3,
			3, 7, 4,
			4, 5, 1,
			1, 0, 4,
			3, 2, 6,
			6, 7, 3
		};

		environmentMap_ = context_.entityManager->Create();
		context_.entityManager->AddComponent<RenderMeshComponent>(environmentMap_, {
			CreateVertexBuffer(cubeVertices.data(), sizeof(SkyGradientVertex), static_cast<uint32_t>(cubeVertices.size())),
			CreateIndexBuffer(cubeIndices),
			static_cast<uint32_t>(cubeIndices.size()),
			std::array<float, 32>(),
			sizeof(EnvironmentMapPushConstants),
			instance
			});
		context_.entityManager->AddComponent<Quadbit::RenderTransformComponent>(environmentMap_, 
			Quadbit::RenderTransformComponent(1.0f, { 0.0f, 0.0f, 0.0f }, { 0, 0, 0, 1 }));
	}
}