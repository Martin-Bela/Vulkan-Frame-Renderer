#include "vulkan_display.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

std::string vulkan_display_error_message = "";

// vkCreateDebugUtilsMessengerEXT and DestroyDebugUtilsMessengerEXT are extension functions, 
// so their implementation is not included in static vulkan library and has to be loaded dynamically
VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pMessenger)
{
	auto implementation = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>
		(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	assert(vkCreateDebugUtilsMessengerEXT != nullptr);
	if (implementation != nullptr) {
		return implementation(instance, pCreateInfo, pAllocator, pMessenger);
	}
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}


void vkDestroyDebugUtilsMessengerEXT(VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger, 
	const VkAllocationCallbacks* pAllocator) 
{
	auto implementation = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>
		(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (implementation != nullptr) {
		implementation(instance, debugMessenger, pAllocator);
	}
}


namespace {
	using c_str = const char*;
	using namespace std::literals;

	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		std::cout << "validation layer: " << pCallbackData->pMessage << '\n' << std::endl;

		return VK_FALSE;
	}


	RETURN_VAL check_validation_layers(const std::vector<c_str>& required_layers) {
		std::vector<vk::LayerProperties>  layers;
		CHECKED_ASSIGN(layers, vk::enumerateInstanceLayerProperties());
		//for (auto& l : layers) puts(l.layerName);

		for (auto& req_layer : required_layers) {
			auto layer_equals = [req_layer](auto layer) { return strcmp(req_layer, layer.layerName) == 0; };
			bool found = std::any_of(layers.begin(), layers.end(), layer_equals);
			CHECK(found, "Layer "s + req_layer + " is not supported.");
		}
		return RETURN_VAL();
	}


	RETURN_VAL check_instance_extensions(const std::vector<c_str>& required_extensions) {
		std::vector<vk::ExtensionProperties> extensions;
		CHECKED_ASSIGN(extensions, vk::enumerateInstanceExtensionProperties(nullptr));

		for (auto& req_exten : required_extensions) {
			auto extension_equals = [req_exten](auto exten) { return strcmp(req_exten, exten.extensionName) == 0; };
			bool found = std::any_of(extensions.begin(), extensions.end(), extension_equals);
			CHECK(found, "Instance extension "s + req_exten + " is not supported.");
		}
		return RETURN_VAL();
	}


	RETURN_VAL check_device_extensions(const std::vector<c_str>& required_extensions, const vk::PhysicalDevice& device) {
		std::vector<vk::ExtensionProperties> extensions;
		CHECKED_ASSIGN(extensions, device.enumerateDeviceExtensionProperties(nullptr));

		for(auto & req_exten: required_extensions) {
			auto extension_equals = [req_exten](auto exten) { return strcmp(req_exten, exten.extensionName) == 0; };
			bool found = std::any_of(extensions.begin(), extensions.end(), extension_equals);
			CHECK(found, "Device extension "s + req_exten + " is not supported.");
		}
		return RETURN_VAL();
	}


	vk::PhysicalDevice choose_GPU(const std::vector<vk::PhysicalDevice>& gpus) {
		for (auto& gpu : gpus) {
			auto properties = gpu.getProperties();
			if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
				return gpu;
			}
		}

		for (auto& gpu : gpus) {
			auto properties = gpu.getProperties();
			if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
				return gpu;
			}
		}
		return gpus[0];
	}

	vk::CompositeAlphaFlagBitsKHR get_composite_alpha(vk::CompositeAlphaFlagsKHR capabilities) {
		using underlying = std::underlying_type_t<vk::CompositeAlphaFlagBitsKHR>;
		underlying result = 1;
		while (!(result & static_cast<underlying>(capabilities))) {
			result <<= 1;
		}
		return static_cast<vk::CompositeAlphaFlagBitsKHR>(result);
	}

	RETURN_VAL create_shader(vk::ShaderModule& shader, 
		const std::filesystem::path& file_path, 
		const vk::Device& device) 
	{
		std::ifstream file(file_path, std::ios::binary);
		CHECK(file.is_open(), "Failed to open file:"s + file_path.string());
		auto size = std::filesystem::file_size(file_path);
		assert(size % 4 == 0);
		std::vector<std::uint32_t> shader_code(size / 4);
		file.read(reinterpret_cast<char*>(shader_code.data()), size);
		CHECK(file.good(), "Error reading from file:"s + file_path.string());
		

		vk::ShaderModuleCreateInfo shader_info;
		shader_info.setCode(shader_code);
		CHECKED_ASSIGN(shader, device.createShaderModule(shader_info));
		return RETURN_VAL();
	}
} //namespace

RETURN_VAL Vulkan_display::create_instance(std::vector<c_str>& required_extensions) {
#ifdef _DEBUG
	std::vector validation_layers{ "VK_LAYER_KHRONOS_validation" };
	PASS_RESULT(check_validation_layers(validation_layers));
#else
	std::vector<c_str> validation_layers;
#endif
	const char* debug_extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	required_extensions.push_back(debug_extension);
	PASS_RESULT(check_instance_extensions(required_extensions));

	vk::ApplicationInfo app_info{};
	app_info.setApiVersion(VK_API_VERSION_1_0);

	vk::InstanceCreateInfo instance_info{};
	instance_info
		.setPApplicationInfo(&app_info)
		.setEnabledLayerCount(static_cast<uint32_t>(validation_layers.size()))
		.setPpEnabledLayerNames(validation_layers.data())
		.setEnabledExtensionCount(static_cast<uint32_t>(required_extensions.size()))
		.setPpEnabledExtensionNames(required_extensions.data());
	CHECKED_ASSIGN(instance, vk::createInstance(instance_info));

#ifdef _DEBUG
	PASS_RESULT(init_validation_layers_error_messenger());
#endif

	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::init_validation_layers_error_messenger() {
	vk::DebugUtilsMessengerCreateInfoEXT messenger_info{};
	using severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
	using type = vk::DebugUtilsMessageTypeFlagBitsEXT;
	messenger_info
		.setMessageSeverity(severity::eError | severity::eInfo | severity::eWarning) // severity::eInfo |
		.setMessageType(type::eGeneral | type::ePerformance | type::eValidation)
		.setPfnUserCallback(debugCallback)
		.setPUserData(nullptr);
	CHECKED_ASSIGN(messenger, instance.createDebugUtilsMessengerEXT(messenger_info));
	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::create_physical_device() {
	assert(instance != vk::Instance{});
	std::vector<vk::PhysicalDevice> gpus;
	CHECKED_ASSIGN(gpus, instance.enumeratePhysicalDevices());

	gpu = choose_GPU(gpus);
	std::cout << "Vulkan uses GPU called: " << gpu.getProperties().deviceName.data() << std::endl;
	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::get_queue_family_index() {
	assert(gpu != vk::PhysicalDevice{});

	std::vector<vk::QueueFamilyProperties> families = gpu.getQueueFamilyProperties();

	queue_family_index = UINT32_MAX;
	for (uint32_t i = 0; i < families.size(); i++) {
		VkBool32 surface_supported;
		CHECKED_ASSIGN(surface_supported, gpu.getSurfaceSupportKHR(i, surface));

		if (surface_supported && (families[i].queueFlags & vk::QueueFlagBits::eGraphics)) {
			queue_family_index = i;
			break;
		}
	}
	CHECK(queue_family_index != UINT32_MAX, "No suitable GPU queue found.");
	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::create_logical_device() {
	assert(gpu != vk::PhysicalDevice{});
	assert(queue_family_index != INT32_MAX);

	std::vector<c_str> required_extensions = { "VK_KHR_swapchain" };
	//VERIFY(check_device_extensions(required_extensions, gpu), "GPU doesn't support required xtensions.");

	float priorities[] = { 1.0 };
	vk::DeviceQueueCreateInfo queue_info{};
	queue_info
		.setQueueFamilyIndex(queue_family_index)
		.setPQueuePriorities(priorities)
		.setQueueCount(1);

	vk::DeviceCreateInfo device_info{};
	device_info
		.setQueueCreateInfoCount(1)
		.setPQueueCreateInfos(&queue_info)
		.setEnabledExtensionCount(static_cast<uint32_t>(required_extensions.size()))
		.setPpEnabledExtensionNames(required_extensions.data());

	CHECKED_ASSIGN(device, gpu.createDevice(device_info));
	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::get_present_mode() {
	std::vector<vk::PresentModeKHR> modes;
	CHECKED_ASSIGN(modes, gpu.getSurfacePresentModesKHR(surface));

	bool vsync = true;
	vk::PresentModeKHR first_choice{}, second_choice{};
	if (vsync) {
		first_choice = vk::PresentModeKHR::eFifo;
		second_choice = vk::PresentModeKHR::eFifoRelaxed;
	}
	else {
		first_choice = vk::PresentModeKHR::eMailbox;
		second_choice = vk::PresentModeKHR::eImmediate;
	}

	if (std::any_of(modes.begin(), modes.end(), [first_choice](auto mode) { return mode == first_choice; })) {
		swapchain_atributes.mode = first_choice;
		swapchain_atributes.mode = first_choice;
		return RETURN_VAL();
	}
	if (std::any_of(modes.begin(), modes.end(), [second_choice](auto mode) { return mode == second_choice; })) {
		swapchain_atributes.mode = second_choice;
		return RETURN_VAL();
	}
	swapchain_atributes.mode = modes[0];
	return RETURN_VAL();
}

RETURN_VAL Vulkan_display::get_surface_format() {
	std::vector<vk::SurfaceFormatKHR> formats;
	CHECKED_ASSIGN(formats, gpu.getSurfaceFormatsKHR(surface));

	vk::SurfaceFormatKHR default_format{};
	default_format.format = vk::Format::eB8G8R8A8Srgb;
	default_format.colorSpace = vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear;

	if (std::any_of(formats.begin(), formats.end(), [default_format](auto& format) {return format == default_format; })) {
		swapchain_atributes.format = default_format;
		return RETURN_VAL();
	}
	swapchain_atributes.format = formats[0];
	return RETURN_VAL();
}


 RETURN_VAL Vulkan_display::create_swap_chain() {
	auto& capabilities = swapchain_atributes.capabilities;
	CHECKED_ASSIGN(capabilities, gpu.getSurfaceCapabilitiesKHR(surface));

	PASS_RESULT(get_present_mode());
	PASS_RESULT(get_surface_format());

	image_size.width = std::clamp(image_size.width,
		capabilities.minImageExtent.width,
		capabilities.maxImageExtent.width);
	image_size.height = std::clamp(image_size.height,
		capabilities.minImageExtent.height,
		capabilities.maxImageExtent.height);

	uint32_t image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount != 0) {
		image_count = std::min(image_count, capabilities.maxImageCount);
	}

	//assert(capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst);
	vk::SwapchainCreateInfoKHR swapchain_info{};
	swapchain_info
		.setSurface(surface)
		.setImageFormat(swapchain_atributes.format.format)
		.setImageColorSpace(swapchain_atributes.format.colorSpace)
		.setPresentMode(swapchain_atributes.mode)
		.setMinImageCount(image_count)
		.setImageExtent(vk::Extent2D(800, 800))
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setImageSharingMode(vk::SharingMode::eExclusive)
		.setPreTransform(swapchain_atributes.capabilities.currentTransform)
		.setCompositeAlpha(get_composite_alpha(swapchain_atributes.capabilities.supportedCompositeAlpha))
		.setClipped(true)
		.setOldSwapchain(swapchain);
	CHECKED_ASSIGN(swapchain, device.createSwapchainKHR(swapchain_info));
	create_swapchain_images();
	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::create_swapchain_images() {
	std::vector<vk::Image> images;
	CHECKED_ASSIGN(images, device.getSwapchainImagesKHR(swapchain));
	uint32_t image_count = static_cast<uint32_t>(images.size());

	vk::ImageViewCreateInfo image_view_info{};
	image_view_info
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(swapchain_atributes.format.format);
	image_view_info.components
		.setR(vk::ComponentSwizzle::eIdentity)
		.setG(vk::ComponentSwizzle::eIdentity)
		.setB(vk::ComponentSwizzle::eIdentity)
		.setA(vk::ComponentSwizzle::eIdentity);
	image_view_info.subresourceRange
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(0)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(1);
	

	swapchain_images.resize(image_count);
	for (uint32_t i = 0; i < image_count; i++) {
		Swapchain_image& image = swapchain_images[i];
		image.image = std::move(images[i]);

		image_view_info.setImage(swapchain_images[i].image);
		CHECKED_ASSIGN(image.view, device.createImageView(image_view_info));
		
		
	}

	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::create_render_pass(){
	vk::RenderPassCreateInfo render_pass_info;

	vk::AttachmentDescription color_attachment;
	color_attachment
		.setFormat(swapchain_atributes.format.format)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
	render_pass_info.setAttachments(color_attachment);

	vk::AttachmentReference attachment_reference;
	attachment_reference
		.setAttachment(0)
		.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
	vk::SubpassDescription subpass;
	subpass
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setColorAttachments(attachment_reference);
	render_pass_info.setSubpasses(subpass);

	vk::SubpassDependency subpass_dependency{};
	subpass_dependency
		.setSrcSubpass(VK_SUBPASS_EXTERNAL)
		.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
	render_pass_info.setDependencies(subpass_dependency);

	render_pass = device.createRenderPass(render_pass_info);

	vk::ClearColorValue clear_color_value{};
	clear_color_value.setFloat32({ 0.3f, 0.3f, 0.3f, 1.0f });
	clear_color.setColor(clear_color_value);

	render_pass_begin_info
		.setRenderPass(render_pass)
		.setRenderArea(vk::Rect2D{ {0,0}, image_size })
		.setClearValues(clear_color);

	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::create_graphics_pipeline(){
	vk::PipelineLayoutCreateInfo pipeline_layout_info;
	CHECKED_ASSIGN(pipeline_layout, device.createPipelineLayout(pipeline_layout_info));


	vk::GraphicsPipelineCreateInfo pipeline_info{};
	
	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages_infos;
	shader_stages_infos[0]
		.setModule(vertex_shader)
		.setPName("main")
		.setStage(vk::ShaderStageFlagBits::eVertex);
	shader_stages_infos[1]
		.setModule(fragment_shader)
		.setPName("main")
		.setStage(vk::ShaderStageFlagBits::eFragment);
	pipeline_info.setStages(shader_stages_infos);
	
	vk::PipelineVertexInputStateCreateInfo vertex_input_state_info{};
	pipeline_info.setPVertexInputState(&vertex_input_state_info);
	
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_info{};
	input_assembly_state_info.setTopology(vk::PrimitiveTopology::eTriangleList);
	pipeline_info.setPInputAssemblyState(&input_assembly_state_info);

	vk::Viewport viewport{};
	viewport
		.setX(0.f)
		.setY(0.f)
		.setWidth(static_cast<float>(image_size.width))
		.setHeight(static_cast<float>(image_size.height))
		.setMinDepth(0.f)
		.setMaxDepth(1.f);
	vk::Rect2D scissor{};
	scissor
		.setOffset({0,0})
		.setExtent(image_size);
	vk::PipelineViewportStateCreateInfo viewport_state_info;
	viewport_state_info
		.setViewports(viewport)
		.setScissors(scissor);
	pipeline_info.setPViewportState(&viewport_state_info);

	vk::PipelineRasterizationStateCreateInfo rasterization_info{};
	rasterization_info
		.setPolygonMode(vk::PolygonMode::eFill)
		.setLineWidth(1.f);
	pipeline_info.setPRasterizationState(&rasterization_info);

	vk::PipelineMultisampleStateCreateInfo multisample_info;
	multisample_info
		.setSampleShadingEnable(false)
		.setRasterizationSamples(vk::SampleCountFlagBits::e1);
	pipeline_info.setPMultisampleState(&multisample_info);

	using color_flags = vk::ColorComponentFlagBits;
	vk::PipelineColorBlendAttachmentState color_blend_attachment{};
	color_blend_attachment
		.setBlendEnable(false)
		.setColorWriteMask(color_flags::eR | color_flags::eG | color_flags::eB | color_flags::eA);
	vk::PipelineColorBlendStateCreateInfo color_blend_info{};
	color_blend_info.setAttachments(color_blend_attachment);
	pipeline_info.setPColorBlendState(&color_blend_info);

	pipeline_info
		.setLayout(pipeline_layout)
		.setRenderPass(render_pass);

	vk::Result result;
	std::tie(result, pipeline) = device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
	CHECK(result, "Pipeline cannot be created.");
	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::create_framebuffers() {
	vk::FramebufferCreateInfo framebuffer_info;
	framebuffer_info
		.setRenderPass(render_pass)
		.setWidth(image_size.width)
		.setHeight(image_size.height)
		.setLayers(1);
	
	for (size_t i = 0; i < swapchain_images.size(); i++) {
		framebuffer_info.setAttachments(swapchain_images[i].view);
		CHECKED_ASSIGN(swapchain_images[i].framebuffer, device.createFramebuffer(framebuffer_info));
	}
	return RETURN_VAL();
}

RETURN_VAL Vulkan_display::create_concurrent_paths()
{
	vk::SemaphoreCreateInfo semaphore_info;

	vk::FenceCreateInfo fence_info{};
	fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);

	concurent_paths.resize(concurent_paths_count);

	for (auto& path : concurent_paths) {
		CHECKED_ASSIGN(path.image_acquired_semaphore, device.createSemaphore(semaphore_info));
		CHECKED_ASSIGN(path.image_rendered_semaphore, device.createSemaphore(semaphore_info));
		CHECKED_ASSIGN(path.path_available_fence, device.createFence(fence_info));
	}

	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::create_command_pool() {
	vk::CommandPoolCreateInfo pool_info{};
	using bits = vk::CommandPoolCreateFlagBits;
	pool_info
		.setQueueFamilyIndex(queue_family_index)
		.setFlags(bits::eTransient | bits::eResetCommandBuffer);
	CHECKED_ASSIGN(command_pool, device.createCommandPool(pool_info));
	return RETURN_VAL();
}


RETURN_VAL Vulkan_display::create_command_buffers() {
	vk::CommandBufferAllocateInfo allocate_info{};
	allocate_info
		.setCommandPool(command_pool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount(static_cast<uint32_t>(concurent_paths_count));
	CHECKED_ASSIGN(command_buffers, device.allocateCommandBuffers(allocate_info));
	return RETURN_VAL();
}

 RETURN_VAL Vulkan_display::init_vulkan(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
	this->surface = surface;
	this->image_size = vk::Extent2D{ width, height };
	// Order of following calls is important
	PASS_RESULT(create_physical_device());
	PASS_RESULT(get_queue_family_index());
	PASS_RESULT(create_logical_device());
	queue = device.getQueue(queue_family_index, 0);
	PASS_RESULT(create_swap_chain());
	PASS_RESULT(create_shader(vertex_shader, "shaders/vert.spv", device));
	PASS_RESULT(create_shader(fragment_shader, "shaders/frag.spv", device));
	PASS_RESULT(create_render_pass());
	PASS_RESULT(create_graphics_pipeline());
	PASS_RESULT(create_framebuffers());
	PASS_RESULT(create_command_pool());
	PASS_RESULT(create_command_buffers());
	PASS_RESULT(create_concurrent_paths());
	return RETURN_VAL();
}

 Vulkan_display::~Vulkan_display(){
	device.waitIdle();
	device.destroy(command_pool);

	for (auto& path : concurent_paths) {
		device.destroy(path.image_acquired_semaphore);
		device.destroy(path.image_rendered_semaphore);
		device.destroy(path.path_available_fence);
	}

	device.destroy(pipeline_layout);
	device.destroy(pipeline);
	for (auto& image : swapchain_images) {
		device.destroy(image.framebuffer);
	}
	device.destroy(render_pass);
	device.destroy(fragment_shader);
	device.destroy(vertex_shader);

	for (auto& image : swapchain_images) {
		device.destroy(image.view);
		//image.image is destroyed by swapchain
	}

	device.destroy(swapchain);
	instance.destroy(surface);
	device.destroy();
	instance.destroy(messenger);
	instance.destroy();
}


RETURN_VAL Vulkan_display::record_commands(unsigned current_path_id, uint32_t image_index) {
	render_pass_begin_info.setFramebuffer(swapchain_images[image_index].framebuffer);

	vk::CommandBuffer& cmd_buffer = command_buffers[current_path_id];
	cmd_buffer.reset();

	vk::CommandBufferBeginInfo begin_info{};
	PASS_RESULT(cmd_buffer.begin(begin_info));

	cmd_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
	cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	cmd_buffer.draw(6, 1, 0, 0);
	cmd_buffer.endRenderPass();

	PASS_RESULT(cmd_buffer.end());

	return RETURN_VAL();
 }

RETURN_VAL Vulkan_display::render() {
	Path& path = concurent_paths[current_path_id];

	CHECK(device.waitForFences(path.path_available_fence, VK_TRUE, UINT64_MAX),
		"Waiting for fence failed.");
	device.resetFences(path.path_available_fence);

	auto [acquired, image_index] = device.acquireNextImageKHR(swapchain, UINT64_MAX, path.image_acquired_semaphore, nullptr);
	CHECK(acquired, "Next swapchain image cannot be acquired.");
	Swapchain_image& swapchain_image = swapchain_images[image_index];

	record_commands(current_path_id, image_index);

	std::vector<vk::PipelineStageFlags> wait_masks{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
	vk::SubmitInfo submit_info{};
	submit_info
		.setCommandBuffers(command_buffers[current_path_id])
		.setWaitDstStageMask(wait_masks)
		.setWaitSemaphores(path.image_acquired_semaphore)
		.setSignalSemaphores(path.image_rendered_semaphore);
	
	PASS_RESULT(queue.submit(submit_info, path.path_available_fence));

	vk::PresentInfoKHR present_info{};
	present_info
		.setImageIndices(image_index)
		.setSwapchains(swapchain)
		.setWaitSemaphores(path.image_rendered_semaphore);
	CHECK(queue.presentKHR(present_info), "Error when presenting image.");
	
	current_path_id++;
	current_path_id %= concurent_paths_count;
	
	return RETURN_VAL();
}


