#include "platform/hardware_requirements.h"
#include "SDL.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

static_assert(sizeof(void *) == 8, "Vespasian requires a 64-bit target.");

bool platform_meets_hardware_requirements(std::string &failure)
{
    uint64_t memory_mb = SDL_GetSystemRAM();
#ifdef _WIN32
    ULONGLONG installed_kb = 0;
    if (GetPhysicallyInstalledSystemMemory(&installed_kb)) memory_mb = installed_kb / 1024;
    const char *library_name = "vulkan-1.dll";
#else
    const char *library_name = "libvulkan.so.1";
#endif
    if (memory_mb < 4096) { failure = "Vespasian requires at least 4 GB of installed RAM."; return false; }
    void *library = SDL_LoadObject(library_name);
    if (!library) { failure = "Vespasian requires a Vulkan-capable graphics driver. Install the driver supplied by your GPU manufacturer."; return false; }
    const auto get_proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
    const auto create_instance = get_proc ? reinterpret_cast<PFN_vkCreateInstance>(get_proc(VK_NULL_HANDLE, "vkCreateInstance")) : nullptr;
    VkApplicationInfo application = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "Vespasian";
    application.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo create = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo = &application;
    VkInstance instance = VK_NULL_HANDLE;
    if (!create_instance || create_instance(&create, nullptr, &instance) != VK_SUCCESS) {
        failure = "Vespasian could not initialize Vulkan. A working 64-bit Vulkan graphics driver is required.";
        SDL_UnloadObject(library);
        return false;
    }
    const auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get_proc(instance, "vkEnumeratePhysicalDevices"));
    const auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(get_proc(instance, "vkGetPhysicalDeviceProperties"));
    const auto memory = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get_proc(instance, "vkGetPhysicalDeviceMemoryProperties"));
    const auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(get_proc(instance, "vkDestroyInstance"));
    bool supported = false;
    uint32_t count = 0;
    if (enumerate && properties && memory && enumerate(instance, &count, nullptr) == VK_SUCCESS && count <= 256) {
        std::vector<VkPhysicalDevice> devices(count);
        const VkResult result = enumerate(instance, &count, devices.data());
        if (result == VK_SUCCESS || result == VK_INCOMPLETE) {
            for (uint32_t i = 0; i < count && i < devices.size(); ++i) {
                VkPhysicalDeviceProperties device_properties;
                properties(devices[i], &device_properties);
                if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) continue;
                VkPhysicalDeviceMemoryProperties memory_properties;
                memory(devices[i], &memory_properties);
                uint64_t device_memory = 0;
                for (uint32_t heap = 0; heap < memory_properties.memoryHeapCount; ++heap) {
                    if (memory_properties.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) device_memory += memory_properties.memoryHeaps[heap].size;
                }
                if (device_memory >= 1024ull * 1024 * 1024) {
                    SDL_Log("Hardware requirements: %llu MB RAM; Vulkan GPU %s, %llu MB device-local memory", memory_mb, device_properties.deviceName, device_memory / (1024 * 1024));
                    supported = true;
                    break;
                }
            }
        }
    }
    if (destroy) destroy(instance, nullptr);
    SDL_UnloadObject(library);
    if (!supported) failure = "Vespasian requires a Vulkan-capable GPU with at least 1 GB of device-local video memory.";
    return supported;
}
