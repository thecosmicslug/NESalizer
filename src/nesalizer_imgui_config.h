#pragma once

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUIFS_NO_EXTRA_METHODS

#define IMGUI_DISABLE_DEMO_WINDOWS                        // Disable demo windows: ShowDemoWindow()/ShowStyleEditor() will be empty. Not recommended.
#define IMGUI_DISABLE_METRICS_WINDOW                      // Disable metrics/debugger and other debug tools: ShowMetricsWindow() and ShowStackToolWindow() will be empty.
#define IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS   // [Win32] Don't implement default clipboard handler. Won't use and link with OpenClipboard/GetClipboardData/CloseClipboard etc. (user32.lib/.a, kernel32.lib/.a)
#define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS         // [Win32] [Default with non-Visual Studio compilers] Don't implement default IME handler (won't require imm32.lib/.a)
#define IMGUI_DISABLE_WIN32_FUNCTIONS                     // no need for windows