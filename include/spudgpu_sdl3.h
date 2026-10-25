//
// Created by nathanmoore on 5/22/26.
//

/**
 * @file spudgpu_sdl3.h
 * @brief Header-only helpers that create a SpudGPU surface for an SDL3
 * window.
 *
 * spudgpu_create_surface_from_sdl3() is defined for whichever graphics API
 * the build compiles (SPUDGPU_COMPILE_VULKAN_API, SPUDGPU_COMPILE_D3D12_API
 * or SPUDGPU_COMPILE_METAL_API). Header-only: nothing here is compiled
 * into SpudLib.
 */

#ifndef SPUDLIB_SPUDGPU_SDL3_H
#define SPUDLIB_SPUDGPU_SDL3_H

#include <spudgpu.h>

#if SPUDGPU_COMPILE_VULKAN_API
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_vulkan.h>

/**
 * @brief The spudgpu_create_surface_from_callback() callback that has
 * SDL_Vulkan_CreateSurface() make the native surface.
 *
 * Logs SDL's error with SDL_Log() on failure.
 *
 * @param[in]  vk_instance The instance's `VkInstance`.
 * @param[in]  user_data   The `SDL_Window *` to create the surface for.
 * @param[out] out_surface A `VkSurfaceKHR *` that receives the surface.
 *
 * @return true if the surface was created.
 */
static bool spudgpu_impl_sdl3_create_surface_fn(
    void *vk_instance,
    void *user_data,
    void *out_surface) {
    bool ok = SDL_Vulkan_CreateSurface(
        (SDL_Window *) user_data,
        (VkInstance) vk_instance,
        NULL,
        (VkSurfaceKHR *) out_surface);
    if (!ok)
        SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
    return ok;
}

/**
 * @brief Creates a surface for an SDL3 window (Vulkan).
 *
 * Calls spudgpu_create_surface_from_callback() with
 * spudgpu_impl_sdl3_create_surface_fn(), so SDL_Vulkan_CreateSurface()
 * makes the VkSurfaceKHR.
 *
 * @param[in] instance Instance to create the surface on.
 * @param[in] window   The SDL3 window.
 *
 * @return The new surface, or NULL if it could not be created.
 */
static inline spudgpu_surface spudgpu_create_surface_from_sdl3(
    spudgpu_instance instance,
    SDL_Window *window) {
    return spudgpu_create_surface_from_callback(
        instance,
        window,
        spudgpu_impl_sdl3_create_surface_fn);
}
#endif // SPUDGPU_COMPILE_VULKAN_API


#if SPUDGPU_COMPILE_D3D12_API
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
// windows.h, not a bare windef.h -- windef.h's own winnt.h include needs
// _AMD64_/_X86_ already defined, which only windows.h's preamble does (it
// translates the compiler's _M_AMD64/_M_IX86 before reaching windef.h).
#include <windows.h>

/**
 * @brief Creates a surface for an SDL3 window (Direct3D 12).
 *
 * Reads the window's HWND from its SDL properties and passes it to
 * spudgpu_create_surface(), with no display handle.
 *
 * @param[in] instance Instance to create the surface on.
 * @param[in] window   The SDL3 window.
 *
 * @return The new surface, or NULL if spudgpu_create_surface() failed.
 */
static inline spudgpu_surface spudgpu_create_surface_from_sdl3(
    spudgpu_instance instance,
    SDL_Window *window) {
    HWND hwnd = (HWND)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        NULL);
    spudgpu_surface surface = NULL;
    spudgpu_create_surface(instance, hwnd, NULL, &surface);
    return surface;
}
#endif // SPUDGPU_COMPILE_D3D12_API


#if SPUDGPU_COMPILE_METAL_API
#include <SDL3/SDL_metal.h>

/**
 * @brief Creates a surface for an SDL3 window (Metal).
 *
 * Creates an SDL_MetalView on the window and passes its CAMetalLayer to
 * spudgpu_create_surface(), with no display handle. The SDL_MetalView is
 * not destroyed here.
 *
 * @param[in] instance Instance to create the surface on.
 * @param[in] window   The SDL3 window.
 *
 * @return The new surface, or NULL if spudgpu_create_surface() failed.
 */
static inline spudgpu_surface spudgpu_create_surface_from_sdl3(
    spudgpu_instance instance,
    SDL_Window *window) {
    SDL_MetalView view = SDL_Metal_CreateView(window);
    void *metal_layer  = SDL_Metal_GetLayer(view);
    spudgpu_surface surface = NULL;
    spudgpu_create_surface(instance, metal_layer, NULL, &surface);
    return surface;
}
#endif // SPUDGPU_COMPILE_METAL_API

#endif //SPUDLIB_SPUDGPU_SDL3_H
