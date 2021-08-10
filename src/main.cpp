//#define NO_EXCEPTIONS

#include "vulkan_display.h" // Vulkan.h must be before GLFW
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <string>
#include <chrono>

using namespace std::literals;
namespace chrono = std::chrono;
namespace vkd = vulkan_display;

namespace {
        using c_str = const char*;

        template<typename fun>
        class scope_exit {
                fun function;
        public:
                scope_exit(fun function) :
                        function{ std::move(function) } { }
                ~scope_exit() {
                        function();
                }
        };

        void glfw_error_callback(int error, const char* description) {
                std::cout<< "Error: " << description << '\n';
        }

        void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
                if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS) {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
        }

        void window_resize_callback(GLFWwindow* window, int width, int height);

        bool get_glfw_vulkan_required_extensions(std::vector<c_str>& result) {
                uint32_t count; // glfw frees returned arrays
                const c_str* extensions = glfwGetRequiredInstanceExtensions(&count);
                if (!extensions) {
                        std::cout << "GLFW doesn't support Vulkan." << std::endl;
                        return false;
                }
                result = std::vector<c_str>(extensions, extensions + count);
                return true;
        }
}

struct GLFW_vulkan_display : vkd::window_changed_callback {
        bool glfw_initialised = false;
        GLFWwindow* window = nullptr;
        vkd::vulkan_display vulkan;
        
        std::byte* image;
        uint32_t image_width, image_height;

        GLFW_vulkan_display() {
                int width, height;
                auto image = stbi_load("./resources/picture.png", &width, &height, nullptr, 4);
                image_width = static_cast<uint32_t>(width);
                image_height = static_cast<uint32_t>(height);
                assert(image);
                this->image = reinterpret_cast<std::byte*>(image);

                if (!glfwInit()) {
                        throw std::runtime_error{ "GLFW cannot be initialised." };
                }
                glfw_initialised = true;
                glfwSetErrorCallback(glfw_error_callback);

                std::vector<c_str> required_extensions;
                if (!get_glfw_vulkan_required_extensions(required_extensions)) {
                        throw std::runtime_error{ "Vulkan is not supported by glfw." };
                }

                vulkan.create_instance(required_extensions, true);

                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                window = glfwCreateWindow(800, 800, "GLFW/Vulkan Window", NULL, NULL);
                if (!window) {
                        throw std::runtime_error{ "Window cannot be created." };
                }

                glfwSetWindowUserPointer(window, this);
                glfwSetKeyCallback(window, key_callback);
                glfwSetWindowSizeCallback(window, window_resize_callback);

                VkSurfaceKHR raw_surface;
                if (glfwCreateWindowSurface(vulkan.get_instance(), window, NULL, &raw_surface) != VK_SUCCESS) {
                        throw std::runtime_error{ "Vulkan surface cannot be created." };
                }

                vulkan.init(raw_surface, 3, this);
        }

        int run() {
                while (!glfwWindowShouldClose(window)) {
                        glfwPollEvents();
                        vulkan.copy_and_queue_image(image, { image_width, image_height, vk::Format::eR8G8B8A8Srgb });
                        vulkan.display_queued_image();
                }

                return 0;
        }

        ~GLFW_vulkan_display() {
                if (window) glfwDestroyWindow(window);
                if (glfw_initialised) glfwTerminate();
        }

        vkd::window_parameters get_window_parameters() override {
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);
                return {static_cast<uint32_t>(width), static_cast<uint32_t>(height), true };
        }

};

namespace {
        void window_resize_callback(GLFWwindow* window, int width, int height) {
                auto display = reinterpret_cast<GLFW_vulkan_display*>(glfwGetWindowUserPointer(window));
                display->vulkan.window_parameters_changed();
        }
} // namespace

/*
int main() {
        GLFW_vulkan_display display{};
        return display.run();
}*/

//------------------------------------SDL2_WINDOW-------------------------------------------------
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

class SDL_vulkan_display : vkd::window_changed_callback{

        bool sdl_initialised = false;
        vkd::vulkan_display vulkan;
        SDL_Window* window;
        bool window_should_close = false;
        
        std::byte* image;
        uint32_t image_width, image_height;
        

        struct color {
                unsigned char r, g, b, a;
        };
        std::vector<color> image2;
        uint32_t image2_width = 2021, image2_height = 999;

        chrono::steady_clock::time_point time{ chrono::steady_clock::now() };
public:
        SDL_vulkan_display() {
                image2.resize(size_t{ image2_height } *image2_width, { 0, 0, 255 });
                for (uint32_t x = 0; x < image2_width; x++) {
                        if (x % 128 < 64) {
                                for (uint32_t y = 0; y < image2_height; y++) {
                                        image2[x + size_t{ y } *image2_width] = { 255, 0, 0 };
                                }
                        }
                }
                int width, height;
                auto image = stbi_load("./resources/picture2.jpg", &width, &height, nullptr, 4);
                image_width = static_cast<uint32_t>(width);
                image_height = static_cast<uint32_t>(height);
                
                assert(image);
                this->image = reinterpret_cast<std::byte*>(image);

                if (SDL_Init(SDL_INIT_EVENTS) != 0) {
                        throw std::runtime_error("SDL cannot be initialised.");
                }
                sdl_initialised = true;

                window = SDL_CreateWindow("SDL Vulkan window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                        800, 800, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
                if (!window) {
                        std::cout << SDL_GetError() << std::endl;
                        throw std::runtime_error("SDL window cannot be created.");
                }

                uint32_t extension_count = 0;
                SDL_Vulkan_GetInstanceExtensions(window, &extension_count, nullptr);
                std::vector<const char*> required_extensions(extension_count);
                SDL_Vulkan_GetInstanceExtensions(window, &extension_count, required_extensions.data());
                assert(extension_count > 0);

                vulkan.create_instance(required_extensions, true);
                auto& instance = vulkan.get_instance();

                VkSurfaceKHR surface;
                if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
                        throw std::runtime_error("SDL cannot create surface.");
                }

                vulkan.init(surface, 3, this);
                
        }

        ~SDL_vulkan_display() {
                if (window) SDL_DestroyWindow(window);
                if (sdl_initialised) SDL_Quit();
        }

        vkd::window_parameters get_window_parameters() override {
                int width, height;
                SDL_Vulkan_GetDrawableSize(window, &width, &height);
                if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
                        width = 0;
                        height = 0;
                }
                return { static_cast<uint32_t>(width), static_cast<uint32_t>(height), true };
        }

        int run() {
                int frame_count = 0;
                while (!window_should_close) {
                        SDL_Event event;
                        while (SDL_PollEvent(&event) != 0) {
                                switch (event.type) {
                                        case SDL_EventType::SDL_QUIT:
                                                window_should_close = true;
                                                break;
                                        case SDL_EventType::SDL_WINDOWEVENT:
                                                if (event.window.event == SDL_WINDOWEVENT_EXPOSED
                                                        || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) 
                                                {
                                                        vulkan.window_parameters_changed();
                                                }
                                                break;
                                        case SDL_EventType::SDL_KEYDOWN:
                                                if (event.key.keysym.sym == SDL_KeyCode::SDLK_q) {
                                                        window_should_close = true;
                                                        break;
                                                }
                                }
                        }
                        auto time = chrono::steady_clock::now();
                        double seconds = chrono::duration_cast<chrono::duration<double>>(time - this->time).count();

                        frame_count++;
                        if (seconds < 3.0) {
                                vulkan.copy_and_queue_image(reinterpret_cast<std::byte*>(image2.data()), { image2_width, image2_height,
                                        sizeof(color) == 4 ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8Srgb });
                        }
                        else {
                                vulkan.copy_and_queue_image(image, { image_width, image_height });
                        }

                        vulkan.display_queued_image();

                        if (seconds > 6.0) {
                                double fps = frame_count / seconds;
                                std::cout << "FPS:" << fps << std::endl;
                                this->time = time;
                                frame_count = 0;
                        }


                }

                return 0;
        }
};

int main() {
        SDL_vulkan_display display{};
        display.run();
        return 0;
}



