#pragma once


#ifdef NO_EXCEPTIONS
#define VULKAN_HPP_NO_EXCEPTIONS
#endif //NO_EXCEPTIONS

#include <vulkan/vulkan.hpp>

#include <string>


namespace vulkan_display_detail {

inline vk::Result to_vk_result(void) {
        return vk::Result::eSuccess;
}

inline vk::Result to_vk_result(bool b) {
        return b ? vk::Result::eSuccess : vk::Result::eErrorUnknown;
}

inline vk::Result to_vk_result(vk::Result res) {
        return res;
}

} // namespace vulkan_display_detail


#ifdef NO_EXCEPTIONS //-------------------------------------------------------
//EXCEPTIONS ARE DISABLED
extern std::string vulkan_display_error_message;

#define RETURN_VAL vk::Result

#define PASS_RESULT(expr) {                                              \
        if (vk::Result res = expr; res != vk::Result::eSuccess) {        \
                assert(false);                                           \
                return res;                                              \
        }                                                                \
}

#define CHECK(expr, msg) {                                               \
        vk::Result res = to_vk_result(expr);                             \
        if ( res != vk::Result::eSuccess) {                              \
                assert(false);                                           \
                vulkan_display_error_message = msg;                      \
                return res;                                              \
        }                                                                \
}

#define CHECKED_ASSIGN(variable, expr) {                                 \
        auto[checked_assign_return_val, checked_assign_value] = expr;    \
        if (checked_assign_return_val != vk::Result::eSuccess) {         \
                assert(false);                                           \
                return checked_assign_return_val;                        \
        } else {variable = std::move(checked_assign_value);}             \
}

#else //NO_EXCEPTIONS -------------------------------------------------------
//EXCEPTIONS ARE ENABLED
#include<exception>

struct  Vulkan_display_exception : public std::runtime_error {
        Vulkan_display_exception() = default;
        Vulkan_display_exception(const std::string& msg) :
                std::runtime_error{ msg } { }
};

#define RETURN_VAL void

#define PASS_RESULT(expr) { expr; }

#define CHECK(expr, msg) { if (to_vk_result(expr) != vk::Result::eSuccess) throw Vulkan_display_exception{msg}; }

#define CHECKED_ASSIGN(variable, expr) { variable = expr; }

#endif //NO_EXCEPTIONS -------------------------------------------------------


struct Window_parameters {
        uint32_t width;
        uint32_t height;
        bool vsync;

        bool operator==(const Window_parameters& other) const {
                return width == other.width &&
                        height == other.height &&
                        vsync == other.vsync;
        }
        bool operator!=(const Window_parameters& other) const {
                return !(*this == other);
        }
};

constexpr uint32_t NO_GPU_SELECTED = UINT32_MAX;

vk::ImageViewCreateInfo default_image_view_create_info(vk::Format format);


namespace vulkan_display_detail {

using c_str = const char*;
using namespace std::literals;

constexpr uint32_t NO_QUEUE_FAMILY_INDEX_FOUND = UINT32_MAX;
constexpr uint32_t SWAPCHAIN_IMAGE_OUT_OF_DATE = UINT32_MAX;

struct Vulkan_context {
        vk::Instance instance;

        bool validation_enabled;
        std::unique_ptr<vk::DispatchLoaderDynamic> dynamic_dispatch_loader;
        vk::DebugUtilsMessengerEXT messenger;

        vk::PhysicalDevice gpu;
        vk::Device device;

        uint32_t queue_family_index = NO_QUEUE_FAMILY_INDEX_FOUND;
        vk::Queue queue;

        vk::SurfaceKHR surface;
        vk::SwapchainKHR swapchain;
        struct {
                vk::SurfaceCapabilitiesKHR capabilities;
                vk::SurfaceFormatKHR format;
                vk::PresentModeKHR mode = vk::PresentModeKHR::eFifo;
        } swapchain_atributes;

        struct Swapchain_image {
                vk::Image image;
                vk::ImageView view;
                vk::Framebuffer framebuffer;
        };
        std::vector<Swapchain_image> swapchain_images;

        vk::Extent2D window_size{ 0, 0 };
        bool vsync;

private:

        RETURN_VAL init_validation_layers_error_messenger();

        RETURN_VAL create_physical_device(uint32_t gpu_index);

        RETURN_VAL create_logical_device();

        RETURN_VAL get_present_mode();

        RETURN_VAL get_surface_format();

        RETURN_VAL create_swap_chain(vk::SwapchainKHR old_swap_chain = VK_NULL_HANDLE);

        RETURN_VAL create_swapchain_views();

        void destroy_swapchain_views() {
                for (auto& image : swapchain_images) {
                        device.destroy(image.view);
                }
        }

        void destroy_framebuffers() {
                for (auto& image : swapchain_images) {
                        device.destroy(image.framebuffer);
                }
        }

public:
        Vulkan_context() = default;
        Vulkan_context(const Vulkan_context& other) = delete;
        Vulkan_context& operator=(const Vulkan_context& other) = delete;
        Vulkan_context(Vulkan_context&& other) = delete;
        Vulkan_context& operator=(Vulkan_context&& other) = delete;

        ~Vulkan_context();

        RETURN_VAL create_instance(std::vector<const char*>& required_extensions, bool enable_validation);

        RETURN_VAL get_available_gpus(std::vector<std::pair<std::string, bool>>& gpus);

        RETURN_VAL init(VkSurfaceKHR surface, Window_parameters parameters, uint32_t gpu_index);

        RETURN_VAL create_framebuffers(vk::RenderPass render_pass);

        RETURN_VAL acquire_next_swapchain_image(uint32_t& image_index, vk::Semaphore acquire_semaphore);

        vk::Framebuffer get_framebuffer(uint32_t framebuffer_id) {
                return swapchain_images[framebuffer_id].framebuffer;
        }

        Window_parameters get_window_parameters() {
                return { window_size.width, window_size.height, vsync };
        }

        RETURN_VAL recreate_swapchain(Window_parameters parameters, vk::RenderPass render_pass);
};

}//namespace 
