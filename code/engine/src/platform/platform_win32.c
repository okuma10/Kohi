#include "platform/platform.h"



// Windows platfrom layer.
#if KPLATFORM_WINDOWS
    #include<stdio.h>

    #include <windows.h>
    #include <windowsx.h>
    #include <stdlib.h>
    #include <string.h>

    #include "core/logger.h"
    #include "core/input.h"
    #include "core/event.h"

    #include "containers/darray.h"

    // For surface creation
    #include <vulkan/vulkan.h>
    #include <vulkan/vulkan_win32.h>
    #include "renderer/Vulkan/vulkan_types.inl"


    typedef struct internal_state{
        HINSTANCE h_instance;
        HWND hwnd;
        VkSurfaceKHR surface;
    } internal_state;


    //Clock
    static f64 clock_frequency;
    static LARGE_INTEGER start_time;


    LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param);


    // Init and shutdown
    b8 platform_startup(
        platform_state* plat_state,
        const char* application_name,
        i32 x,
        i32 y,
        i32 width,
        i32 height)
    {

        plat_state->internal_state = malloc(sizeof(internal_state));
        internal_state *state = (internal_state *)plat_state->internal_state;

        state->h_instance = GetModuleHandleA(0);


        // Setup and register window class.
        HICON icon = LoadIcon(state->h_instance, IDI_APPLICATION);
        WNDCLASSA wc;
        memset(&wc,0,sizeof(wc));
        wc.style = CS_DBLCLKS; // Get double-clicks
        wc.lpfnWndProc = win32_process_message;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = state->h_instance;
        wc.hIcon = icon;
        wc.hCursor = LoadCursor(NULL,IDC_ARROW); //NULL; // Manage the cursor manually
        wc.hbrBackground = NULL;                //Transparent
        wc.lpszClassName = "kohi_window_class";


        if(!RegisterClassA(&wc)){
            MessageBoxA(0, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
            return FALSE;
        }


        // Create Window
        u32 client_x        = x;
        u32 client_y        = y;
        u32 client_width    = width;
        u32 client_height   = height;

        u32 window_x        = client_x;
        u32 window_y        = client_y;
        u32 window_width    = client_width;
        u32 window_height   = client_height;

        //      Styling
        u32 window_style = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;
        u32 window_ex_style = WS_EX_APPWINDOW;

        window_style |= WS_MAXIMIZEBOX;
        window_style |= WS_MINIMIZEBOX;
        window_style |= WS_THICKFRAME;

        //      Obtain the size of the border
        RECT border_rect = {0, 0, 0, 0};
        AdjustWindowRectEx(&border_rect, window_style, 0, window_ex_style);

        //      In this case, the border rectangle is negative.
        window_x += border_rect.left;
        window_y += border_rect.top;

        //      Grow by the size of the OS border.
        window_width  += border_rect.right  - border_rect.left;
        window_height += border_rect.bottom - border_rect.top;

        //      Create
        HWND handle = CreateWindowExA(
            window_ex_style, "kohi_window_class", application_name,
            window_style, window_x, window_y, window_width, window_height,
            0, 0, state->h_instance, 0);

        
        // Check if all is ok
        if(handle == 0){
            MessageBoxA(NULL, "Wincow creation failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);

            KFATAL("Window creation failed!");
            return FALSE;
        }else{
            state->hwnd = handle;
        }


        // Show the window
        b32 should_activate = 1; //TODO: if the window should not accept input, this should be false.
        i32 show_window_command_flags = should_activate ? SW_SHOW : SW_SHOWNOACTIVATE;
        // If initially minimized, use SW_MINIMIZE : SW_SHOWMINNOACTIVE;
        // If initially maximized, use SW_SHOWMAXIMIZED : SW_MAXIMIZE
        ShowWindow(state->hwnd, show_window_command_flags);


        // Clock setup
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        clock_frequency = 1.0 /(f64)frequency.QuadPart;
        QueryPerformanceCounter(&start_time);


        return TRUE;

    }

    void platform_shutdown(platform_state* plat_state){
        // Simply cold-cast to the known type.
        internal_state *state = (internal_state *) plat_state->internal_state;

        if(state->hwnd){
            DestroyWindow(state->hwnd);
            state->hwnd = 0;
        }
    }



    // ?
    b8 platfrom_pump_messages(platform_state* plat_state){
        MSG message;
        while(PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)){
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        return TRUE;
    }



    // Memory managment
    void *platfrom_allocate(u64 size, b8 aligned){
        return malloc(size);
    }
    void  platfrom_free(void* block, b8 aligned){
        free(block);
    }
    void *platfrom_zero_memory(void* block, u64 size){
        return memset(block, 0, size);
    }
    void *platfrom_copy_memory(void* dest, const void* source, u64 size){
        return memcpy(dest, source,size);
    }
    void *platfrom_set_memory(void* dest, i32 value, u64 size){
        return memset(dest, value, size);
    }



    // Console output
    void platform_console_write(const char* message, u8 colour){
        // // Color
        // HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        // // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
        // static u8 levels[6] = {64, 4, 6, 2, 1, 8};
        // SetConsoleTextAttribute(console_handle, levels[colour]);

        char levels[6][44] ={"\033[48;2;235;059;090m\033[38;2;255;255;255m",
                             "\033[48;2;000;000;000m\033[38;2;235;059;090m",
                             "\033[48;2;000;000;000m\033[38;2;250;130;049m",
                             "\033[48;2;000;000;000m\033[38;2;247;183;049m",
                             "\033[48;2;000;000;000m\033[38;2;075;123;236m",
                             "\033[48;2;000;000;000m\033[38;2;209;216;224m"
                            };
        char* close_format="\033[0m";


        DWORD dwMode = 0;
        GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), dwMode);


        char new_str[6000];
        sprintf(new_str,"%s%s%s",levels[colour], message, close_format);
        u64 new_str_len = strlen(new_str);

        // Print
        OutputDebugStringA(new_str);
        LPDWORD number_wirtten = 0;

        WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), new_str, (DWORD)new_str_len, number_wirtten, 0);
    }
    void platfrom_console_write_error(const char* message, u8 colour){
        // Color
        //HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
        // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
        //static u8 levels[6] = {64, 4, 6, 2, 1, 8};
        //SetConsoleTextAttribute(console_handle, levels[colour]);

        char levels[6][50] ={"\033[48;2;235;059;090m\033[38;2;255;255;255;1m",
                             "\033[48;2;192;057;043m\033[38;2;000;000;000;1m",
                             "\033[48;2;000;000;000m\033[38;2;250;130;049m",
                             "\033[48;2;000;000;000m\033[38;2;247;183;049m",
                             "\033[48;2;000;000;000m\033[38;2;075;123;236m",
                             "\033[48;2;000;000;000m\033[38;2;209;216;224m"
                            };

        DWORD dwMode = 0;
        GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), dwMode);

        char* close_format="\033[0m";
    
        char new_str[6];
        sprintf(new_str,"%s%s%s",levels[colour], message, close_format);
        
        u64 new_str_len = strlen(new_str);

        
        // Print
        OutputDebugStringA(new_str);
        LPDWORD number_wirtten = 0;
        WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), new_str, (DWORD)new_str_len, number_wirtten, 0);
    }


    // Get time
    f64 platfrom_get_absolute_time(){
        LARGE_INTEGER now_time;
        QueryPerformanceCounter(&now_time);
        return (f64)now_time.QuadPart * clock_frequency; 
    }



    // Sleep on the thread for the provided ms. This blocks the main thread.
    // Should only be used for giving time back to the OS for unused update power.
    // Therefore it is not exported.
    void platform_sleep(u64 ms){
        Sleep(ms);
    }


    // Surface creation for Vulkan
    b8 platform_create_vulkan_surface(platform_state* plat_state, vulkan_context* context){
        // Simply cold-cast to the known type.
        internal_state *state = (internal_state *)plat_state->internal_state;

        VkWin32SurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        create_info.hinstance = state->h_instance;
        create_info.hwnd = state->hwnd;

        VkResult result = vkCreateWin32SurfaceKHR(context->instance, &create_info, context->allocator, &state->surface);
        if(result != VK_SUCCESS){
            KFATAL("Vulkan surface creation failed.");
            return FALSE;
        }

        context->surface = state->surface;

        return TRUE;
    }


    void platrofm_get_required_extension_names(const char*** names_darray){
        darray_push(*names_darray, &"VK_KHR_win32_surface");
    }

    

    LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param){
        switch(msg){
            case WM_ERASEBKGND:
                // Notify the Os that erasing will be handled by the application to prevent flicker.
                return 1;
            case WM_CLOSE:
                // TODO: Fire an event for the application to quit.
                event_context data = {};
                event_fire(EVENT_CODE_APPLICATION_QUIT, 0, data);
                return TRUE;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            case WM_SIZE: {
                // Get the updated size.
                // RECT r;
                // GetClientRect(hwnd, &r);
                // u32 width  = r.right - r.left;
                // u32 height = r.bottom - r.top;

                //TODO: Fire an event for window resize.
            }break;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                // Key pressed/released
                b8 pressed = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
                keys key = (u16)w_param;

                // Pass to the input subsystem for processing.
                input_process_key(key, pressed);
            }break;
            case WM_MOUSEMOVE:{
                // Mouse move
                i32 x_position = GET_X_LPARAM(l_param);
                i32 y_position = GET_Y_LPARAM(l_param);
                
                // Pass over to the input sybsystem
                input_process_mouse_move(x_position, y_position);
            }break;
            case WM_MOUSEWHEEL:{
                i32 z_delta = GET_WHEEL_DELTA_WPARAM(w_param);
                if(z_delta != 0){
                    // Flatten the input to an OS-idependent (-1,1)
                    z_delta = (z_delta < 0) ? -1 : 1;
                    
                    input_process_mouse_wheel(z_delta);
                }
            }break;
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:{
                b8 pressed = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
                
                buttons mouse_button = BUTTON_MAX_BUTTONS;
                switch(msg){
                    case WM_LBUTTONDOWN:
                    case WM_LBUTTONUP:
                        mouse_button = BUTTON_LEFT;
                        break;
                    case WM_MBUTTONDOWN:
                    case WM_MBUTTONUP:
                        mouse_button = BUTTON_MIDDLE;
                        break;
                    case WM_RBUTTONDOWN:
                    case WM_RBUTTONUP:
                        mouse_button = BUTTON_RIGHT;
                        break;
                }

                // Pass over to the input subsystem.
                if (mouse_button != BUTTON_MAX_BUTTONS){
                    input_process_button(mouse_button, pressed);
                }
            }break;
        }

        return DefWindowProcA(hwnd, msg, w_param, l_param);
    }



#endif // KPLATFORM WINDOWS

