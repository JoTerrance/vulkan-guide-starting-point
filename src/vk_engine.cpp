
#include "vk_engine.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "vk_types.h"
#include "vk_initializers.h"
//bootstrap library
#include <VkBootstrap.h>
#include <iostream>

//we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
using namespace std;
#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			std::cout <<"Detected Vulkan error: " << err << std::endl; \
			abort();                                                \
		}                                                           \
	} while (0)

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
	
	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);

    init_vulkan();
    init_swapchain();
	
	//everything went fine
	_isInitialized = true;
}
void VulkanEngine::init_vulkan()
{
  vkb::InstanceBuilder builder;

	//make the Vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Example Vulkan Application")
		.request_validation_layers(true)
		.require_api_version(1, 1, 0)
		.use_default_debug_messenger()
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	//store the instance
	_instance = vkb_inst.instance;
	//store the debug messenger
	_debug_messenger = vkb_inst.debug_messenger;

  // get the surface of the window we opened with SDL
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	//use vkbootstrap to select a GPU.
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 2)
		.set_surface(_surface)
		.select()
		.value();

	//create the final Vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

  // Get the queue used for graphics operations
  _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}
void VulkanEngine::init_swapchain(){
vkb::SwapchainBuilder swapchainBuilder{_chosenGPU,_device,_surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.build()
		.value();

	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

	_swapchainImageFormat = vkbSwapchain.image_format;
};
void VulkanEngine::init_commands()
{
	//create a command pool for commands submitted to the graphics queue.
  //Most Vulkan Info structures, used for all of the VkCreateX functions, and a lot of the other Vulkan structures, need sType and pNext set. This is used for extensions, as some extensions will still call the VkCreateX function, but with structs of a different type than the normal one. The sType helps the implementation know what struct is being used in the function.
  //With Vulkan structures, it is very important that we do this
	VkCommandPoolCreateInfo commandPoolInfo = {};
  //By doing that ` = {}` thing, we are letting the compiler initialize the entire struct to zero. This is critical, as in general Vulkan structs will have their defaults set in a way that 0 is relatively safe. By doing that, we make sure we don’t leave uninitialized data in the struct.
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = nullptr;

	//the command pool will be one that can submit graphics commands
	commandPoolInfo.queueFamilyIndex = _graphicsQueueFamily;
	//we also want the pool to allow for resetting of individual command buffers
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool));

  //allocate the default command buffer that we will use for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo = {};
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.pNext = nullptr;

	//commands will be made from our _commandPool
	cmdAllocInfo.commandPool = _commandPool;
	//we will allocate 1 command buffer
	cmdAllocInfo.commandBufferCount = 1;
	// command level is Primary
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  // The .level is set to Primary . Command buffers can be Primary of Secondary level. Primary level are the ones that are sent into a VkQueue, and do all of the work. This is what we will use in the guide. Secondary level are ones that can act as “subcommands” to a primary buffer. They are most commonly used in advanced multithreading scenarios.


	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer));
}
void VulkanEngine::cleanup(){
  if (_isInitialized) {
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
		//destroy swapchain resources
		for (int i = 0; i < _swapchainImageViews.size(); i++) {
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
		}

		vkDestroyDevice(_device, nullptr);
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);
		SDL_DestroyWindow(_window);
	}	
}

void VulkanEngine::draw()
{
	//nothing yet
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;
		}

		draw();
	}
}

