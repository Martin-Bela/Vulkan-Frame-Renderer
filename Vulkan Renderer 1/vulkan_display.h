#pragma once

#ifdef NO_EXCEPTIONS
#define VULKAN_HPP_NO_EXCEPTIONS

const char* vulkan_display_error_message = nullptr;

#define RETURN_VAL vk::Result

#define CHECK(expr) if ((expr) != vk::Result::eSuccess) return expr

#define GET_CHECKED(name, expr) auto[rv##__LINE__, name] = expr; if (rv##__LINE__ != vk::Result::eSuccess) { vulkan_display_error_message = msg; return rv##__LINE__; }

#define RETURN_SUCC return vk::Result::eSuccess;

#else
#include<exception>

class  vulkan_display_exception : public std::exception {

};

#define RETURN_VAL void

#define CHECK(expr, msg) if ((expr) != vk::Result::eSuccess) throw 

#define GET_CHECKED(name, expr) auto name = expr

#define RETURN_SUCC

#endif


#define VULKAN_HPP_TYPESAFE_CONVERSION
#include <vulkan/vulkan.hpp>


using c_str = const char*;


class vulkan_display {
	vk::Instance instance;

#ifdef _DEBUG
	vk::DebugUtilsMessengerEXT messenger;
#endif // _DEBUG

	vk::PhysicalDevice gpu;
	vk::Device device;

	uint32_t queue_family_index = UINT32_MAX;
	vk::Queue queue;

	vk::SurfaceKHR surface;
	vk::SwapchainKHR swapchain{ /* old swapchain */ nullptr };
	struct {
		vk::SurfaceCapabilitiesKHR capabilities;
		vk::SurfaceFormatKHR format;
		vk::PresentModeKHR mode = vk::PresentModeKHR::eFifo;
	} swapchain_atributes{};
	std::vector<vk::Image> swapchain_images;
	std::vector<vk::ImageView> swapchain_image_views;
	std::vector<vk::Fence> swapchain_image_fences;

	vk::Extent2D image_size{ 0, 0 };

	vk::CommandPool command_pool;
	std::vector<vk::CommandBuffer> command_buffers;
private:
	bool init_validation_layers_error_messenger();

	bool create_physical_device();

	bool get_queue_family_index();

	bool create_logical_device();

	bool get_present_mode();

	bool get_surface_format();

	bool create_swap_chain();

	bool create_swapchain_images();

	bool create_command_pool();

	bool create_command_buffers();

public:
	vulkan_display() = default;

	bool create_instance(std::vector<c_str>& required_extensions);

	inline const VkInstance& get_instance() {
		return instance;
	}

	bool init_vulkan(VkSurfaceKHR surface, uint32_t width, uint32_t height);

	bool render();
};