#include "vulkan_utils.h"


const char* vulkan_result_string(VkResult result, b8 get_extended){
    // From: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkResult.html
    // Success codes
    switch (result){
        default:
        case VK_SUCCESS:
            return !get_extended ? "VK_SUCCESS" : "VK_SUCCESS command succesfully completed";
        case VK_NOT_READY:
            return !get_extended ? "VK_NOT_READY" : "VK_NOT_READY A fence or query has not yet completed";
        case VK_TIMEOUT:
            return !get_extended ? "VK_TIMEOUT" : "VK_TIMEOUT A wait operation has not completed in specific time";
        case VK_EVENT_SET:
            return !get_extended ? "VK_EVENT_SET" : "VK_EVENT_SET An event is signaled";
        case VK_EVENT_RESET:
            return !get_extended ? "VK_EVENT_RESET" : "VK_EVENT_RESET An event is unsignaled";
        case VK_SUBOPTIMAL_KHR:
            return !get_extended ? "VK_SUBOPTIMAL_KHR" : "VK_SUBOPTIMAL_KHR A swapchain no longer matches the surface properties exactly, but can still be used to render";
        case VK_THREAD_IDLE_KHR:
            return !get_extended ? "VK_THREAD_IDLE_KHR" : "VK_THREAD_IDLE_KHR A wait operation has not completed in specific time";
        case VK_TIMEOUT:
            return !get_extended ? "VK_TIMEOUT" : "VK_TIMEOUT A wait operation has not completed in specific time";
        case VK_TIMEOUT:
            return !get_extended ? "VK_TIMEOUT" : "VK_TIMEOUT A wait operation has not completed in specific time";
    }
}

b8 vulkan_result_is_success(VkResult result); 