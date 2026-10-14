cmake_minimum_required(VERSION 3.30)
project(spudlib)


# Define the allowed choices array
set(VALID_BACKENDS "Vulkan" "D3D12" "Metal")

# Initialize the cache variable with a default value
set(GRAPHICS_BACKEND "Vulkan" CACHE STRING "Choose exactly one graphics backend to compile")

# Create the drop-down menu restriction for interactive GUIs
set_property(CACHE GRAPHICS_BACKEND PROPERTY STRINGS ${VALID_BACKENDS})

# STRICT VALIDATION: Force selection to match exactly one choice from the list
if(NOT GRAPHICS_BACKEND IN_LIST VALID_BACKENDS)
    string(REPLACE ";" ", " PRINTABLE_BACKENDS "${VALID_BACKENDS}")
    message(FATAL_ERROR
        "Invalid backend selection: '${GRAPHICS_BACKEND}'.\n"
        "You must choose EXACTLY ONE option from the following list: [${PRINTABLE_BACKENDS}]"
    )
endif()

# Proceed with your clean compile-time single macro definitions...
message(STATUS "Selected Graphics Backend: ${GRAPHICS_BACKEND}")

# --- vendored-or-fetched dependency ----------------------------------------
# Probe eqdev/.deps/<dir> for a hand-vendored checkout and use it if present
# (one copy shared across the whole workspace, no per-repo duplication),
# otherwise fetch <repo> at the pinned <tag> so a standalone clone of this repo
# still configures with nothing vendored by hand. Probe <dir>/CMakeLists.txt,
# not just the directory, so an empty placeholder folder doesn't match.
include(FetchContent)
function(spud_declare_vendored_or_fetch name dir repo tag)
    # Accept either the plain `git clone` folder name (<dir>) or the
    # FetchContent-style <dir>-src, so a vendored checkout is found without
    # renaming. Probe <cand>/CMakeLists.txt, not just the directory, so an
    # empty placeholder folder doesn't match.
    foreach(_cand "${dir}" "${dir}-src")
        set(_vendored "${CMAKE_CURRENT_SOURCE_DIR}/../.deps/${_cand}")
        if(EXISTS "${_vendored}/CMakeLists.txt")
            message(STATUS "${name}: using vendored ${_vendored}")
            FetchContent_Declare(${name} SOURCE_DIR "${_vendored}")
            return()
        endif()
    endforeach()
    message(STATUS "${name}: no vendored copy under .deps/, fetching ${repo}@${tag}")
    FetchContent_Declare(${name} GIT_REPOSITORY "${repo}" GIT_TAG "${tag}")
endfunction()

# Don't do anything if another library/application is already compiling SpudLib
if(NOT TARGET spudlib)
    # Set initial SpudLib sources
    set(SPUDLIB_SOURCES
        include/spudaudio.h
        include/spudcore.h
        include/spudfiles.h
        include/spudgpu.h
        include/spudinput.h
        include/spudmemory.h
        include/spudnet.h
        include/spudperf.h
        src/net/spudnetshared.h
        src/input/spudinputshared.h
        src/spudcore.c
        src/spudmemory.c
        src/spudfiles.c
        src/spudperf.c
        src/spudnet.c
        src/net/spudnethttp.c
        src/net/spudnetwaitset.c
        src/spudaudio.c
        src/spudinput.c
        src/gpu/spudgpuformat.c
    )
    # Determine which backend sources to append.
    if(WIN32)
        list(APPEND SPUDLIB_SOURCES
            src/files/backends/windows/spudfileswindows.c
            src/net/backends/windows/spudnetwindows.c
            src/net/backends/windows/spudnetwinhttp.c
            src/memory/backends/windows/spudmemorywindows.c
        )
    endif()
    if(UNIX AND NOT APPLE)
        list(APPEND SPUDLIB_SOURCES
            src/files/backends/linux/spudfileslinux.c
            src/net/backends/posix/spudnetsockets.c
            src/net/backends/linux/spudnetcurl.c
            src/memory/backends/linux/spudmemorylinux.c
        )
    endif()
    if(APPLE)
        # SpudNet's HTTP/WebSocket backend is Objective-C (NSURLSession), on
        # every graphics backend. Must enable OBJC before add_library() below.
        enable_language(OBJC)
        list(APPEND SPUDLIB_SOURCES
            src/files/backends/apple/spudfilesapple.c
            src/net/backends/posix/spudnetsockets.c
            src/net/backends/apple/spudneturlsession.m
        )
    endif()

    # SpudInput: the native input APIs built for the target platform, published
    # as PUBLIC SPUDINPUT_COMPILE_<api> definitions (after add_library). See
    # the table in include/spudinput.h. The front end (src/spudinput.c) is in
    # every build; with no API it has nothing to bind an instance to.
    set(SPUDINPUT_APIS "")
    if(APPLE)
        # GameController is on macOS, iOS and iPadOS; watchOS has none.
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
            set(SPUDINPUT_APIS GAMECONTROLLER)
        endif()
    endif()
    message(STATUS "SpudInput: native APIs: ${SPUDINPUT_APIS}")
    if("GAMECONTROLLER" IN_LIST SPUDINPUT_APIS)
        # Objective-C; OBJC is already enabled for Apple above.
        list(APPEND SPUDLIB_SOURCES src/input/backends/apple/spudinputgamecontroller.m)
    endif()

    # SpudAudio: the target platform and the native audio APIs built for it,
    # decided once here. They pick the sources and link libraries, and become
    # the PUBLIC SPUDAUDIO_PLATFORM_<platform> / SPUDAUDIO_COMPILE_<api>
    # definitions (after add_library), so SpudAudio's code and its hosts see
    # the same answer. See the table in include/spudaudio.h.
    set(SPUDAUDIO_PLATFORM "")
    set(SPUDAUDIO_APIS "")
    if(WIN32)
        set(SPUDAUDIO_PLATFORM WINDOWS)
        set(SPUDAUDIO_APIS WASAPI)
    elseif(APPLE)
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(SPUDAUDIO_PLATFORM MACOS)
            set(SPUDAUDIO_APIS COREAUDIO)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS") # iPhone and iPad
            set(SPUDAUDIO_PLATFORM IOS)          # no backend yet (AVAudioSession + RemoteIO)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "watchOS")
            set(SPUDAUDIO_PLATFORM WATCHOS)      # no backend yet
        else()
            message(FATAL_ERROR "SpudAudio: unsupported Apple platform '${CMAKE_SYSTEM_NAME}'")
        endif()
    elseif(UNIX)
        set(SPUDAUDIO_PLATFORM LINUX)
        set(SPUDAUDIO_APIS ALSA)
    endif()
    message(STATUS "SpudAudio: platform ${SPUDAUDIO_PLATFORM}, native APIs: ${SPUDAUDIO_APIS}")

    if("WASAPI" IN_LIST SPUDAUDIO_APIS)
        list(APPEND SPUDLIB_SOURCES src/audio/backends/windows/spudaudiowasapi.c)
    endif()
    if("ALSA" IN_LIST SPUDAUDIO_APIS)
        list(APPEND SPUDLIB_SOURCES src/audio/backends/linux/spudaudioalsa.c)
    endif()
    if("COREAUDIO" IN_LIST SPUDAUDIO_APIS)
        list(APPEND SPUDLIB_SOURCES src/audio/backends/apple/spudaudiocoreaudio.c)
    endif()
    if(SPUDAUDIO_PLATFORM MATCHES "^(MACOS|IOS|WATCHOS)$")
        # Shared by every Apple audio backend; builds on all Apple OSes.
        list(APPEND SPUDLIB_SOURCES
            src/audio/backends/apple/spudaudioapple.h
            src/audio/backends/apple/spudaudioapple.c
        )
    endif()
    if(NOT SPUDAUDIO_APIS)
        # No native API for this platform yet: argument checks only.
        list(APPEND SPUDLIB_SOURCES src/audio/backends/stub/spudaudiostub.c)
    endif()
    if(WIN32 OR(UNIX AND NOT APPLE))
        # Append Vulkan sources if Vulkan API is selected for compile.
        if(GRAPHICS_BACKEND STREQUAL "Vulkan")
            list(APPEND SPUDLIB_SOURCES
                src/gpu/backends/vulkan/spudgpuvulkan.h
                src/gpu/backends/vulkan/spudgpuvulkancontext.c
                src/gpu/backends/vulkan/spudgpuvulkanbuffer.c
                src/gpu/backends/vulkan/spudgpuvulkanheap.c
                src/gpu/backends/vulkan/spudgpuvulkanshader.c
                src/gpu/backends/vulkan/spudgpuvulkanswapchain.c
                src/gpu/backends/vulkan/spudgpuvulkanimage.c
                src/gpu/backends/vulkan/spudgpuvulkandescriptors.c
                src/gpu/backends/vulkan/spudgpuvulkancommand.c
                src/gpu/backends/vulkan/spudgpuvulkanrenderpass.c
                src/gpu/backends/vulkan/spudgpuvulkannative.c
                src/gpu/backends/vulkan/spudgpuvulkansync.c
                include/spudgpu_vulkan_natives.h
            )

        # Append D3D12 sources if D3D12 API is selected for compile.
        elseif(GRAPHICS_BACKEND STREQUAL "D3D12")
            list(APPEND SPUDLIB_SOURCES
                src/gpu/backends/d3d12/spudgpud3d12.hpp
                src/gpu/backends/d3d12/spudgpud3d12context.cpp
                src/gpu/backends/d3d12/spudgpud3d12buffer.cpp
                src/gpu/backends/d3d12/spudgpud3d12heap.cpp
                src/gpu/backends/d3d12/spudgpud3d12shader.cpp
                src/gpu/backends/d3d12/spudgpud3d12swapchain.cpp
                src/gpu/backends/d3d12/spudgpud3d12image.cpp
                src/gpu/backends/d3d12/spudgpud3d12descriptors.cpp
                src/gpu/backends/d3d12/spudgpud3d12command.cpp
                src/gpu/backends/d3d12/spudgpud3d12renderpass.cpp
                src/gpu/backends/d3d12/spudgpud3d12native.cpp
                src/gpu/backends/d3d12/spudgpud3d12sync.cpp
                include/spudgpu_d3d12_natives.h
            )
        endif()

        # Append SDL3 support for Windows and Linux systems, but not Apple.
        list(APPEND SPUDLIB_SOURCES
            include/spudgpu_sdl3.h
        )
    elseif(APPLE)
        # Append Metal sources if Metal API is selected for compile.
        if (GRAPHICS_BACKEND STREQUAL "Metal")
            # Metal backend is plain Objective-C (.m), not Objective-C++ -
            # chosen for lowest compile overhead (matches the Vulkan
            # backend's plain-C compile speed rather than D3D12's C++
            # backend). Must enable OBJC before add_library() below.
            enable_language(OBJC)

            list(APPEND SPUDLIB_SOURCES
                include/spudgpu_metal_natives.h
                src/gpu/backends/metal/spudgpumetalcontext.m
                src/gpu/backends/metal/spudgpumetalbuffer.m
                src/gpu/backends/metal/spudgpumetalheap.m
                src/gpu/backends/metal/spudgpumetalshader.m
                src/gpu/backends/metal/spudgpumetalswapchain.m
                src/gpu/backends/metal/spudgpumetalimage.m
                src/gpu/backends/metal/spudgpumetaldescriptors.m
                src/gpu/backends/metal/spudgpumetalcommand.m
                src/gpu/backends/metal/spudgpumetalrenderpass.m
                src/gpu/backends/metal/spudgpumetalnative.m
                src/gpu/backends/metal/spudgpumetalsync.m
            )
        endif()
    endif()

    # After determing source backends, add_library to SpudLib target.
    add_library(spudlib STATIC ${SPUDLIB_SOURCES})

    # _DEBUG in a Debug build, for every compiler. MSVC's debug CRT defines it
    # already (CMAKE_MSVC_RUNTIME_LIBRARY pairs the two per config); GCC and
    # Clang define nothing, so without this a Debug build there leaves out
    # everything under `#if _DEBUG`. PUBLIC because spudcore.h and spudgpu.h
    # test it: structs in them change with it, so spudlib and whatever links
    # it must agree.
    if(NOT MSVC)
        target_compile_definitions(spudlib PUBLIC $<$<CONFIG:Debug>:_DEBUG>)
    endif()

    # Now set the compile-time macros and packages and whatnot.
    if(WIN32)
        target_compile_definitions(spudlib PUBLIC SPUDLIB_PLATFORM_WIN32=1)
        target_compile_definitions(spudlib PUBLIC SPUDLIB_PLATFORM_WINDOWS=1)
        # winhttp: SpudNet's HTTP and WebSocket clients. crypt32: their
        # certificate checks against a caller's own roots and pins.
        target_link_libraries(spudlib PRIVATE shlwapi ws2_32 winhttp crypt32)
    elseif (UNIX AND NOT APPLE)
        target_compile_definitions(spudlib PUBLIC SPUDLIB_PLATFORM_LINUX=1)
        # SpudNet's HTTP and WebSocket clients (and its instance): libcurl4-openssl-dev /
        # libcurl-devel, and pthreads for the WebSocket's lock. All of SpudNet
        # needs libcurl 7.84 or later: a spudnet_instance may be made while
        # other threads run, and libcurl's global init is only safe then from
        # that version on. It also has to have been built thread-safe, which
        # only the library itself can say; spudnet_instance_create asks it at
        # run time. WebSocket needs 8.11 or later and otherwise compiles to
        # entry points that return SPUDRESULT_SPUDNET_UNSUPPORTED.
        find_package(CURL 7.84 REQUIRED)
        find_package(Threads REQUIRED)
        target_link_libraries(spudlib PRIVATE CURL::libcurl Threads::Threads)
        # libm: spudlib calls libm (sqrtf, etc.) internally. On Linux that's a
        # separate DSO; Windows folds it into the CRT and Apple into libSystem,
        # so it's only linked here. spudlib builds STATIC, so this propagates to
        # consumers' final link line too - which is also why a sample or host
        # that calls libm itself doesn't have to repeat it.
        target_link_libraries(spudlib PRIVATE m)
        if(CURL_VERSION_STRING VERSION_LESS "8.11")
            message(WARNING "SpudNet: libcurl ${CURL_VERSION_STRING} is older than 8.11 - spudnet_websocket_* will return SPUDRESULT_SPUDNET_UNSUPPORTED.")
        endif()
    elseif (APPLE)
        target_compile_definitions(spudlib PUBLIC SPUDLIB_PLATFORM_APPLE=1)
        # SPUDLIB_PLATFORM_APPLE is every Apple system. SpudNet has to tell
        # Apple Watch apart: the system refuses TCP sockets and WebSockets to
        # an ordinary application there, and spudnet.h leaves both out
        # (SPUDNET_EXT_TCP, SPUDNET_EXT_WEBSOCKET) - and stops the build if
        # this is missing from a watchOS one. The TCP and wait-set sources
        # stay in the list above; with both parts out they compile to nothing.
        if(CMAKE_SYSTEM_NAME STREQUAL "watchOS")
            target_compile_definitions(spudlib PUBLIC SPUDLIB_PLATFORM_WATCHOS=1)
        endif()
        # Foundation: NSURLSession, for SpudNet's HTTP and WebSocket clients.
        # Security: SecTrust, for their certificate checks.
        target_link_libraries(spudlib PRIVATE "-framework Foundation" "-framework Security")
    endif()

    # SpudInput definitions and libraries, from the choice made above.
    foreach(api IN LISTS SPUDINPUT_APIS)
        target_compile_definitions(spudlib PUBLIC SPUDINPUT_COMPILE_${api}=1)
    endforeach()
    if("GAMECONTROLLER" IN_LIST SPUDINPUT_APIS)
        # CoreHaptics is what GameController plays a gamepad's motors through.
        target_link_libraries(spudlib PRIVATE "-framework GameController" "-framework CoreHaptics" "-framework Foundation")
    endif()

    # SpudAudio definitions and libraries, from the choice made above.
    if(SPUDAUDIO_PLATFORM)
        target_compile_definitions(spudlib PUBLIC SPUDAUDIO_PLATFORM_${SPUDAUDIO_PLATFORM}=1)
    endif()
    foreach(api IN LISTS SPUDAUDIO_APIS)
        target_compile_definitions(spudlib PUBLIC SPUDAUDIO_COMPILE_${api}=1)
    endforeach()
    if("WASAPI" IN_LIST SPUDAUDIO_APIS)
        # ole32: COM (CoCreateInstance / CoIncrementMTAUsage).
        # avrt: MMCSS (AvSetMmThreadCharacteristicsW) for the audio thread.
        target_link_libraries(spudlib PRIVATE ole32 avrt)
    endif()
    if("ALSA" IN_LIST SPUDAUDIO_APIS)
        # libasound2-dev / alsa-lib-devel, and pthreads for the audio thread.
        find_package(ALSA REQUIRED)
        find_package(Threads REQUIRED)
        # libudev-dev / systemd-devel: udev's "sound" events, which are how
        # device-change notification hears of a card (ALSA reports none).
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SPUDAUDIO_LIBUDEV REQUIRED IMPORTED_TARGET libudev)
        target_link_libraries(spudlib PRIVATE ALSA::ALSA Threads::Threads PkgConfig::SPUDAUDIO_LIBUDEV)
    endif()
    if("COREAUDIO" IN_LIST SPUDAUDIO_APIS)
        # The HAL, and CoreFoundation for its CFString names.
        target_link_libraries(spudlib PRIVATE "-framework CoreAudio" "-framework CoreFoundation")
    endif()
    if(SPUDAUDIO_PLATFORM MATCHES "^(MACOS|IOS)$")
        # AudioToolbox expands channel-layout tags; the watchOS SDK has none.
        target_link_libraries(spudlib PRIVATE "-framework AudioToolbox")
    endif()

    if(WIN32 OR(UNIX AND NOT APPLE))
        # Set the Vulkan compile-time packages and macros.
        if(GRAPHICS_BACKEND STREQUAL "Vulkan")
            set_property(TARGET spudlib PROPERTY C_STANDARD 23)
            target_compile_definitions(spudlib PUBLIC SPUDGPU_COMPILE_VULKAN_API=1)

            if(WIN32)
                # Manually hint where Vulkan is located on Windows
                # On Linux, it is automatically /usr/include and /usr/lib64
                set(Vulkan_INCLUDE_DIR "C:/VulkanSDK/1.4.341.1/Include")
                set(Vulkan_LIBRARY "C:/VulkanSDK/1.4.341.1/Lib/vulkan-1.lib")
                set(Vulkan_GLSLC_EXECUTABLE "C:/VulkanSDK/1.4.341.1/Bin/glslc.exe")
            elseif(UNIX AND NOT APPLE)
                # The Vulkan WSI surface type is compiled in (spudgpuvulkanswapchain.c
                # switches on SPUDGPU_PLATFORM_XLIB / SPUDGPU_PLATFORM_WAYLAND), so it
                # must match the window handles the caller will pass: an X11
                # Display*/Window, or a wl_display*/wl_surface*. This is the caller's
                # choice, NOT something SpudLib sniffs from whichever dev packages
                # happen to be installed on the build machine - auto-detecting it
                # silently built a Wayland surface creator that then crashed in Mesa's
                # WSI when handed X11 handles. Pick it explicitly via SPUDGPU_LINUX_WSI;
                # Xlib is the default because it also works under XWayland.
                set(SPUDGPU_LINUX_WSI "Xlib" CACHE STRING "Linux Vulkan WSI surface type: Xlib or Wayland")
                set_property(CACHE SPUDGPU_LINUX_WSI PROPERTY STRINGS "Xlib" "Wayland")
                if(SPUDGPU_LINUX_WSI STREQUAL "Wayland")
                    find_package(PkgConfig REQUIRED)
                    pkg_check_modules(WAYLAND REQUIRED wayland-client)
                    target_compile_definitions(spudlib PUBLIC SPUDGPU_PLATFORM_WAYLAND VK_USE_PLATFORM_WAYLAND_KHR)
                    target_link_libraries(spudlib PRIVATE ${WAYLAND_LIBRARIES})
                    target_include_directories(spudlib PRIVATE ${WAYLAND_INCLUDE_DIRS})
                elseif(SPUDGPU_LINUX_WSI STREQUAL "Xlib")
                    target_compile_definitions(spudlib PUBLIC SPUDGPU_PLATFORM_XLIB VK_USE_PLATFORM_XLIB_KHR)
                    target_link_libraries(spudlib PRIVATE X11) # X11 is usually available without find_package on Linux
                else()
                    message(FATAL_ERROR "SPUDGPU_LINUX_WSI must be 'Xlib' or 'Wayland', got '${SPUDGPU_LINUX_WSI}'")
                endif()
            endif()

            # Add Vulkan packages after setting up everything else for Vulkan.
            find_package(Vulkan REQUIRED)
            target_include_directories(spudlib PRIVATE ${Vulkan_INCLUDE_DIRS})
            target_link_libraries(spudlib PRIVATE Vulkan::Vulkan)
        endif()

        # Set the D3D12 compile-time packages and macros.
        if(GRAPHICS_BACKEND STREQUAL "D3D12")
            set_property(TARGET spudlib PROPERTY CXX_STANDARD 26)
            set_property(TARGET spudlib PROPERTY CXX_SCAN_FOR_MODULES OFF)
            target_compile_definitions(spudlib PUBLIC SPUDGPU_COMPILE_D3D12_API=1)

            # SPIRV-Cross (for SPIR-V → HLSL cross-compilation)
            if(WIN32)
                # Use ENV hint so any installed Vulkan SDK version works.
                set(Vulkan_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")

                find_package(Vulkan REQUIRED)
                target_include_directories(spudlib PRIVATE ${Vulkan_INCLUDE_DIRS})

                # Built from source (not the Vulkan SDK's prebuilt .lib files) so it
                # picks up CMAKE_MSVC_RUNTIME_LIBRARY and matches our CRT per-config
                # (the SDK's libs are release-only /MD and mismatch /MDd debug builds).
                # Prefer eqdev/.deps/SPIRV-Cross if vendored (shared with the Metal
                # path and ApricotFields, no duplicate checkout); else fetch.
                include(FetchContent)
                set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
                set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
                set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
                set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
                set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
                set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
                set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
                spud_declare_vendored_or_fetch(spirv_cross SPIRV-Cross
                    https://github.com/KhronosGroup/SPIRV-Cross.git vulkan-sdk-1.4.363.0)
                set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
                FetchContent_MakeAvailable(spirv_cross)

                target_link_libraries(spudlib PRIVATE
                    d3d12
                    dxgi
                    dxguid
                    "$ENV{VULKAN_SDK}/Lib/dxcompiler.lib"
                    spirv-cross-hlsl
                    spirv-cross-glsl
                    spirv-cross-core
                )
            else()
                return()
            endif()
        endif()

        # Set up SDL3 stuff
        # Path to the folder containing SDL3Config.cmake
        # Usually: [SDL3-folder]/lib/cmake/SDL3
        if(WIN32)
            set(SDL3_DIR "../SDL3-3.4.8/cmake")
        endif()

        find_package(SDL3 REQUIRED)
        target_link_libraries(spudlib PRIVATE SDL3::SDL3)

        # Copy DLL to output folder (So the app actually runs)
        if(WIN32)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            )
        endif()
    elseif(APPLE)
        # Set the Metal compile-time packages and macros.
        if(GRAPHICS_BACKEND STREQUAL "Metal")
            target_compile_definitions(spudlib PUBLIC SPUDGPU_COMPILE_METAL_API=1)

            # Required to link against id<MTLDevice>/id<MTLCommandQueue>/etc.
            # QuartzCore is needed for CAMetalLayer (surface/swapchain
            # creation); Foundation for NSObject-family types Metal.h pulls in.
            target_link_libraries(spudlib PRIVATE
                "-framework Metal"
                "-framework QuartzCore"
                "-framework Foundation"
            )

            # SPIRV-Cross (for SPIR-V -> MSL cross-compilation) - same
            # dependency the D3D12 backend vendors for its own SPIR-V -> HLSL
            # step, just built with the MSL backend instead of the HLSL one.
            # The C API is required, not the C++ one: spudgpumetalshader.m is
            # plain Objective-C (see the OBJC note above), which can't link
            # against SPIRV-Cross's C++ template-heavy interface directly.
            #
            # Vendored under eqdev/.deps/SPIRV-Cross (the same eqdev/.deps/
            # convention already used for SDL3/glm/imgui-docking/etc.); if that
            # checkout isn't present (e.g. a standalone clone of this repo),
            # fetch the pinned tag instead.
            include(FetchContent)
            set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_MSL ON CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_C_API ON CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
            spud_declare_vendored_or_fetch(spirv_cross SPIRV-Cross
                https://github.com/KhronosGroup/SPIRV-Cross.git vulkan-sdk-1.4.363.0)
            set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
            FetchContent_MakeAvailable(spirv_cross)

            target_link_libraries(spudlib PRIVATE
                spirv-cross-c
                spirv-cross-msl
                spirv-cross-glsl
                spirv-cross-core
            )

            # Command-list encoding, image/buffer resources, and the swap
            # chain are implemented and verified against real Apple hardware
            # - see src/gpu/backends/metal/*.m. include/spudgpu_metal_natives.h
            # is drafted and mostly implemented alongside them. Shader/
            # pipeline creation (this block's reason for existing) and
            # descriptor set binding remain the newest pieces.
        endif()
    endif()

    target_precompile_headers(spudlib PRIVATE include/spudgpu.h)

    target_include_directories(spudlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

    # Hardware tests: each needs a real device (and spudaudio_sine a
    # listener), so they're built but not registered with ctest.
    option(SPUDLIB_BUILD_TESTS "Build SpudLib's hardware tests" ON)
    if(SPUDLIB_BUILD_TESTS)
        # SpudPerf's clocks and its wait. No hardware and nobody watching, so
        # this one is a ctest.
        enable_testing()
        add_executable(spudperf_test tests/spudperf_test.c)
        target_link_libraries(spudperf_test PRIVATE spudlib)
        add_test(NAME spudperf_test COMMAND spudperf_test)

        # SpudInput's front end, and whatever gamepads happen to be plugged
        # in. It passes with none, so it is a ctest too.
        add_executable(spudinput_test tests/spudinput_test.c)
        target_link_libraries(spudinput_test PRIVATE spudlib)
        if(APPLE)
            # The test runs the main run loop itself, which is how
            # GameController's devices arrive.
            target_link_libraries(spudinput_test PRIVATE "-framework CoreFoundation")
        endif()
        add_test(NAME spudinput_test COMMAND spudinput_test)

        add_executable(spudaudio_sine tests/spudaudio_sine.c)
        target_link_libraries(spudaudio_sine PRIVATE spudlib)
        if(UNIX AND NOT APPLE)
            target_link_libraries(spudaudio_sine PRIVATE m)
        endif()

        # SpudNet against a real network stack. It needs the server in
        # tests/spudnet_test_server.py running beside it (Python 3 and the
        # openssl command), which tests/run_spudnet_test.sh sees to, so it
        # isn't registered with ctest either.
        add_executable(spudnet_test tests/spudnet_test.c)
        target_link_libraries(spudnet_test PRIVATE spudlib)
        if(UNIX)
            find_package(Threads REQUIRED)
            target_link_libraries(spudnet_test PRIVATE Threads::Threads)
        endif()
    endif()
endif()
