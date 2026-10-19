//! Auto-generated FFI bindings for spudgpu.h
//! Re-generate with: bindgen spudgpu.h -o bindings.rs -- -DSPUDGPU_COMPILE_VULKAN_API=1
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

use std::ffi::c_void;
use std::os::raw::{c_char, c_float};

// ----------------------------------------------------------------
// Opaque handle types
// Each spudgpu
// _foo is a *mut pointer to an opaque C struct.
// Users never construct these — only SpudGPU creates and destroys them.
// ----------------------------------------------------------------

#[repr(C)]
pub struct spudgpu_instance_t {
    _private: [u8; 0],
}
pub type spudgpu_instance = *mut spudgpu_instance_t;

#[repr(C)]
pub struct spudgpu_device_t {
    _private: [u8; 0],
}
pub type spudgpu_device = *mut spudgpu_device_t;

#[repr(C)]
pub struct spudgpu_resource_pool_t {
    _private: [u8; 0],
}
pub type spudgpu_resource_pool = *mut spudgpu_resource_pool_t;

#[repr(C)]
pub struct spudgpu_buffer_t {
    _private: [u8; 0],
}
pub type spudgpu_buffer = *mut spudgpu_buffer_t;

#[repr(C)]
pub struct spudgpu_buffer_view_t {
    _private: [u8; 0],
}
pub type spudgpu_buffer_view = *mut spudgpu_buffer_view_t;

#[repr(C)]
pub struct spudgpu_image_t {
    _private: [u8; 0],
}
pub type spudgpu_image = *mut spudgpu_image_t;

#[repr(C)]
pub struct spudgpu_image_view_t {
    _private: [u8; 0],
}
pub type spudgpu_image_view = *mut spudgpu_image_view_t;

#[repr(C)]
pub struct spudgpu_shader_pipeline_t {
    _private: [u8; 0],
}
pub type spudgpu_shader_pipeline = *mut spudgpu_shader_pipeline_t;

#[repr(C)]
pub struct spudgpu_command_list_t {
    _private: [u8; 0],
}
pub type spudgpu_command_list = *mut spudgpu_command_list_t;

#[repr(C)]
pub struct spudgpu_command_queue_t {
    _private: [u8; 0],
}
pub type spudgpu_command_queue = *mut spudgpu_command_queue_t;

#[repr(C)]
pub struct spudgpu_swap_chain_t {
    _private: [u8; 0],
}
pub type spudgpu_swap_chain = *mut spudgpu_swap_chain_t;

// ----------------------------------------------------------------
// SPUDGPU_FORMAT  (mirrors the enum in spudgpu.h)
// ----------------------------------------------------------------

pub type SPUDGPU_FORMAT = u32;

pub const SPUDGPU_FORMAT_UNKNOWN: SPUDGPU_FORMAT                     = 0;
pub const SPUDGPU_FORMAT_R32G32B32A32_TYPELESS: SPUDGPU_FORMAT       = 1;
pub const SPUDGPU_FORMAT_R32G32B32A32_FLOAT: SPUDGPU_FORMAT          = 2;
pub const SPUDGPU_FORMAT_R32G32B32A32_UINT: SPUDGPU_FORMAT           = 3;
pub const SPUDGPU_FORMAT_R32G32B32A32_SINT: SPUDGPU_FORMAT           = 4;
pub const SPUDGPU_FORMAT_R32G32B32_TYPELESS: SPUDGPU_FORMAT          = 5;
pub const SPUDGPU_FORMAT_R32G32B32_FLOAT: SPUDGPU_FORMAT             = 6;
pub const SPUDGPU_FORMAT_R32G32B32_UINT: SPUDGPU_FORMAT              = 7;
pub const SPUDGPU_FORMAT_R32G32B32_SINT: SPUDGPU_FORMAT              = 8;
pub const SPUDGPU_FORMAT_R16G16B16A16_TYPELESS: SPUDGPU_FORMAT       = 9;
pub const SPUDGPU_FORMAT_R16G16B16A16_FLOAT: SPUDGPU_FORMAT          = 10;
pub const SPUDGPU_FORMAT_R16G16B16A16_UNORM: SPUDGPU_FORMAT          = 11;
pub const SPUDGPU_FORMAT_R16G16B16A16_UINT: SPUDGPU_FORMAT           = 12;
pub const SPUDGPU_FORMAT_R16G16B16A16_SNORM: SPUDGPU_FORMAT          = 13;
pub const SPUDGPU_FORMAT_R16G16B16A16_SINT: SPUDGPU_FORMAT           = 14;
pub const SPUDGPU_FORMAT_R32G32_TYPELESS: SPUDGPU_FORMAT             = 15;
pub const SPUDGPU_FORMAT_R32G32_FLOAT: SPUDGPU_FORMAT                = 16;
pub const SPUDGPU_FORMAT_R32G32_UINT: SPUDGPU_FORMAT                 = 17;
pub const SPUDGPU_FORMAT_R32G32_SINT: SPUDGPU_FORMAT                 = 18;
pub const SPUDGPU_FORMAT_R32G8X24_TYPELESS: SPUDGPU_FORMAT           = 19;
pub const SPUDGPU_FORMAT_D32_FLOAT_S8X24_UINT: SPUDGPU_FORMAT        = 20;
pub const SPUDGPU_FORMAT_R32_FLOAT_X8X24_TYPELESS: SPUDGPU_FORMAT    = 21;
pub const SPUDGPU_FORMAT_X32_TYPELESS_G8X24_UINT: SPUDGPU_FORMAT     = 22;
pub const SPUDGPU_FORMAT_R10G10B10A2_TYPELESS: SPUDGPU_FORMAT        = 23;
pub const SPUDGPU_FORMAT_R10G10B10A2_UNORM: SPUDGPU_FORMAT           = 24;
pub const SPUDGPU_FORMAT_R10G10B10A2_UINT: SPUDGPU_FORMAT            = 25;
pub const SPUDGPU_FORMAT_R11G11B10_FLOAT: SPUDGPU_FORMAT             = 26;
pub const SPUDGPU_FORMAT_R8G8B8A8_TYPELESS: SPUDGPU_FORMAT           = 27;
pub const SPUDGPU_FORMAT_R8G8B8A8_UNORM: SPUDGPU_FORMAT              = 28;
pub const SPUDGPU_FORMAT_R8G8B8A8_UNORM_SRGB: SPUDGPU_FORMAT         = 29;
pub const SPUDGPU_FORMAT_R8G8B8A8_UINT: SPUDGPU_FORMAT               = 30;
pub const SPUDGPU_FORMAT_R8G8B8A8_SNORM: SPUDGPU_FORMAT              = 31;
pub const SPUDGPU_FORMAT_R8G8B8A8_SINT: SPUDGPU_FORMAT               = 32;
pub const SPUDGPU_FORMAT_R16G16_TYPELESS: SPUDGPU_FORMAT             = 33;
pub const SPUDGPU_FORMAT_R16G16_FLOAT: SPUDGPU_FORMAT                = 34;
pub const SPUDGPU_FORMAT_R16G16_UNORM: SPUDGPU_FORMAT                = 35;
pub const SPUDGPU_FORMAT_R16G16_UINT: SPUDGPU_FORMAT                 = 36;
pub const SPUDGPU_FORMAT_R16G16_SNORM: SPUDGPU_FORMAT                = 37;
pub const SPUDGPU_FORMAT_R16G16_SINT: SPUDGPU_FORMAT                 = 38;
pub const SPUDGPU_FORMAT_R32_TYPELESS: SPUDGPU_FORMAT                = 39;
pub const SPUDGPU_FORMAT_D32_FLOAT: SPUDGPU_FORMAT                   = 40;
pub const SPUDGPU_FORMAT_R32_FLOAT: SPUDGPU_FORMAT                   = 41;
pub const SPUDGPU_FORMAT_R32_UINT: SPUDGPU_FORMAT                    = 42;
pub const SPUDGPU_FORMAT_R32_SINT: SPUDGPU_FORMAT                    = 43;
pub const SPUDGPU_FORMAT_R24G8_TYPELESS: SPUDGPU_FORMAT              = 44;
pub const SPUDGPU_FORMAT_D24_UNORM_S8_UINT: SPUDGPU_FORMAT           = 45;
pub const SPUDGPU_FORMAT_R24_UNORM_X8_TYPELESS: SPUDGPU_FORMAT       = 46;
pub const SPUDGPU_FORMAT_X24_TYPELESS_G8_UINT: SPUDGPU_FORMAT        = 47;
pub const SPUDGPU_FORMAT_R8G8_TYPELESS: SPUDGPU_FORMAT               = 48;
pub const SPUDGPU_FORMAT_R8G8_UNORM: SPUDGPU_FORMAT                  = 49;
pub const SPUDGPU_FORMAT_R8G8_UINT: SPUDGPU_FORMAT                   = 50;
pub const SPUDGPU_FORMAT_R8G8_SNORM: SPUDGPU_FORMAT                  = 51;
pub const SPUDGPU_FORMAT_R8G8_SINT: SPUDGPU_FORMAT                   = 52;
pub const SPUDGPU_FORMAT_R16_TYPELESS: SPUDGPU_FORMAT                = 53;
pub const SPUDGPU_FORMAT_R16_FLOAT: SPUDGPU_FORMAT                   = 54;
pub const SPUDGPU_FORMAT_D16_UNORM: SPUDGPU_FORMAT                   = 55;
pub const SPUDGPU_FORMAT_R16_UNORM: SPUDGPU_FORMAT                   = 56;
pub const SPUDGPU_FORMAT_R16_UINT: SPUDGPU_FORMAT                    = 57;
pub const SPUDGPU_FORMAT_R16_SNORM: SPUDGPU_FORMAT                   = 58;
pub const SPUDGPU_FORMAT_R16_SINT: SPUDGPU_FORMAT                    = 59;
pub const SPUDGPU_FORMAT_R8_TYPELESS: SPUDGPU_FORMAT                 = 60;
pub const SPUDGPU_FORMAT_R8_UNORM: SPUDGPU_FORMAT                    = 61;
pub const SPUDGPU_FORMAT_R8_UINT: SPUDGPU_FORMAT                     = 62;
pub const SPUDGPU_FORMAT_R8_SNORM: SPUDGPU_FORMAT                    = 63;
pub const SPUDGPU_FORMAT_R8_SINT: SPUDGPU_FORMAT                     = 64;
pub const SPUDGPU_FORMAT_A8_UNORM: SPUDGPU_FORMAT                    = 65;
pub const SPUDGPU_FORMAT_R1_UNORM: SPUDGPU_FORMAT                    = 66;
pub const SPUDGPU_FORMAT_R9G9B9E5_SHAREDEXP: SPUDGPU_FORMAT          = 67;
pub const SPUDGPU_FORMAT_R8G8_B8G8_UNORM: SPUDGPU_FORMAT             = 68;
pub const SPUDGPU_FORMAT_G8R8_G8B8_UNORM: SPUDGPU_FORMAT             = 69;
pub const SPUDGPU_FORMAT_BC1_TYPELESS: SPUDGPU_FORMAT                = 70;
pub const SPUDGPU_FORMAT_BC1_UNORM: SPUDGPU_FORMAT                   = 71;
pub const SPUDGPU_FORMAT_BC1_UNORM_SRGB: SPUDGPU_FORMAT              = 72;
pub const SPUDGPU_FORMAT_BC2_TYPELESS: SPUDGPU_FORMAT                = 73;
pub const SPUDGPU_FORMAT_BC2_UNORM: SPUDGPU_FORMAT                   = 74;
pub const SPUDGPU_FORMAT_BC2_UNORM_SRGB: SPUDGPU_FORMAT              = 75;
pub const SPUDGPU_FORMAT_BC3_TYPELESS: SPUDGPU_FORMAT                = 76;
pub const SPUDGPU_FORMAT_BC3_UNORM: SPUDGPU_FORMAT                   = 77;
pub const SPUDGPU_FORMAT_BC3_UNORM_SRGB: SPUDGPU_FORMAT              = 78;
pub const SPUDGPU_FORMAT_BC4_TYPELESS: SPUDGPU_FORMAT                = 79;
pub const SPUDGPU_FORMAT_BC4_UNORM: SPUDGPU_FORMAT                   = 80;
pub const SPUDGPU_FORMAT_BC4_SNORM: SPUDGPU_FORMAT                   = 81;
pub const SPUDGPU_FORMAT_BC5_TYPELESS: SPUDGPU_FORMAT                = 82;
pub const SPUDGPU_FORMAT_BC5_UNORM: SPUDGPU_FORMAT                   = 83;
pub const SPUDGPU_FORMAT_BC5_SNORM: SPUDGPU_FORMAT                   = 84;
pub const SPUDGPU_FORMAT_B5G6R5_UNORM: SPUDGPU_FORMAT                = 85;
pub const SPUDGPU_FORMAT_B5G5R5A1_UNORM: SPUDGPU_FORMAT              = 86;
pub const SPUDGPU_FORMAT_B8G8R8A8_UNORM: SPUDGPU_FORMAT              = 87;
pub const SPUDGPU_FORMAT_B8G8R8X8_UNORM: SPUDGPU_FORMAT              = 88;
pub const SPUDGPU_FORMAT_R10G10B10_XR_BIAS_A2_UNORM: SPUDGPU_FORMAT  = 89;
pub const SPUDGPU_FORMAT_B8G8R8A8_TYPELESS: SPUDGPU_FORMAT           = 90;
pub const SPUDGPU_FORMAT_B8G8R8A8_UNORM_SRGB: SPUDGPU_FORMAT         = 91;
pub const SPUDGPU_FORMAT_B8G8R8X8_TYPELESS: SPUDGPU_FORMAT           = 92;
pub const SPUDGPU_FORMAT_B8G8R8X8_UNORM_SRGB: SPUDGPU_FORMAT         = 93;
pub const SPUDGPU_FORMAT_BC6H_TYPELESS: SPUDGPU_FORMAT               = 94;
pub const SPUDGPU_FORMAT_BC6H_UF16: SPUDGPU_FORMAT                   = 95;
pub const SPUDGPU_FORMAT_BC6H_SF16: SPUDGPU_FORMAT                   = 96;
pub const SPUDGPU_FORMAT_BC7_TYPELESS: SPUDGPU_FORMAT                = 97;
pub const SPUDGPU_FORMAT_BC7_UNORM: SPUDGPU_FORMAT                   = 98;
pub const SPUDGPU_FORMAT_BC7_UNORM_SRGB: SPUDGPU_FORMAT              = 99;
pub const SPUDGPU_FORMAT_AYUV: SPUDGPU_FORMAT                        = 100;
pub const SPUDGPU_FORMAT_Y410: SPUDGPU_FORMAT                        = 101;
pub const SPUDGPU_FORMAT_Y416: SPUDGPU_FORMAT                        = 102;
pub const SPUDGPU_FORMAT_NV12: SPUDGPU_FORMAT                        = 103;
pub const SPUDGPU_FORMAT_P010: SPUDGPU_FORMAT                        = 104;
pub const SPUDGPU_FORMAT_P016: SPUDGPU_FORMAT                        = 105;
pub const SPUDGPU_FORMAT_420_OPAQUE: SPUDGPU_FORMAT                  = 106;
pub const SPUDGPU_FORMAT_YUY2: SPUDGPU_FORMAT                        = 107;
pub const SPUDGPU_FORMAT_Y210: SPUDGPU_FORMAT                        = 108;
pub const SPUDGPU_FORMAT_Y216: SPUDGPU_FORMAT                        = 109;
pub const SPUDGPU_FORMAT_NV11: SPUDGPU_FORMAT                        = 110;
pub const SPUDGPU_FORMAT_AI44: SPUDGPU_FORMAT                        = 111;
pub const SPUDGPU_FORMAT_IA44: SPUDGPU_FORMAT                        = 112;
pub const SPUDGPU_FORMAT_P8: SPUDGPU_FORMAT                          = 113;
pub const SPUDGPU_FORMAT_A8P8: SPUDGPU_FORMAT                        = 114;
pub const SPUDGPU_FORMAT_B4G4R4A4_UNORM: SPUDGPU_FORMAT              = 115;
pub const SPUDGPU_FORMAT_P208: SPUDGPU_FORMAT                        = 130;
pub const SPUDGPU_FORMAT_V208: SPUDGPU_FORMAT                        = 131;
pub const SPUDGPU_FORMAT_V408: SPUDGPU_FORMAT                        = 132;

// ----------------------------------------------------------------
// SPUDGPU_NATIVE_API
// ----------------------------------------------------------------

pub type SPUDGPU_NATIVE_API = u32;
pub const SPUDGPU_NATIVE_API_NONE: SPUDGPU_NATIVE_API   = 0;
pub const SPUDGPU_NATIVE_API_VULKAN: SPUDGPU_NATIVE_API = 1;
pub const SPUDGPU_NATIVE_API_METAL: SPUDGPU_NATIVE_API  = 2;

// ----------------------------------------------------------------
// SPUDGPU_MEMORY_KIND  (one value, not a bitmask; buffers only)
// ----------------------------------------------------------------

pub type SPUDGPU_MEMORY_KIND = u32;
pub const SPUDGPU_MEMORY_KIND_NONE: SPUDGPU_MEMORY_KIND            = 0;
pub const SPUDGPU_MEMORY_KIND_DEVICE: SPUDGPU_MEMORY_KIND          = 1;
pub const SPUDGPU_MEMORY_KIND_UPLOAD: SPUDGPU_MEMORY_KIND          = 2;
pub const SPUDGPU_MEMORY_KIND_READBACK: SPUDGPU_MEMORY_KIND        = 3;
pub const SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE: SPUDGPU_MEMORY_KIND = 4;

// ----------------------------------------------------------------
// SPUDGPU_BUFFER_USAGE
// ----------------------------------------------------------------

pub type SPUDGPU_BUFFER_USAGE = u32;
pub const SPUDGPU_BUFFER_USAGE_NONE: SPUDGPU_BUFFER_USAGE    = 0;
pub const SPUDGPU_BUFFER_USAGE_VERTEX: SPUDGPU_BUFFER_USAGE  = 1 << 0;
pub const SPUDGPU_BUFFER_USAGE_INDEX: SPUDGPU_BUFFER_USAGE   = 1 << 1;
pub const SPUDGPU_BUFFER_USAGE_UNIFORM: SPUDGPU_BUFFER_USAGE = 1 << 2;
pub const SPUDGPU_BUFFER_USAGE_STORAGE: SPUDGPU_BUFFER_USAGE = 1 << 3;

// ----------------------------------------------------------------
// SPUDGPU_IMAGE_TYPE
// ----------------------------------------------------------------

pub type SPUDGPU_IMAGE_TYPE = u32;
pub const SPUDGPU_IMAGE_TYPE_NONE: SPUDGPU_IMAGE_TYPE = 0;
pub const SPUDGPU_IMAGE_TYPE_1D: SPUDGPU_IMAGE_TYPE   = 1 << 0;
pub const SPUDGPU_IMAGE_TYPE_2D: SPUDGPU_IMAGE_TYPE   = 1 << 1;
pub const SPUDGPU_IMAGE_TYPE_3D: SPUDGPU_IMAGE_TYPE   = 1 << 2;

// ----------------------------------------------------------------
// SPUDGPU_IMAGE_USAGE
// ----------------------------------------------------------------

pub type SPUDGPU_IMAGE_USAGE = u32;
pub const SPUDGPU_IMAGE_USAGE_NONE: SPUDGPU_IMAGE_USAGE                     = 0;
pub const SPUDGPU_IMAGE_USAGE_SAMPLED: SPUDGPU_IMAGE_USAGE                  = 1 << 0;
pub const SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT: SPUDGPU_IMAGE_USAGE         = 1 << 1;
pub const SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT: SPUDGPU_IMAGE_USAGE = 1 << 2;
pub const SPUDGPU_IMAGE_USAGE_STORAGE: SPUDGPU_IMAGE_USAGE                  = 1 << 3;
pub const SPUDGPU_IMAGE_USAGE_TRANSFER_SRC: SPUDGPU_IMAGE_USAGE             = 1 << 4;
pub const SPUDGPU_IMAGE_USAGE_TRANSFER_DST: SPUDGPU_IMAGE_USAGE             = 1 << 5;

// ----------------------------------------------------------------
// SPUDGPU_IMAGE_VIEW_TYPE
// ----------------------------------------------------------------

pub type SPUDGPU_IMAGE_VIEW_TYPE = u32;
pub const SPUDGPU_IMAGE_VIEW_TYPE_NONE: SPUDGPU_IMAGE_VIEW_TYPE        = 0;
pub const SPUDGPU_IMAGE_VIEW_TYPE_1D: SPUDGPU_IMAGE_VIEW_TYPE          = 1 << 0;
pub const SPUDGPU_IMAGE_VIEW_TYPE_2D: SPUDGPU_IMAGE_VIEW_TYPE          = 1 << 1;
pub const SPUDGPU_IMAGE_VIEW_TYPE_3D: SPUDGPU_IMAGE_VIEW_TYPE          = 1 << 2;
pub const SPUDGPU_IMAGE_VIEW_TYPE_CUBE: SPUDGPU_IMAGE_VIEW_TYPE        = 1 << 3;
pub const SPUDGPU_IMAGE_VIEW_TYPE_1D_ARRAY: SPUDGPU_IMAGE_VIEW_TYPE    = 1 << 4;
pub const SPUDGPU_IMAGE_VIEW_TYPE_2D_ARRAY: SPUDGPU_IMAGE_VIEW_TYPE    = 1 << 5;
pub const SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY: SPUDGPU_IMAGE_VIEW_TYPE  = 1 << 6;

// ----------------------------------------------------------------
// SPUDGPU_PRESENT_MODE
// ----------------------------------------------------------------

pub type SPUDGPU_PRESENT_MODE = u32;
pub const SPUDGPU_PRESENT_MODE_IMMEDIATE: SPUDGPU_PRESENT_MODE = 0;
pub const SPUDGPU_PRESENT_MODE_FIFO: SPUDGPU_PRESENT_MODE      = 1;
pub const SPUDGPU_PRESENT_MODE_MAILBOX: SPUDGPU_PRESENT_MODE   = 2;

// ----------------------------------------------------------------
// Descriptor structs  (must exactly match the C layout)
// ----------------------------------------------------------------

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct spudgpu_buffer_desc {
    pub usage: u32,
    pub memory_kind: u32,
    pub size: u64,
    pub debug_name: *const c_char,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct spudgpu_buffer_view_desc {
    pub parent_buffer: spudgpu_buffer,
    pub offset_from_parent_buffer: u64,
    pub stride: u64,
    pub size: u64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct spudgpu_image_desc {
    pub usage: u32,
    pub type_: u32,          // 'type' is a keyword in Rust
    pub format: SPUDGPU_FORMAT,
    pub width: u32,
    pub height: u32,
    pub depth: u32,
    pub array_layers: u32,
    pub mip_levels: u32,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct spudgpu_image_view_desc_subresource_range {
    pub aspect_mask: u64,
    pub base_mip_level: u64,
    pub mip_level_count: u64,
    pub base_array_layer: u64,
    pub array_layer_count: u64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct spudgpu_image_view_desc {
    pub parent_image: spudgpu_image,
    pub type_: u32,          // 'type' is a keyword in Rust
    pub subresource_range: spudgpu_image_view_desc_subresource_range,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct SPUDGPU_VIEWPORT {
    pub x: c_float,
    pub y: c_float,
    pub width: c_float,
    pub height: c_float,
    pub min_depth: c_float,
    pub max_depth: c_float,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct SPUDGPU_SCISSOR_RECT {
    pub x: c_float,
    pub y: c_float,
    pub width: c_float,
    pub height: c_float,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct spudgpu_swap_chain_desc {
    pub window_handle: *mut c_void,
    pub width: u32,
    pub height: u32,
    pub buffer_count: u32,
    pub format: SPUDGPU_FORMAT,
    pub present_mode: SPUDGPU_PRESENT_MODE,
    pub fullscreen: bool,
}

// ----------------------------------------------------------------
// Extern "C" function declarations — one per public spudgpu.h symbol
// ----------------------------------------------------------------

extern "C" {
    // Instance
    pub fn spudgpu_init(
        native_api: SPUDGPU_NATIVE_API,
        application_name: *const c_char,
        application_version: u32,
        engine_name: *const c_char,
        engine_version: u32,
    ) -> spudgpu_instance;

    pub fn spudgpu_terminate(instance: spudgpu_instance);

    pub fn spudgpu_enumerate_devices(
        instance: spudgpu_instance,
        pp_output_devices: *mut *mut spudgpu_device,
        p_output_device_count: *mut u32,
    ) -> bool;

    pub fn spudgpu_get_native_gpu_api(instance: spudgpu_instance) -> SPUDGPU_NATIVE_API;

    // Buffer
    pub fn spudgpu_create_buffer(
        device: spudgpu_device,
        desc: *const spudgpu_buffer_desc,
    ) -> spudgpu_buffer;

    pub fn spudgpu_get_buffer_desc(buffer: spudgpu_buffer) -> spudgpu_buffer_desc;

    pub fn spudgpu_get_buffer_gpu_address(buffer: spudgpu_buffer, out_address: *mut u64) -> u32;

    pub fn spudgpu_destroy_buffer(device: spudgpu_device, buffer: spudgpu_buffer);

    pub fn spudgpu_create_buffer_view(
        buffer: spudgpu_buffer,
        desc: *const spudgpu_buffer_view_desc,
    ) -> spudgpu_buffer_view;

    pub fn spudgpu_get_buffer_view_desc(view: spudgpu_buffer_view) -> spudgpu_buffer_view_desc;

    pub fn spudgpu_destroy_buffer_view(buffer: spudgpu_buffer_view);

    // Image
    pub fn spudgpu_create_image(
        device: spudgpu_device,
        desc: *const spudgpu_image_desc,
    ) -> spudgpu_image;

    pub fn spudgpu_get_image_desc(image: spudgpu_image) -> spudgpu_image_desc;

    pub fn spudgpu_destroy_image(device: spudgpu_device, image: spudgpu_image);

    pub fn spudgpu_create_image_view(
        image: spudgpu_image,
        desc: *const spudgpu_image_view_desc,
    ) -> spudgpu_image_view;

    pub fn spudgpu_destroy_image_view(device: spudgpu_device, image_view: spudgpu_image_view);

    pub fn spudgpu_get_image_view_desc(image_view: spudgpu_image_view) -> spudgpu_image_view_desc;

    // Command list
    pub fn spudgpu_begin_command_list(cmd: spudgpu_command_list);
    pub fn spudgpu_end_command_list(cmd: spudgpu_command_list);

    pub fn spudgpu_set_viewports(
        cmd: spudgpu_command_list,
        first_viewport: u32,
        viewport_count: u32,
        viewports: *const SPUDGPU_VIEWPORT,
    );

    pub fn spudgpu_set_scissor_rects(
        cmd: spudgpu_command_list,
        first_scissor_rect: u32,
        scissor_rect_count: u32,
        scissor_rects: *const SPUDGPU_SCISSOR_RECT,
    );

    pub fn spudgpu_set_vertex_buffers(
        cmd: spudgpu_command_list,
        start_slot: u32,
        view_count: u32,
        buffer_views: *mut spudgpu_buffer_view,
    );

    pub fn spudgpu_set_index_buffers(
        cmd: spudgpu_command_list,
        view_count: u32,
        buffer_views: *mut spudgpu_buffer_view,
    );

    pub fn spudgpu_draw(
        cmd: spudgpu_command_list,
        vertex_count: u32,
        start_vertex_location: u32,
    );

    pub fn spudgpu_draw_indexed(
        cmd: spudgpu_command_list,
        index_count: u32,
        start_index_location: u32,
        base_vertex_location: u32,
    );

    pub fn spudgpu_draw_instanced(
        cmd: spudgpu_command_list,
        vertex_count_per_instance: u32,
        instance_count: u32,
        start_vertex_location: u32,
        start_instance_location: u32,
    );

    pub fn spudgpu_draw_indexed_instanced(
        cmd: spudgpu_command_list,
        index_count_per_instance: u32,
        instance_count: u32,
        start_index_location: u32,
        base_vertex_location: u32,
        start_instance_location: u32,
    );

    // Swap chain
    pub fn spudgpu_create_swap_chain(
        device: spudgpu_device,
        desc: *const spudgpu_swap_chain_desc,
    ) -> spudgpu_swap_chain;

    pub fn spudgpu_destroy_swap_chain(swap_chain: spudgpu_swap_chain);

    pub fn spudgpu_get_swap_chain_desc(swap_chain: spudgpu_swap_chain) -> spudgpu_swap_chain_desc;

    pub fn spudgpu_swap_chain_acquire_next_image(swap_chain: spudgpu_swap_chain) -> u32;

    pub fn spudgpu_swap_chain_present(swap_chain: spudgpu_swap_chain);
}