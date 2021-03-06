#pragma once
#include "vulkan_context.h"
#include <functional>

namespace vulkan_display {

struct image_description {
        vk::Extent2D size;
        vk::Format format{};

        image_description() = default;
        image_description(vk::Extent2D size, vk::Format format) :
                size{ size }, format{ format } { }
        image_description(uint32_t width, uint32_t height, vk::Format format = vk::Format::eR8G8B8A8Srgb) :
                image_description{ vk::Extent2D{width, height}, format } { }

        bool operator==(const image_description& other) const {
                return size == other.size && format == other.format;
        }

        bool operator!=(const image_description& other) const {
                return !(*this == other);
        }
};

class image;

} // vulkan_display-----------------------------------------

namespace vulkan_display_detail {

class transfer_image {
        vk::DeviceMemory memory;
        vk::Image image;
        vk::ImageLayout layout{};
        vk::AccessFlags access;

public:
        static constexpr uint32_t NO_ID = UINT32_MAX;
        uint32_t id = NO_ID;
        vk::ImageView view;
        std::byte* ptr = nullptr;
        vulkan_display::image_description description;

        vk::DeviceSize row_pitch = 0;

        bool fence_set = false;       // true if waiting for is_available_fence is neccessary
        vk::Fence is_available_fence; // is_available_fence isn't signalled when gpu uses the image

        bool update_desciptor_set = true;
        vk::Sampler sampler;

        using preprocess_function = std::function<void(vulkan_display::image& image)>;
        preprocess_function preprocess_fun{ nullptr };

        RETURN_TYPE init(vk::Device device, uint32_t id);

        RETURN_TYPE create(vk::Device device, vk::PhysicalDevice gpu, vulkan_display::image_description description);

        vk::ImageMemoryBarrier create_memory_barrier(
                vk::ImageLayout new_layout,
                vk::AccessFlags new_access_mask,
                uint32_t src_queue_family_index = VK_QUEUE_FAMILY_IGNORED,
                uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED);

        /// update_description_sets should be called everytime before recording the command buffer
        RETURN_TYPE update_description_set(vk::Device device, vk::DescriptorSet descriptor_set, vk::Sampler sampler);

        RETURN_TYPE destroy(vk::Device device, bool destroy_fence = true);

        transfer_image() = default;
        transfer_image(vk::Device device, uint32_t id) {
                init(device, id);
        }
};

} // vulkan_display_detail


namespace vulkan_display {

class image {
        
        vulkan_display_detail::transfer_image* transfer_image = nullptr;
public:
        image() = default;
        explicit image(vulkan_display_detail::transfer_image& image) :
                transfer_image{ &image }
        { 
                assert(image.id != vulkan_display_detail::transfer_image::NO_ID);
                transfer_image->preprocess_fun = nullptr;
        }

        uint32_t get_id() {
                assert(transfer_image); 
                return transfer_image->id;
        }

        std::byte* get_memory_ptr() {
                assert(transfer_image);
                return transfer_image->ptr;
        }

        image_description get_description() {
                assert(transfer_image);
                return transfer_image->description;
        }

        vk::DeviceSize get_row_pitch() {
                assert(transfer_image);
                return transfer_image->row_pitch;
        }

        vk::Extent2D get_size() {
                return transfer_image->description.size;
        }

        vulkan_display_detail::transfer_image* get_transfer_image() {
                return transfer_image;
        }

        void set_process_function(std::function<void(image& image)> function) {
                transfer_image->preprocess_fun = std::move(function);
        }

        void preprocess(){
                if (transfer_image->preprocess_fun) {
                        transfer_image->preprocess_fun(*this);
                }
        }
};

} //namespace vulkan_display

