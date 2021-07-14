//#define NO_EXCEPTIONS

#include "vulkan_display.h" // Vulkan.h must be before GLFW

#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <iostream>
#include <string>

using namespace std::literals;
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
		}
		result = std::vector<c_str>(extensions, extensions + count);
		return true;
	}
}


struct GLFW_vulkan_display : public Window_inteface {
	bool glfw_initialised = false;
	GLFWwindow* window = nullptr;
	Vulkan_display vulkan;
	
	unsigned char* image;
	int image_width, image_height;

	GLFW_vulkan_display() {
		
		image = stbi_load("./resources/picture2.jpg", &image_width, &image_height, nullptr, 4);
		assert(image);


		if (!glfwInit()) {
			throw std::runtime_error{ "GLFW cannot be initialised." };
		}
		glfw_initialised = true;
		glfwSetErrorCallback(glfw_error_callback);

		std::vector<c_str> required_extensions;
		if (!get_glfw_vulkan_required_extensions(required_extensions)) {
			throw std::runtime_error{ "Vulkan is not supported by glfw." };
		}

		vulkan.create_instance(required_extensions);

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


		vulkan.init(raw_surface, this);
	}

	int run() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			vulkan.render(image, image_width, image_height);
		}

		return 0;
	}

	~GLFW_vulkan_display() {
		if (window) glfwDestroyWindow(window);
		if (glfw_initialised) glfwTerminate();
	}



	std::pair<uint32_t, uint32_t> get_window_size() override
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		return {static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	}

};

namespace {
	void window_resize_callback(GLFWwindow* window, int width, int height) {
		auto display = reinterpret_cast<GLFW_vulkan_display*>(glfwGetWindowUserPointer(window));
		display->vulkan.resize_window();
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


class SDL_vulkan_display : Window_inteface{
	bool sdl_initialised = false;
	Vulkan_display vulkan;
	SDL_Window* window;
	bool window_should_close = false;
	
	unsigned char* image;
	int image_width, image_height;

public:
	SDL_vulkan_display() {
		image = stbi_load("./resources/picture2.jpg", &image_width, &image_height, nullptr, 4);
		assert(image);

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

		vulkan.create_instance(required_extensions);
		auto& instance = vulkan.get_instance();

		VkSurfaceKHR surface;
		if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
			throw std::runtime_error("SDL cannot create surface.");
		}

		vulkan.init(surface, this);
		
	}


	~SDL_vulkan_display() {
		if (window) SDL_DestroyWindow(window);
		if (sdl_initialised) SDL_Quit();
	}

	std::pair<uint32_t, uint32_t> get_window_size() override
	{
		int width, height;
		SDL_GetWindowSize(window, &width, &height);
		return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	}

	int run() {
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
							vulkan.resize_window();
						}
						break;
					case SDL_EventType::SDL_KEYDOWN:
						if (event.key.keysym.sym == SDL_KeyCode::SDLK_q) {
							window_should_close = true;
							break;
						}
				}
			}
			vulkan.render(image, image_width, image_height);
		}

		return 0;
	}
};


int main() {
	SDL_vulkan_display display{};
	display.run();
	return 0;
}






