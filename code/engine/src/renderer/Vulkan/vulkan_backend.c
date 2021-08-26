#include "vulkan_backend.h"
#include "vulkan_types.inl"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_renderpass.h"
#include "vulkan_command_buffer.h"
#include "vulkan_framebuffer.h"
#include "vulkan_fence.h"

#include "core/application.h"
#include "core/logger.h"
#include "core/kstring.h"
#include "core/kmemory.h"

#include "containers/darray.h"
#include "vulkan_platform.h"



// Static Vulakn Context
static vulkan_context context;
static u32 cached_framebuffer_width = 0;
static u32 cached_framebuffer_height = 0;


VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);

i32 find_memory_index(u32 type_filter, u32 property_flags);
void create_command_buffers(renderer_backend * backend);
void regenerate_framebuffers(renderer_backend* backend, vulkan_swapchain* swapchain, vulkan_renderpass* renderpass);





b8 vulkan_renderer_backend_initialize(renderer_backend* backend, const char* application_name, struct platform_state* plat_state){
    
    // Function pointers
    context.find_memory_index = find_memory_index;
    
    
    // TODO: custom allocator.
    context.allocator = 0;


    // Get initial width/height
    application_get_framebuffer_size(&cached_framebuffer_width, &cached_framebuffer_height);
    context.framebuffer_width = (cached_framebuffer_width != 0) ? cached_framebuffer_width : 800;
    context.framebuffer_height = (cached_framebuffer_height != 0) ? cached_framebuffer_height : 600;
    cached_framebuffer_width = 0;
    cached_framebuffer_height = 0;


    // Setup Vulkan Instance
    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.apiVersion          = VK_API_VERSION_1_2;
    app_info.pApplicationName    = application_name;
    app_info.applicationVersion  = VK_MAKE_VERSION(1,0,0);
    app_info.pEngineName         = "Kohi Enginge";
    app_info.engineVersion       = VK_MAKE_VERSION(1,0,0);

    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo        = &app_info;


    // Obtain a list of required extensions
    const char** required_extensions = darray_create(const char*);
    darray_push(required_extensions, &VK_KHR_SURFACE_EXTENSION_NAME);   // Generic surface extension
    platrofm_get_required_extension_names(&required_extensions);        // Platform specific extenion(s)
    #if defined(_DEBUG)
        darray_push(required_extensions, &VK_EXT_DEBUG_UTILS_EXTENSION_NAME) // Debug utilities

        KDEBUG("Required extensions:")
        u32 length = darray_length(required_extensions);
        for (u32 i = 0 ; i < length; ++i){
            KDEBUG(required_extensions[i]);
        }
    #endif

    create_info.enabledExtensionCount   = darray_length(required_extensions);   //<-----Extensions
    create_info.ppEnabledExtensionNames = required_extensions;                  //<--------┘

    

    // Validation Layers
    const char** required_validation_layer_names = 0;
    u32 required_validation_layer_count = 0;

    // If validation should be done, get a list of the required validation layer names
    // and make sure they exist. Validation layers should only be anabled on non-release builds.
    #if defined(_DEBUG)
        KINFO("Validation layers enabled. Enumerating...");

        // the list of validation layers required
        required_validation_layer_names = darray_create(const char*);
        darray_push(required_validation_layer_names, &"VK_LAYER_KHRONOS_validation");
        required_validation_layer_count = darray_length(required_validation_layer_names);

        // Obtain a list of available validation layers
        u32 available_layer_count = 0;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
        VkLayerProperties* available_layers = darray_reserve(VkLayerProperties, available_layer_count);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));

        // verify all required layers are available
        for(u32 i=0; i < required_validation_layer_count; ++i){
            KINFO("Searching for layer: %s...", required_validation_layer_names[i]);
            b8 found = FALSE;
            for(u32 j = 0; j < available_layer_count; ++j){
                if(strings_equal(required_validation_layer_names[i], available_layers[j].layerName)){
                    found = TRUE;
                    KINFO("Found.");
                    break;
                }
            }

            if(!found){
                KFATAL("Required validation layer is missing: %s", required_validation_layer_names[i]);
                return FALSE;
            }
        }

        KINFO("All required validation layers are present.")
    #endif
    create_info.enabledLayerCount    = required_validation_layer_count;      //<-----Layers
    create_info.ppEnabledLayerNames  = required_validation_layer_names;      //<--------┘


    VK_CHECK(vkCreateInstance(&create_info, context.allocator, &context.instance));
    KINFO("Vulkan Instance created.");


    // Debugger
    #if defined(_DEBUG)
        KDEBUG("Creating Vulkan Debugger...");

        u32 log_severity =  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
                            //VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
                                                                             // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug_create_info.messageSeverity = log_severity;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debug_create_info.pfnUserCallback = vk_debug_callback;

        PFN_vkCreateDebugUtilsMessengerEXT func = 
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT");
        KASSERT_MSG(func, "Failed to create debug messenger!");
        VK_CHECK(func(context.instance, &debug_create_info, context.allocator, &context.debug_messenger));
        KDEBUG("Vulkan debugger created");
    #endif

    
    // Surface creation ---------------------------------------------------------------------------
    KDEBUG("Creatiung Vulkan surface...");
    if(!platform_create_vulkan_surface(plat_state, &context)){
        KERROR("Failed to create platform surface!");
        return FALSE;
    }
    KDEBUG("Vulkan surface created.");


    // Device creation ---------------------------------------------------------------------------
    if(!vulkan_device_create(&context)){
        KERROR("Failed to create device!");
        return FALSE;
    }

    
    // Swapchain ---------------------------------------------------------------------------
    vulkan_swapchain_create(
        &context,
        context.framebuffer_width,
        context.framebuffer_height,
        &context.swapchain);


    // Renderpass ---------------------------------------------------------------------------
    vulkan_renderpass_create(
        &context,
        &context.main_renderpass,
        0, 0, context.framebuffer_width, context.framebuffer_height,
        0.0f, 0.0f, 0.2f, 1.0f,
        1.0f,
        0);


    // Swapchain FrameBuffers ----------------------------------------------------------------
    context.swapchain.framebuffers = darray_reserve(vulkan_framebuffer, context.swapchain.image_count);
    regenerate_framebuffers(backend, &context.swapchain, &context.main_renderpass);


    // Command Buffers ------------------------------------------------------------------------
    create_command_buffers(backend);


    // Create Sync Objects. -------------------------------------------------------------------
    context.image_available_semaphores = darray_reserve(VkSemaphore, context.swapchain.max_frames_in_flight);
    context.queue_complete_semaphores  = darray_reserve(VkSemaphore, context.swapchain.max_frames_in_flight);
    context.in_flight_fences = darray_reserve(vulkan_fence, context.swapchain.max_frames_in_flight);

    for (u8 i=0; i<context.swapchain.max_frames_in_flight; ++i){
        VkSemaphoreCreateInfo semaphore_create_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(context.device.logical_device, &semaphore_create_info, context.allocator, &context.image_available_semaphores[i]);
        vkCreateSemaphore(context.device.logical_device, &semaphore_create_info, context.allocator, &context.queue_complete_semaphores[i]);

        // Create the fence in a signaled state, indicating that the first fram has already been "rendered".
        // This will prevent the application from waiting indefinitely for the first frame to render since it
        // cannot be rendered until a frame is "rendered" before it
        vulkan_fence_create(&context, TRUE, &context.in_flight_fences[i]);
    }

    // In flight fences should not yet exist at this point, so clear the list. These are stored in pointers
    // because the initial state should be 0, and will be 0 when not in use. Actual fences are not owned
    // by the list.
    context.images_in_flight = darray_reserve(vulkan_fence, context.swapchain.image_count);
    for(u32 i =0; i<context.swapchain.image_count; ++i){
        context.images_in_flight[i] = 0;
    }



    KINFO("VULKAN renderer initialized succesfully.");
    return TRUE;
}



void vulkan_renderer_backend_shutdown(renderer_backend* backend){
    vkDeviceWaitIdle(context.device.logical_device);
    // Destroy in the opposite order of creation


    // Sync objects
    for (u8 i = 0; i<context.swapchain.max_frames_in_flight; ++i){
        if(context.image_available_semaphores[i]){
            vkDestroySemaphore(
                context.device.logical_device,
                context.image_available_semaphores[i],
                context.allocator);
            context.image_available_semaphores[i] = 0;
        }
         if (context.queue_complete_semaphores[i]){
            vkDestroySemaphore(
                context.device.logical_device,
                context.queue_complete_semaphores[i],
                context.allocator);
            context.queue_complete_semaphores[i] = 0;
        }
        vulkan_fence_destroy(&context, &context.in_flight_fences[i]);
    }
    darray_destroy(context.image_available_semaphores);
    context.image_available_semaphores = 0;

    darray_destroy(context.queue_complete_semaphores);
    context.queue_complete_semaphores = 0;

    darray_destroy(context.in_flight_fences);
    context.in_flight_fences = 0; 

    darray_destroy(context.images_in_flight); 
    context.images_in_flight = 0; 
   


    // Command Buffers ---------------------------------------------------------------------------
    for(u32 i =0; i< context.swapchain.image_count; ++i){
        if(context.graphics_command_buffers[i].handle){
            vulkan_command_buffer_free(
                &context,
                context.device.graphics_command_pool,
                &context.graphics_command_buffers[i]);
            context.graphics_command_buffers[i].handle = 0;
        }
    }
    darray_destroy(context.graphics_command_buffers);
    context.graphics_command_buffers = 0;


    // Destroy Framebuffers ----------------------------------------------------------------
    for(u32 i = 0; i<context.swapchain.image_count; ++i){
        vulkan_framebuffer_destroy(&context, &context.swapchain.framebuffers[i]);
    }


    // Renderpass ---------------------------------------------------------------------------
    vulkan_renderpass_destroy(&context, &context.main_renderpass);


    // Swapchain ---------------------------------------------------------------------------
    vulkan_swapchain_destroy(&context, &context.swapchain);

    KDEBUG("Destroying Vulkan device...");
    vulkan_device_destroy(&context);

    KDEBUG("Destroying Vulkan surface...");
    if(context.surface){
        vkDestroySurfaceKHR(context.instance, context.surface, context.allocator);
        context.surface = 0;
    }

    KDEBUG("Destroying Vulkan debugger...");
    if(context.debug_messenger){
        PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT");
        func(context.instance, context.debug_messenger, context.allocator);
    }

    KDEBUG("Destrpuing Vulkan instance...");
    vkDestroyInstance(context.instance, context.allocator);
}




void vulkan_renderer_backend_on_resized(renderer_backend* backend, u16 width, u16 height){
    // Update the "framebuffer size generation", a counter which indicates when the
    // framebuffer size has been updated.

    cached_framebuffer_width  =  width;
    cached_framebuffer_height = height;
    context.framebuffer_size_generation++;

    KINFO("Vulkan renderer backend -> resized : w/h/gen: %i/%i/%llu", width, height,context.framebuffer_size_generation);
}
 



b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time){
    // Frame Init ----------------------------------------------------------------------------------
    vulkan_device* device = &context.device;

    // Check if recreating swapchain and boot out.
    if(context.recreating_swapchain){
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if(!vulkan_result_is_success(result)){
            KERROR("vulkan_renderer_backend_begin_frame vkDeviceWaitIdle (1) failed : '%s'", vulkan_result_string(result,TRUE));
            return FALSE;
        }
        KINFO("Recreating swapchain, booting.");
        return FALSE;
    }
    
    // Check if the framebuffer has been resize. If so, a new swapchaing must be created.
    if(context.framebuffer_size_generation != context.framebuffer_size_last_generation){
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if(!vulkan_result_is_success(result)){
            KERROR("vulkan_renderer_backedn_begin_frame vkDeviceWaitIdle (2) failed: '%s'", vulkan_result_string(result,TRUE));
            return FALSE;
        }

        // If the swapchain recreation failed (because, for example, the window was minimized),
        // boot out before unsetting the flag.
        if(!recreate_swapchain(backend)){
            return FALSE;
        }

        KINFO("Resized, booting.");
        return FALSE;
    }


    // Wait for the execution of the current frame to complete. The fence being free will allow this one to move on.
    if(!vulkan_fence_wait(
        &context,
        &context.in_flight_fences[context.current_frame],
        UINT64_MAX))
    {
            KWARN("In-flight fence wait failure!");
            return FALSE;
    }


    // Acqyure the nest image from the swapchain. Pass along the semaphore that should signaled when this completes.
    // This same semaphore will later be waited on by the queue submission to ensure this image is available.
    if(!vulkan_swapchain_acquire_next_image_index(
        &context,
        &context.swapchain,
        UINT64_MAX,
        context.image_available_semaphores[context.current_frame],
        0,
        &context.image_index))
    {
        return FALSE;
    }



    // Begin recording commands ----------------------------------------------------------------------
    vulkan_command_buffer* command_buffer =  &context.graphics_command_buffers[context.image_index];
    vulkan_command_buffer_reset(command_buffer);
    vulkan_command_buffer_begin(command_buffer, FALSE, FALSE, FALSE);

    //      Dynamic State 
    VkViewport viewport;
    viewport.x          =  0.0f;
    viewport.y          =  (f32)context.framebuffer_height;
    viewport.width      =  (f32)context.framebuffer_width;
    viewport.height     = -(f32)context.framebuffer_height;
    viewport.minDepth   =  0.0f;
    viewport.maxDepth   =  1.0f;
    //      Scissor
    VkRect2D scissor;
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = context.framebuffer_width;
    scissor.extent.height = context.framebuffer_height;

    vkCmdSetViewport(command_buffer->handle, 0, 1, &viewport);
    vkCmdSetScissor( command_buffer->handle, 0, 1, &scissor);

    context.main_renderpass.w = context.framebuffer_width;
    context.main_renderpass.h = context.framebuffer_height;

    // Begin the render pass.
    vulkan_renderpass_begin(
        command_buffer,
        &context.main_renderpass,
        context.swapchain.framebuffers[context.image_index].handle);
    

    return TRUE;
}
b8 vulkan_renderer_backend_end_frame(renderer_backend* backend, f32 delta_time){

    vulkan_command_buffer* command_buffer = &context.graphics_command_buffers[context.image_index];

    // End renderpass
    vulkan_renderpass_end(command_buffer, &context.main_renderpass);

    vulkan_command_buffer_end(command_buffer);

    // Make sure the previous frame is not using this image(i.e it's fence is being waited on)
    if(context.images_in_flight[context.image_index] != VK_NULL_HANDLE){ //was frame
        vulkan_fence_wait(
            &context,
            context.images_in_flight[context.image_index],
            UINT64_MAX);
    }

    // Mark the image fence as in-use by this frame.
    context.images_in_flight[context.image_index] = &context.in_flight_fences[context.current_frame];

    // Reset the fence for use on the next frame
    vulkan_fence_reset(&context, &context.in_flight_fences[context.current_frame]);
    


    // Submit the queue and wait for the operation to complete.
    //  Begin queue submition----------------------------------------------------------------------
    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    // Command buffer(s) to be executed.
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = command_buffer->handle;
    // The Semaphores to be signaled when the queue is complete
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &context.queue_complete_semaphores[context.current_frame];
    // Wait semaphore ensures that the operation cannot begin until the image is available.
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &context.image_available_semaphores[context.current_frame];
    // Each semaphore waits on the corresponding pipeline stage to complete. 1:1 ratio
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT prevents subsequent color attachment
    // writes from executing until the semaphore signals (i.e one frame is presented at a time)
    VkPipelineStageFlags flags[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.pWaitDstStageMask = flags;

    VkResult result = vkQueueSubmit(    context.device.graphics_queue,
                                        1,
                                        &submit_info,
                                        context.in_flight_fences[context.current_frame].handle);
    if(result != VK_SUCCESS){
        KERROR("vkQueueSubmit failed with result: %s", vulkan_result_string(result, TRUE));
        return FALSE;
    }


    vulkan_command_buffer_update_submitted(command_buffer);
    // End of the queue submition--------------------------------------------------------------


    // Give the image back to the swapchain
    vulkan_swapchain_present(&context,
        &context.swapchain,
        context.device.graphics_queue,
        context.device.present_queue,
        context.queue_complete_semaphores[context.current_frame],
        context.image_index);


    return TRUE;
}



VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {

    switch (message_severity) {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            KERROR(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            KWARN(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            KINFO(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            KTRACE(callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}



i32 find_memory_index(u32 type_filter, u32 property_flags){
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(context.device.physical_device, &memory_properties);

    for(u32 i = 0; i<memory_properties.memoryTypeCount; ++i){
        // Check each memory type to see if it its bits is set to 1.
        if(type_filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags)
        {
            return i;
        }
    }

    KWARN("Unable to find suitable memory type!");
    return -1;
}


void create_command_buffers(renderer_backend* backend){
    if(!context.graphics_command_buffers){
        context.graphics_command_buffers = darray_reserve(vulkan_command_buffer, context.swapchain.image_count);
        for (u32 i = 0; i<context.swapchain.image_count; ++i){
            kzero_memory(&context.graphics_command_buffers[i], sizeof(vulkan_command_buffer));
        }
    }

    for(u32 i = 0; i<context.swapchain.image_count; ++i){
        if(context.graphics_command_buffers[i].handle){
            vulkan_command_buffer_free(
                &context,
                context.device.graphics_command_pool,
                &context.graphics_command_buffers[i]);
        }

        kzero_memory(&context.graphics_command_buffers[i], sizeof(vulkan_command_buffer));
        vulkan_command_buffer_allocate(
            &context,
            context.device.graphics_command_pool,
            TRUE,
            &context.graphics_command_buffers[i]);
    }
    KINFO("Vulkan command buffers created.")
}

void regenerate_framebuffers(renderer_backend* backend, vulkan_swapchain* swapchain, vulkan_renderpass* renderpass){
    for(u32 i = 0; i<swapchain->image_count; ++i){
        // TODO: make this dynamic based on the currently configured attachments
        u32 attachment_count = 2;
        VkImageView attachments[] = {
            swapchain->views[i],
            swapchain->depth_attachment.view};

        vulkan_framebuffer_create(
            &context,
            renderpass,
            context.framebuffer_width,
            context.framebuffer_height,
            attachment_count,
            attachments,
            &context.swapchain.framebuffers[i]); 
    }
}