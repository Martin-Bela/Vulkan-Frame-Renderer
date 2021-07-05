#pragma once

#ifdef NO_EXCEPTIONS
#define VULKAN_HPP_NO_EXCEPTIONS
#endif

#define VULKAN_HPP_TYPESAFE_CONVERSION
#include <vulkan/vulkan.hpp>

#include<string>

inline vk::Result to_vk_result(bool b) {
	return b ? vk::Result::eSuccess : vk::Result::eErrorUnknown;
}

inline vk::Result to_vk_result(vk::Result res) {
	return res;
}

#ifdef NO_EXCEPTIONS //-------------------------------------------------------
//EXCEPTIONS AREE DISABLED
extern std::string vulkan_display_error_message;

#define RETURN_VAL vk::Result

#define PASS_RESULT(expr) { if (vk::Result res = expr; res != vk::Result::eSuccess) return res; }

#define CHECK(expr, msg) {                                           \
	vk::Result res = to_vk_result(expr);                             \
	if ( res != vk::Result::eSuccess) {                              \
		vulkan_display_error_message = msg;                          \
		return res;                                                  \
	}                                                                \
}

#define CHECKED_ASSIGN(variable, expr) {                             \
	auto[checked_assign_return_val, checked_assign_value] = expr;    \
	if (checked_assign_return_val != vk::Result::eSuccess) {         \
		return checked_assign_return_val;                            \
	} else {variable = std::move(checked_assign_value);}             \
}


#else //NO_EXCEPTIONS -------------------------------------------------------
//EXCEPTIONS ARE ENABLED
#include<exception>

struct  vulkan_display_exception {
	std::string message;
	inline vulkan_display_exception() = default;
	inline vulkan_display_exception(std::string msg) :
		message{std::move(msg)} { }
	inline const char* what() {
		return message.c_str();
	}
};

#define RETURN_VAL void

#define PASS_RESULT(expr) { expr; }

#define CHECK(expr, msg) { if (to_vk_result(expr) != vk::Result::eSuccess) throw vulkan_display_exception{msg}; }

#define CHECKED_ASSIGN(variable, expr) { variable = expr; }


#endif //NO_EXCEPTIONS -------------------------------------------------------


class vulkan_display {
	vk::Instance instance;

	vk::DebugUtilsMessengerEXT messenger;

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
	RETURN_VAL init_validation_layers_error_messenger();

	RETURN_VAL create_physical_device();

	RETURN_VAL get_queue_family_index();

	RETURN_VAL create_logical_device();

	RETURN_VAL get_present_mode();

	RETURN_VAL get_surface_format();

	RETURN_VAL create_swap_chain();

	RETURN_VAL create_swapchain_images();

	RETURN_VAL create_command_pool();

	RETURN_VAL create_command_buffers();

	RETURN_VAL create_render_pass();

	RETURN_VAL create_graphics_pipeline();

public:
	vulkan_display() = default;

	RETURN_VAL create_instance(std::vector<const char*>& required_extensions);

	inline const VkInstance& get_instance() {
		return instance;
	}

	RETURN_VAL init_vulkan(VkSurfaceKHR surface, uint32_t width, uint32_t height);

	RETURN_VAL render();
};



//-------------------------MAKRO_HELPER_FUNCTIONS---------------------
/*#define CONCATENATE_IMPL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_IMPL(x, y)
#define UNIQ(x) CONCATENATE(x, __LINE__)*/