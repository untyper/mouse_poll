#ifndef MOUSEPOLL_HPP
#define MOUSEPOLL_HPP

#include <iostream>
#include <memory>
#include <thread>
#include <random>
#include <functional>
#include <windows.h>

class mouse_poll {
private:
    enum struct state {
        running,
        stopped
    } polling_state = state::stopped;

    // void (*callback)(mouse_poll*);
    std::function<void(mouse_poll*)> callback;
    RAWINPUTDEVICE devices[1];

    struct {
        std::unique_ptr<std::thread> ptr;
        MSG message;
    } thread;

    struct {
        HWND handle;
        HINSTANCE instance;
        char className[26];
    } window;

    // Main message loop for thread's message queue
    void message_loop() {
        while(
            GetMessage(&this->thread.message, NULL, 0, 0) > 0
            && this->polling_state == state::running
        ) {
            TranslateMessage(&this->thread.message);
            DispatchMessage(&this->thread.message);
        }

        // On quit, unregister mouse device and destroy message window
        this->unregister_mouse_device();
        this->destroy_message_window();
    }

    // Non-static window procedures can't be used to register a window class
    // https://stackoverflow.com/a/17221900
    inline static LRESULT CALLBACK procedure_router(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
        // Get pointer to class instance from window's userdata
        mouse_poll* context_mouse_poll = reinterpret_cast<mouse_poll*>(
            GetWindowLongPtr(handle, GWLP_USERDATA)
        );

        if (context_mouse_poll)
            return context_mouse_poll->window_procedure(handle, message, wParam, lParam);
        return DefWindowProc(handle, message, wParam, lParam);
    }

    // Window procedure for handling raw mouse input
    LRESULT CALLBACK window_procedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_INPUT) {
            UINT raw_input_size = sizeof(RAWINPUT);
            RAWINPUT raw_input[raw_input_size];

            if (GetRawInputData(
                    (HRAWINPUT)lParam, RID_INPUT, raw_input, &raw_input_size, sizeof(RAWINPUTHEADER)
                ) != (UINT)-1) {
                if (raw_input->header.dwType == RIM_TYPEMOUSE) {
                    USHORT& usButtonFlags = raw_input->data.mouse.usButtonFlags;

                    // Movement delta
                    this->input.delta_x = raw_input->data.mouse.lLastX;
                    this->input.delta_y = raw_input->data.mouse.lLastY;

                    // Wheel delta
                    if (usButtonFlags & RI_MOUSE_WHEEL)
                        this->input.delta_wheel = (short)raw_input->data.mouse.usButtonData / WHEEL_DELTA;
                    else this->input.delta_wheel = 0;

                    // Button states
                    if (usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
                        this->input.button1 = true;
                    else if (usButtonFlags & RI_MOUSE_BUTTON_1_UP)
                        this->input.button1 = false;

                    if (usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
                        this->input.button2 = true;
                    else if (usButtonFlags & RI_MOUSE_BUTTON_2_UP)
                        this->input.button2 = false;

                    if (usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
                        this->input.button3 = true;
                    else if (usButtonFlags & RI_MOUSE_BUTTON_3_UP)
                        this->input.button3 = false;

                    if (usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
                        this->input.button4 = true;
                    else if (usButtonFlags & RI_MOUSE_BUTTON_4_UP)
                        this->input.button4 = false;

                    if (usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
                        this->input.button5 = true;
                    else if (usButtonFlags & RI_MOUSE_BUTTON_5_UP)
                        this->input.button5 = false;

                    // Finally call user provided callback with the input data
                    this->callback(this);
                }
            }
        }
        return DefWindowProc(handle, message, wParam, lParam);
    }

    bool register_mouse_device() {
        this->devices[0].usUsagePage = 0x01;
        this->devices[0].usUsage     = 0x02;
        this->devices[0].dwFlags     = RIDEV_INPUTSINK; // 0x00000100
        this->devices[0].hwndTarget  = this->window.handle;

        if (!RegisterRawInputDevices(this->devices, 1, sizeof(this->devices[0]))) {
            std::cerr << "Failed to register raw input device. Error: " << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    bool unregister_mouse_device() {
        this->devices[0].dwFlags    = RIDEV_REMOVE;
        this->devices[0].hwndTarget = nullptr;

        if (!RegisterRawInputDevices(this->devices, 1, sizeof(this->devices[0]))) {
            std::cerr << "Failed to remove raw input device. Error: " << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    bool create_message_window() {
        this->window.handle = CreateWindowExA(
            0, this->window.className, "", 0, 0, 0, 0, 0,
            HWND_MESSAGE, 0, this->window.instance, 0
        );

        if (this->window.handle == nullptr) {
            std::cerr << "Failed to create message window. Error: " << GetLastError() << std::endl;
            return false;
        }

        // Store object pointer to window's user data so static procedure can route to member procedure
        SetLastError(0);
        if (!SetWindowLongPtr(this->window.handle, GWLP_USERDATA, (LONG_PTR)this) && GetLastError()) {
            std::cerr << "Failed to set pointer to object instance. Error: " << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    bool destroy_message_window() {
        if (!::DestroyWindow(this->window.handle)) {
            std::cerr << "Failed to destroy message window. Error: " << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    bool register_window_class() {
        WNDCLASSEXA windowClass;
        windowClass.cbSize        = sizeof(WNDCLASSEX);
        windowClass.style         = 0;
        windowClass.lpfnWndProc   = mouse_poll::procedure_router;
        windowClass.cbClsExtra    = 0;
        windowClass.cbWndExtra    = 0;
        windowClass.hInstance     = this->window.instance;
        windowClass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
        windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        windowClass.lpszMenuName  = NULL;
        windowClass.lpszClassName = this->window.className;
        windowClass.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

        if (!RegisterClassExA(&windowClass)) {
            std::cerr << "Failed to register window class. Error: " << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    // Give a random class name to message window
    void set_window_classname() {
        int length = 'z' - 'a'; // 25
        static std::random_device rd;
        static std::mt19937 mt(rd());
        static std::uniform_int_distribution<int> dist(0, length);

        for (int i = 0; i < length; i++)
            this->window.className[i] = 'a' + dist(mt);

        this->window.className[length] = 0;
    }

    void thread_entry() {
        this->window.instance = GetModuleHandleA(0);
        this->set_window_classname();

        if (this->register_window_class() && this->create_message_window()) {
            this->register_mouse_device();

            // Begin thread message loop
            this->message_loop();
        }
    }

public:
    // Struct where raw input will be stored
    struct {
        int
            delta_x     = 0,
            delta_y     = 0,
            delta_wheel = 0;
        bool
            button1 = false,
            button2 = false,
            button3 = false,
            button4 = false,
            button5 = false;
    } input;

    bool start() {
        // Create message window on another thread only if there isn't another thread running
        if (this->polling_state != state::running)
            this->polling_state = state::running;
        else return false;

        this->thread.ptr = std::make_unique<std::thread>(
            std::thread(&mouse_poll::thread_entry, this)
        );
        return true;
    }

    bool stop() {
        if (this->polling_state != state::running)
            return false;

        // Break message loop
        this->polling_state = state::stopped;

        if (!this->thread.ptr->joinable()) {
            std::cerr << "Failed to join with main thread." << std::endl;
            return false;
        }

        this->thread.ptr->join();
        this->thread.ptr.reset(nullptr);
        return true;
    }

    mouse_poll(std::function<void(mouse_poll*)> callback) {
        this->callback = callback;
    }
};

#endif