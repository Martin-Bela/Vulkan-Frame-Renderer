//#define NO_EXCEPTIONS

#include "vulkan_display.h" // Vulkan.h must be before GLFW

#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <iostream>


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
		
		image = stbi_load("./resources/picture.png", &image_width, &image_height, nullptr, 4);
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

		glfwSetKeyCallback(window, key_callback);


		VkSurfaceKHR raw_surface;
		if (glfwCreateWindowSurface(vulkan.get_instance(), window, NULL, &raw_surface) != VK_SUCCESS) {
			throw std::runtime_error{ "Vulkan surface cannot be created." };
		}


		vulkan.init(raw_surface, this);
	}

	int main() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			vulkan.render(image, static_cast<uint64_t>(image_width) * image_height * 4);
		}

		return 0;
	}

	~GLFW_vulkan_display() {
		if (window) {
			glfwDestroyWindow(window);
		}
		if (glfw_initialised) {
			glfwTerminate();
		}
	}



	// Inherited via window_inteface
	virtual std::pair<uint32_t, uint32_t> get_window_size() override
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		return {static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	}

};


int main() {
	GLFW_vulkan_display display{};
	return display.main();
}