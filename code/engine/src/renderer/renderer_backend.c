#include "renderer_backend.h"
#include "Vulkan/vulkan_backend.h"



b8 renderer_backend_create(enum renderer_backend_type type, struct platform_state* plat_state, renderer_backend* out_renderer_backend){
    out_renderer_backend->plat_state = plat_state;

    if (type == RENDERER_BACKEDND_TYPE_VULKAN){
        out_renderer_backend->initialize    = vulkan_renderer_backend_initialize;
        out_renderer_backend->shutdown      = vulkan_renderer_backend_shutdown;
        out_renderer_backend->begin_frame   = vulkan_renderer_backend_begin_frame;
        out_renderer_backend->end_frame     = vulkan_renderer_backend_end_frame;
        out_renderer_backend->resized       = vulkan_renderer_backend_on_resized;

        return TRUE;
    }
    if (type == RENDERER_BACKEDND_TYPE_OPENGL){
        // TODO: Fill

        return TRUE;
    }
    if (type == RENDERER_BACKEDND_TYPE_DIRECTX){
        // TODO: Fill

        return TRUE;
    }

    return FALSE;
}


void renderer_backend_destroy(renderer_backend* renderer_backend){
    renderer_backend->initialize = 0;
    renderer_backend->shutdown = 0;
    renderer_backend->begin_frame = 0;
    renderer_backend->end_frame = 0;
    renderer_backend->resized = 0;
}