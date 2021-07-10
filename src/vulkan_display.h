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

struct  Vulkan_display_exception {
	std::string message;
	inline Vulkan_display_exception() = default;
	inline Vulkan_display_exception(std::string msg) :
		message{std::move(msg)} { }
	inline const char* what() {
		return message.c_str();
	}
};

#define RETURN_VAL void

#define PASS_RESULT(expr) { expr; }

#define CHECK(expr, msg) { if (to_vk_result(expr) != vk::Result::eSuccess) throw Vulkan_display_exception{msg}; }

#define CHECKED_ASSIGN(variable, expr) { variable = expr; }


#endif //NO_EXCEPTIONS -------------------------------------------------------


class Vulkan_display {
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

	struct Swapchain_image {
		vk::Image image;
		vk::ImageView view;
		vk::Framebuffer framebuffer;
		vk::Fence* image_queue_fence;
	};
	std::vector<Swapchain_image> swapchain_images;

	vk::Extent2D image_size{ 0, 0 };
	vk::Viewport viewport;
	vk::Rect2D scissor;

	vk::ShaderModule vertex_shader;
	vk::ShaderModule fragment_shader;
	
	vk::RenderPass render_pass;
	vk::ClearValue clear_color;
	vk::RenderPassBeginInfo render_pass_begin_info;

	vk::Sampler sampler;
	vk::DescriptorSetLayout descriptor_set_layout;
	vk::DescriptorPool descriptor_pool;
	std::vector<vk::DescriptorSet> descriptor_sets;
	
	vk::PipelineLayout pipeline_layout;
	vk::Pipeline pipeline;

	vk::CommandPool command_pool;
	std::vector<vk::CommandBuffer> command_buffers;

	unsigned concurent_paths_count = 3;
	unsigned current_path_id = 0;
	struct Path {
		vk::Semaphore image_acquired_semaphore;
		vk::Semaphore image_rendered_semaphore;
		vk::Fence path_available_fence;
	};
	std::vector<Path> concurent_paths;

	vk::DeviceMemory transfer_image_memory;
public:
	struct Transfer_image {
		vk::Image image;
		vk::ImageView view;
		void* ptr;
		vk::ImageLayout layout = vk::ImageLayout::ePreinitialized;
		vk::AccessFlagBits access = vk::AccessFlagBits::eMemoryWrite;
	};
private:
	std::vector<Transfer_image> transfer_images;
	vk::DeviceSize transfer_image_size;

private:
	RETURN_VAL init_validation_layers_error_messenger();

	RETURN_VAL create_physical_device();

	RETURN_VAL get_queue_family_index();

	RETURN_VAL create_logical_device();

	RETURN_VAL get_present_mode();

	RETURN_VAL get_surface_format();

	RETURN_VAL create_swap_chain();

	RETURN_VAL create_swapchain_images();

	RETURN_VAL create_viewport_and_scissor();

	RETURN_VAL create_texture_sampler();

	RETURN_VAL create_descriptor_pool();

	RETURN_VAL create_description_sets();

	RETURN_VAL create_render_pass();

	RETURN_VAL create_descriptor_set_layout();

	RETURN_VAL create_pipeline_layout();

	RETURN_VAL create_graphics_pipeline();

	RETURN_VAL create_framebuffers();

	RETURN_VAL create_paths();

	RETURN_VAL create_command_pool();

	RETURN_VAL create_command_buffers();

	RETURN_VAL create_concurrent_paths();

	RETURN_VAL create_transfer_images(uint32_t width, uint32_t height, vk::Format format = vk::Format::eR8G8B8A8Srgb);

	RETURN_VAL record_graphics_commands(unsigned current_path_id, uint32_t image_index);
public:
	Vulkan_display() = default;

	~Vulkan_display();

	RETURN_VAL create_instance(std::vector<const char*>& required_extensions);

	const vk::Instance& get_instance() {
		return instance;
	}

	RETURN_VAL init_vulkan(VkSurfaceKHR surface, uint32_t width, uint32_t height);

	RETURN_VAL resize(uint32_t width, uint32_t height);

	RETURN_VAL render(unsigned char* frame, uint64_t size);

};
