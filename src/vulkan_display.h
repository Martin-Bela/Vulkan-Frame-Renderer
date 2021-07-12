#include "vulkan_context.h"
#include <utility>

class Window_inteface {
public:
	virtual std::pair<uint32_t, uint32_t> get_window_size() = 0;
};



class Vulkan_display {
	Window_inteface* window;
	vulkan_display_detail::Vulkan_context context;
	vk::Device device;

	vk::Viewport viewport;
	vk::Rect2D scissor;

	vk::ShaderModule vertex_shader;
	vk::ShaderModule fragment_shader;
	
	vk::RenderPass render_pass;
	vk::ClearValue clear_color;

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

	struct Transfer_image {
		vk::Image image;
		vk::ImageView view;
		void* ptr;
		vk::ImageLayout layout = vk::ImageLayout::ePreinitialized;
		vk::AccessFlagBits access = vk::AccessFlagBits::eMemoryWrite;
	};
	std::vector<Transfer_image> transfer_images;
	
	vk::Extent2D transfer_image_size;
	vk::DeviceSize transfer_image_byte_size;
	struct {
		uint32_t x;
		uint32_t y;
		uint32_t width;
		uint32_t height;
	} render_area;
private:
	vk::ImageMemoryBarrier create_memory_barrier(
		Vulkan_display::Transfer_image& image,
		vk::ImageLayout new_layout,
		vk::AccessFlagBits new_access_mask,
		uint32_t src_queue_family_index = VK_QUEUE_FAMILY_IGNORED,
		uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED);


	RETURN_VAL create_texture_sampler();

	RETURN_VAL create_descriptor_pool();

	RETURN_VAL create_description_sets();

	RETURN_VAL create_render_pass();

	RETURN_VAL create_descriptor_set_layout();

	RETURN_VAL create_pipeline_layout();

	RETURN_VAL create_graphics_pipeline();

	RETURN_VAL create_paths();

	RETURN_VAL create_command_pool();

	RETURN_VAL create_command_buffers();

	RETURN_VAL create_concurrent_paths();

	RETURN_VAL create_transfer_images(uint32_t width, uint32_t height, vk::Format format = vk::Format::eR8G8B8A8Srgb);

	RETURN_VAL record_graphics_commands(unsigned current_path_id, uint32_t image_index);

	RETURN_VAL update_render_area();
public:
	Vulkan_display() = default;

	~Vulkan_display();

	RETURN_VAL create_instance(std::vector<const char*>& required_extensions) {
		context.create_instance(required_extensions);
	}

	const vk::Instance& get_instance() {
		return context.instance;
	}

	RETURN_VAL init(VkSurfaceKHR surface, Window_inteface* window);

	RETURN_VAL resize_window() {
		auto [width, height] = window->get_window_size();
		context.recreate_swapchain(vk::Extent2D{width, height}, render_pass);
		update_render_area();
		return RETURN_VAL();
	}

	RETURN_VAL render(unsigned char* frame, uint64_t size);

};
