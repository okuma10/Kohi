#include "platform.h"



#if PLATFORM_LINUX

    #include "core\logger.h"
    #include "core\input.h"
    #include "core\event.h"
    #include "containers\darray.h"

    #include <xcb/xcb.h>
    #include <X11/keysym.h>
    #include <X11/XKBlib.h>     // Sudo apt-get install libx11-dev
    #include <X11/Xlib.h>
    #include <X11/Xlib-xcb.h>   // sudo apt-get install libxkbcommon-x11-dev
    #include <sys/time.h>


    #if _POSIX_C_SOURCE >= 199309L
        #include <time.h> // nanosleep
    #else
        #include <unistd.h> // usleep
    #endif


    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>


    // For surface creation
    #define VK_USEPLATFORM_XCB_KHR
    #include <vulkan/vulkan.h>
    #include "renderer/Vulkan/vulkan_types.inl"


    typedef struct internal_state{
        Display             *display;
        xcb_connection_t    *connection;
        xcb_window_t        window;
        xcb_screen_t        *screen;
        xcb_atom_t          wm_protocols;
        xcb_atom_t          wm_delete_win;
        VkSurfaceKHR        surface;
    } internal_state;


    // Key translation
    keys translate_keycode(u32 x_keycode);


    // Init and shutdown
    KAPI b8 platform_startup(
        platform_state* plat_state,
        const char* application_name,
        i32 x,
        i32 y,
        i32 width,
        i32 height)
    {
        // Create the internal state
        plat_state->internal_state = malloc(sizeof(internal_state));
        internal_state *state = (internal_state *)plat_state->internal_state;


        // Connect to X
        state->display = XOpenDisplay(NULL);


        // Turn off key repeats.
        XAutoRepeatOff(state->display);


        // Retrive the connection from the display

        state->connection = XGetXCBConnection(state->display);


        // Check for Errors
        if (xcb_connection_has_error(state->connection)){
            KFATAL("Failed to connect to X server via XCB.");
            return FALSE;
        }


        // Loop through screens using iterator
        xcb_screen_itterator_t it = xcb_setup_roots_iterator(setup);
        int screen_p = 0;
        for (i32 s = screen_p; s > 0; s--){
            xcb_screen_next(&it);
        }


        // After screens have been looped through, assign it.
        state->screen = it.data;


        // Allocate XID for the window to be crated.
        state->window = xcb_generate_id(state->connection);


        // Register event types.
        // XCB_CW_BACK_PIXEL = filling then winfow bg with a signle colour
        // XCB_CW_EVENT_MASK = is required
        u32 event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

        // Listen for keyboard and mouse buttons
        u32 event_values =  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                            XCB_EVENT_MASK_KEY_PRESS    | XCB_EVENT_MASK_KEY_RELEASE    |
                            XCB_EVENT_MASK_EXPOSURE     | XCB_EVENT_MASK_POINTER_MOTION |
                            XCB_EVENT_MASK_STRUCTURE_NOTIFY;        


        // Values to be sent over XCB (bg colour, events)
        u32 value_list [] = {state->screen->black_pixel, event_values};


       // Create the window
       xcb_void_cookie_t cookie = xcb_create_window(
           state->connection,
           XCB_COPY_FROM_PARENT,            // Depth
           state->window,
           state->screen->root,             // Parent
           x,                               // X
           y,                               // Y
           width,                           // Width
           height,                          // Height
           0,                               // No Border
           XCB_WINDOW_CLASS_INPUT_OUTPUT,   // Class
           state->screen->root_visual,
           event_mask,
           value_list);


        // Chane the Title
        xcb_change_property(
            state->connection,
            XCB_PROP_MODE_REPLACE,
            state->window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,                  // Data should be viewed 8 bits at a time
            strlen(application_name),
            application_name);


        // Tell the server to notify when the window manager
        // attempts to destroy the window
        xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(
            state->connection,
            0,
            strlen("WM_DELETE_WINDOW"),
            "WM_DELETE_WINDOW");
        xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
            state->connection,
            0,
            strlen("WM_PROTOCOLS"),
            "WM_PROTOCOLS");
        xcb_intern_atom_reply_t *wm_delete_reply = xcb_intern_atom_reply(
            state->connection,
            wm_delete_cookie,
            NULL);
        xcb_intern_atom_reply_t *wm_protocols_reply = xcb_intern_atom_reply(
            state->connection,
            wm_protocols_cookie,
            NULL);
        state->wm_delete_win = wm_delete_reply->atom;
        state->wm_protocols  = wm_protocols_reply->atom;


        xcb_change_property(
            state->connection,
            XCB_PROP_MODE_REPLACE,
            state->window,
            wm_protocols_reply->atom,
            4,
            32,
            1,
            &wm_delete_reply-> atom);


        // Map the window to the screen
        xcb_map_window(state->connection, state->window);


        // Flush the stream
        i32 stream_result = xcb_flush(state->connection);
        if(stream_result <= 0){
            KFATAL("An error occured when flushing the stream: %d", stream_result);
            return FALSE;
        }


        return TRUE;
    }
    KAPI void platform_shutdown(platform_state* plat_state){
        // simply cold-cast to the known type.
        internal_state *state = (internal_state *)plat_state->internal_state;


        // Trun key repeats back on since this is global for the os... just... wow.
        XAutoRepeatOn(state->display);


        xcb_destroy_window(state->connection, state->window);

    }


    // ?
    KAPI b8 platfrom_pump_messages(platform_state* plat_state){
        internal_state *state = (internal_state *)plat_state->internal_state;

        xcb_generic_event_t *event;
        xcb_client_message_event_t *cm;

        b8 quit_flagged = FALSE;


        // Poll for events until null is returned.
        while (event != 0) {
            event = xcb_pool_for_event(state-connection);
            if(event == 0){
                break;
            }


            // Input events
            switch (event->response_type & -0x80){
                case XCB_KEY_PRESS:
                case XCB_KEY_RELEASE:{
                    // TODO: Key Presses and releases
                    xcb_key_press_event_t *kb_event = (xcb_key_press_event_t *)event;
                    b8 pressed = event->response_type == XCB_KEY_PRESS;
                    xcb_keykode_t code = kb_event->detail;
                    KeySym key_sym = XkbKeycodeToKeySym(
                        state->display,
                        (KeyCode)code, //event.xkey.keycode,
                        0,
                        code & ShiftMask ? 1 : 0);

                    keys key = translate_keycode(key_sym);

                    //Pass to the input subsystem for processing.
                    input_process_key(key, pressed);


                }break;
                case XCB_BUTTON_PRESS:
                case XCB_BUTTON_RELEASE: {
                    xcb_button_press_event_t *mouse_event = (xcb_button_press_event_t *) event;
                    b8 pressed = event->response_type == XCB_BUTTON_PRESS;
                    buttons mouse_button = BUTTON_MAX_BUTTONS;
                    switch(mouse_event->detail){
                        case XCB_BUTTON_INDEX_1:
                            mouse_button = BUTTON_LEFT;
                            break;
                        case XCB_BUTTON_INDEX_2:
                            mouse_button = BUTTON_MIDDLE;
                            break;
                        case XCB_BUTTON_INDEX_3:
                            mouse_button = BUTTON_RIGHT;
                            break;
                    }

                    // Pass over to the input subsystem.
                    if(mouse_button != BUTTON_MAX_BUTTONS){
                        input_process_button(mouse_button, pressed);
                    }
                }break;
                case XCB_MOTION_NOTIFY:
                    // Mouse move
                    xcb_motion_notify_event_t *move_event = (xcb_motion_notify_event_t *)event;

                    //Pass over to the input subsystem.
                    input_process_mouse_move(move_event->event_x, move_event->event.y);

                break;

                case XCB_CONFIGURE_NOTIFY:{
                    // TODO: Resizing   
                }break;
                
                case XCB_CLIENT_MESSAGE: {
                    cm = (xcb_client_message_event_t *)event;

                    // Window close
                    if (cm->data.data32[0] == state->wm_delete_win){
                        quit_flagged = TRUE;
                    }
                }break;

                default:
                    // Something else
                    break;

            }

            free(event);
        }

        return !quit_flagged;
    }



    // Memory managment
    void* platfrom_allocate(u64 size, b8 aligned){
        return malloc(size);
    }
    void  platfrom_free(void* block, b8 aligned){
        free(block);
    }
    void* platfrom_zero_memory(void* block, u64 size){
        return memset(block, 0, size);
    }
    void* platfrom_copy_memory(void* dest, const void* source, u64 size){
        return memcpy(dest, source, size);
    }
    void* platfrom_set_memory(void* dest, i32 value, u64 size){
        return memset(dest, value, size);
    }



    // Console output
    void platform_console_write(const char* message, u8 colour){
        // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
        const char* colour_strings[] = {"0;41", "1;31", "1;33", "1;32", "1:34", "1;30"};
        printf("\033[%sm%s\033[0m," colour_strings[colour],message);
    }
    void platfrom_console_write_error(const char* message, u8 colour){
        // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
        const char* colour_strings[] = {"0;41", "1;31", "1;33", "1;32", "1:34", "1;30"};
        printf("\033[%sm%s\033[0m," colour_strings[colour],message);
    }


    // Get time
    f64 platfrom_get_absolute_time(){
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return now.tv_sec + now.tv_nsec * 0.000000001;
    }



    // Sleep on the thread for the provided ms. This blocks the main thread.
    // Should only be used for giving time back to the OS for unused update power.
    // Therefore it is not exported.
    void platform_sleep(u64 ms){
        #if _POSIX_C_SOURCE >= 199309L
            struct timespec ts;
            ts.tv_sec = ms/1000;
            ts.tv_nsec = (ms % 1000) * 1000 * 1000;
            nanosleep(&ts, 0);
        #else
            if(ms >= 1000){
                sleep(ms / 1000);
            }
            usleep((ms % 1000) * 1000);
        #endif
    }

    void platrofm_get_required_extension_names(const char*** names_darray){
        darray_push(*names_array, &"VK_KHR_xcb_surface");
    }


    // Surface creation for Vulkan
    b8 platform_create_vulkan_surface(platform_state* plat_state, vulkan_context* context){
        // Simply cold cast to the known type.
        internal_state *state = (internal_state)plat_state->internal_state;

        VkXcbSurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR};
        create_info.connection = state->connection;
        create_info.window = state->window;

        vkRsult result = vkCreateXcbSurfaceKHR(
            context->instance,
            &create_info,
            context->allocator,
            &state->surface);

        if(result != VK_SUCCESS){
            KFATAL("Vulkan surface creation failed.");
            return FALSE;
        }

        context->surface = state->surface;
        return TRUE;
    }


    keys translate_keycode(u32 x_keycode){
        switch(x_keycode){
            case XK_BackSpace:
                return KEY_BACKSPACE;
        }
    }




#endif