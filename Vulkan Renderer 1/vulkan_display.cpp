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

VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pMessenger)
{
	auto implementation = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>
		(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	assert(vkCreateDebugUtilsMessengerEXT != nullptr);
	return implementation(instance, pCreateInfo, pAllocator, pMessenger);
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

} //namespace

//---------------------------------------PRIVATE------------------------------------------------

RETURN_VAL vulkan_display::init_validation_layers_error_messenger() {
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


RETURN_VAL vulkan_display::create_physical_device() {
	assert(instance != vk::Instance{});
	std::vector<vk::PhysicalDevice> gpus;
	CHECKED_ASSIGN(gpus, instance.enumeratePhysicalDevices());

	gpu = choose_GPU(gpus);
	std::cout << "Vulkan uses GPU called: " << gpu.getProperties().deviceName.data() << std::endl;
	return RETURN_VAL();
}


RETURN_VAL vulkan_display::get_queue_family_index() {
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


RETURN_VAL vulkan_display::create_logical_device() {
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


RETURN_VAL vulkan_display::get_present_mode() {
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

RETURN_VAL vulkan_display::get_surface_format() {
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


 RETURN_VAL vulkan_display::create_swap_chain() {
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

	assert(capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst);

	vk::SwapchainCreateInfoKHR swapchain_info{};
	swapchain_info
		.setSurface(surface)
		.setImageFormat(swapchain_atributes.format.format)
		.setImageColorSpace(swapchain_atributes.format.colorSpace)
		.setPresentMode(swapchain_atributes.mode)
		.setMinImageCount(image_count)
		.setImageExtent(vk::Extent2D(800, 800))
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
		.setImageSharingMode(vk::SharingMode::eExclusive)
		.setPreTransform(swapchain_atributes.capabilities.currentTransform)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setClipped(true)
		.setOldSwapchain(swapchain);
	CHECKED_ASSIGN(swapchain, device.createSwapchainKHR(swapchain_info));
	create_swapchain_images();
	return RETURN_VAL();
}


RETURN_VAL vulkan_display::create_swapchain_images() {
	CHECKED_ASSIGN(swapchain_images, device.getSwapchainImagesKHR(swapchain));

	uint32_t image_count = static_cast<uint32_t>(swapchain_images.size());
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
	swapchain_image_views.resize(image_count);
	swapchain_image_fences.resize(image_count);
	for (uint32_t i = 0; i < image_count; i++) {
		image_view_info.setImage(swapchain_images[i]);
		CHECKED_ASSIGN(swapchain_image_views[i], device.createImageView(image_view_info));
		vk::FenceCreateInfo fence_info{};
		fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
		CHECKED_ASSIGN(swapchain_image_fences[i], device.createFence(fence_info));
	}
	return RETURN_VAL();
}


 RETURN_VAL vulkan_display::create_command_pool() {
	vk::CommandPoolCreateInfo pool_info{};
	pool_info.setQueueFamilyIndex(queue_family_index);
	CHECKED_ASSIGN(command_pool, device.createCommandPool(pool_info));
	return RETURN_VAL();
}


 RETURN_VAL vulkan_display::create_command_buffers() {
	vk::CommandBufferAllocateInfo allocate_info{};
	allocate_info
		.setCommandPool(command_pool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount(static_cast<uint32_t>(swapchain_images.size()));
	CHECKED_ASSIGN(command_buffers, device.allocateCommandBuffers(allocate_info));

	vk::ClearColorValue clear_color{};
	clear_color.setFloat32({ 1.f, 0.f, 0.f, 1.f });

	vk::ImageSubresourceRange image_range{};
	image_range
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setLayerCount(1)
		.setLevelCount(1);

	vk::ImageMemoryBarrier first_barrier{};
	first_barrier
		.setOldLayout(vk::ImageLayout::eUndefined)
		.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setSubresourceRange(image_range);

	vk::ImageMemoryBarrier second_barrier{};
	second_barrier
		.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
		.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setSubresourceRange(image_range);

	vk::CommandBufferBeginInfo begin_info{};
	for (uint32_t i = 0; i < command_buffers.size(); i++) {
		using p_flags = vk::PipelineStageFlagBits;
		first_barrier.setImage(swapchain_images[i]);
		second_barrier.setImage(swapchain_images[i]);
		PASS_RESULT(command_buffers[i].begin(begin_info));
		command_buffers[i].pipelineBarrier(p_flags::eTopOfPipe, p_flags::eTransfer, vk::DependencyFlagBits::eByRegion, {}, {}, { first_barrier });
		command_buffers[i].clearColorImage(swapchain_images[i], vk::ImageLayout::eTransferDstOptimal, clear_color, image_range);
		command_buffers[i].pipelineBarrier(p_flags::eTransfer, p_flags::eBottomOfPipe, vk::DependencyFlagBits::eByRegion, {}, {}, { second_barrier });
		PASS_RESULT(command_buffers[i].end());
	}
	return RETURN_VAL();
}



 //---------------------------------------PUBLIC------------------------------------------------

 RETURN_VAL vulkan_display::create_instance(std::vector<c_str>& required_extensions) {
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


 RETURN_VAL vulkan_display::init_vulkan(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
	this->surface = surface;
	this->image_size = vk::Extent2D{ width, height };
	// Order of following calls is important
	PASS_RESULT(create_physical_device());
	PASS_RESULT(get_queue_family_index());
	PASS_RESULT(create_logical_device());
	queue = device.getQueue(queue_family_index, 0);
	PASS_RESULT(create_swap_chain());
	PASS_RESULT(create_command_pool());
	PASS_RESULT(create_command_buffers());
	return RETURN_VAL();
}


 RETURN_VAL vulkan_display::render() {
	vk::SemaphoreCreateInfo semaphor_info{};

	vk::Semaphore image_available_semaphore;
	CHECKED_ASSIGN(image_available_semaphore, device.createSemaphore(semaphor_info));
	auto [acquired, image_index] = device.acquireNextImageKHR(swapchain, UINT64_MAX, image_available_semaphore, nullptr);
	CHECK(acquired, "Next swapchain image cannot be acquired.");

	vk::Semaphore image_prepared_semaphore;
	CHECKED_ASSIGN(image_prepared_semaphore, device.createSemaphore(semaphor_info));
	std::vector<vk::PipelineStageFlags> wait_masks{ vk::PipelineStageFlagBits::eAllCommands };
	vk::SubmitInfo submit_info{};
	submit_info
		.setCommandBuffers(command_buffers[image_index])
		.setSignalSemaphores(image_prepared_semaphore)
		.setWaitDstStageMask(wait_masks)
		.setWaitSemaphores(image_available_semaphore);

	CHECK(device.waitForFences(swapchain_image_fences[image_index], VK_TRUE, UINT64_MAX),
		"Waiting for fence...");
	device.resetFences(swapchain_image_fences[image_index]);
	
	PASS_RESULT(queue.submit(submit_info, swapchain_image_fences[image_index]));

	vk::PresentInfoKHR present_info{};
	present_info
		.setImageIndices(image_index)
		.setSwapchains(swapchain)
		.setWaitSemaphores(image_prepared_semaphore);
	CHECK(queue.presentKHR(present_info), "Error when presenting image.");
	return RETURN_VAL();
}


