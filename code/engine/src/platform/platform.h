#pragma once

#include "defines.h"



typedef struct platform_state{
    void* internal_state;
}platform_state;



// Init and shutdown
b8 platform_startup(
    platform_state* plat_state,
    const char* application_name,
    i32 x,
    i32 y,
    i32 width,
    i32 height);
void platform_shutdown(platform_state* plat_state);


// ?
b8 platfrom_pump_messages(platform_state* plat_state);



// Memory managment
void* platfrom_allocate(u64 size, b8 aligned);
void  platfrom_free(void* block, b8 aligned);
void* platfrom_zero_memory(void* block, u64 size);
void* platfrom_copy_memory(void* dest, const void* source, u64 size);
void* platfrom_set_memory(void* dest, i32 value, u64 size);



// Console output
void platform_console_write(const char* message, u8 colour);
void platfrom_console_write_error(const char* message, u8 colour);


// Get time
f64 platfrom_get_absolute_time();



// Sleep on the thread for the provided ms. This blocks the main thread.
// Should only be used for giving time back to the OS for unused update power.
// Therefore it is not exported.
void platform_sleep(u64 ms);