#pragma once

#include <SDL3/SDL_init.h>
#include <volk.h>

namespace window {
    bool Create(SDL_Window** sdl_window);
    void Destroy(SDL_Window** sdl_window);

    bool CreateSurface(SDL_Window** sdl_window, const VkInstance instance, VkSurfaceKHR* surface);
    void DestroySurface(const VkInstance instance, VkSurfaceKHR* surface);

    const char* const* GetInstanceExtensions(uint32_t* count);
    bool GetSizeInPixels(SDL_Window** sdl_window, int* width, int* height);
} // namespace window
