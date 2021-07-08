//#define NO_EXCEPTIONS

#include "vulkan_display.h" // Vulkan.h must be before GLFW

#include <GLFW/glfw3.h>
#include <SOIL2/SOIL2.h>

#include <iostream>

namespace {
	using c_str = const char*;

	struct GLFW {
		bool good;
		GLFW() { good = glfwInit(); }
		~GLFW() { glfwTerminate(); }
	};


	void glfw_error_callback(int error, const char* description) {
		printf("Error: %s\n", description);
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
	

int main() {
	GLFW glfw;
	if (!glfw.good) {
		printf("GLFW cannot be initialised.\n");
		return -1;
	}
	glfwSetErrorCallback(glfw_error_callback);

	std::vector<c_str> required_extensions;
	if (!get_glfw_vulkan_required_extensions(required_extensions)) return false;


	Vulkan_display vulkan;
	vulkan.create_instance(required_extensions);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window_ptr = glfwCreateWindow(800, 800, "GLFW/Vulkan Window", NULL, NULL);
	auto deleter = [](GLFWwindow* window) { if (window) { glfwDestroyWindow(window); } };
	std::unique_ptr<GLFWwindow, decltype(deleter)> window(window_ptr, deleter);
	if (!window) {
		return -1;
	}
	glfwSetKeyCallback(window_ptr, key_callback);


	VkSurfaceKHR raw_surface;
	if (glfwCreateWindowSurface(vulkan.get_instance(), window_ptr, NULL, &raw_surface) != VK_SUCCESS) {
		printf("Window cannot be created.\n");
	}
	
	vulkan.init_vulkan(raw_surface, 800, 800);

	while (!glfwWindowShouldClose(window_ptr)) {
		glfwPollEvents();
		vulkan.render();
	}
	
	return 0;
}