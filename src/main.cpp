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
	

int main() {
	int width, height;

	unsigned char* image = stbi_load("./resources/picture.png", &width, &height, nullptr, 4);
	assert(image);

	
	if (!glfwInit()) {
		std::cout<<"GLFW cannot be initialised." <<std::endl;
		return -1;
	}
	scope_exit glfw_deleter( []{ glfwTerminate(); } );
	glfwSetErrorCallback(glfw_error_callback);

	std::vector<c_str> required_extensions;
	if (!get_glfw_vulkan_required_extensions(required_extensions)) return -1;


	Vulkan_display vulkan;
	vulkan.create_instance(required_extensions);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window_ptr = glfwCreateWindow(800, 800, "GLFW/Vulkan Window", NULL, NULL);
	if (!window_ptr) {
		return -1;
	}
	auto window_deleter = [](GLFWwindow* window) { glfwDestroyWindow(window); };
	std::unique_ptr<GLFWwindow, decltype(window_deleter)> window(window_ptr, window_deleter);

	glfwSetKeyCallback(window_ptr, key_callback);


	VkSurfaceKHR raw_surface;
	if (glfwCreateWindowSurface(vulkan.get_instance(), window_ptr, NULL, &raw_surface) != VK_SUCCESS) {
		std::cout << "Window cannot be created." << std::endl;
	}
	
	vulkan.init_vulkan(raw_surface, 800, 800);

	while (!glfwWindowShouldClose(window_ptr)) {
		glfwPollEvents();
		vulkan.render(image, static_cast<uint64_t>( width ) * height * 4);
	}
	
	return 0;
}