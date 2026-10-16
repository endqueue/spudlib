//! Safe, idiomatic Rust bindings to the SpudGPU C library.
//!
//! # Quick start
//! ```no_run
//! use spudgpu::{Instance, NativeApi, Buffer, BufferDesc, BufferUsage, MemoryKind};
//!
//! let instance = Instance::new(NativeApi::Vulkan, "My App", 1, "My Engine", 1)
//!     .expect("Failed to initialize SpudGPU");
//!
//! let devices = instance.enumerate_devices()
//!     .expect("No GPU devices found");
//!
//! let device = &devices[0];
//!
//! let buffer = Buffer::new(device, &BufferDesc {
//!     usage: BufferUsage::VERTEX | BufferUsage::INDEX,
//!     memory_kind: MemoryKind::DEVICE,
//!     size: 1024,
//! }).expect("Failed to create buffer");
//! ```

use spudlib_sys as sys;
use std::ffi::CString;

// ----------------------------------------------------------------
// Error type
// ----------------------------------------------------------------

#[derive(Debug)]
pub enum SpudGpuError {
    InitFailed,
    NoDevicesFound,
    InvalidArgument(&'static str),
    AllocationFailed(&'static str),
}

impl std::fmt::Display for SpudGpuError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InitFailed              => write!(f, "SpudGPU initialization failed"),
            Self::NoDevicesFound          => write!(f, "No GPU devices found"),
            Self::InvalidArgument(msg)    => write!(f, "Invalid argument: {msg}"),
            Self::AllocationFailed(msg)   => write!(f, "Allocation failed: {msg}"),
        }
    }
}

impl std::error::Error for SpudGpuError {}

pub type Result<T> = std::result::Result<T, SpudGpuError>;

// ----------------------------------------------------------------
// NativeApi
// ----------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NativeApi {
    Vulkan,
    Metal,
}

impl NativeApi {
    fn to_raw(self) -> sys::SPUDGPU_NATIVE_API {
        match self {
            NativeApi::Vulkan => sys::SPUDGPU_NATIVE_API_VULKAN,
            NativeApi::Metal  => sys::SPUDGPU_NATIVE_API_METAL,
        }
    }
}

// ----------------------------------------------------------------
// Format
// ----------------------------------------------------------------

/// Pixel/texel format, mirrors SPUDGPU_FORMAT_* constants.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u32)]
pub enum Format {
    Unknown                 = sys::SPUDGPU_FORMAT_UNKNOWN,
    R32G32B32A32Float       = sys::SPUDGPU_FORMAT_R32G32B32A32_FLOAT,
    R32G32B32A32Uint        = sys::SPUDGPU_FORMAT_R32G32B32A32_UINT,
    R32G32B32A32Sint        = sys::SPUDGPU_FORMAT_R32G32B32A32_SINT,
    R16G16B16A16Float       = sys::SPUDGPU_FORMAT_R16G16B16A16_FLOAT,
    R16G16B16A16Unorm       = sys::SPUDGPU_FORMAT_R16G16B16A16_UNORM,
    R8G8B8A8Unorm           = sys::SPUDGPU_FORMAT_R8G8B8A8_UNORM,
    R8G8B8A8UnormSrgb       = sys::SPUDGPU_FORMAT_R8G8B8A8_UNORM_SRGB,
    B8G8R8A8Unorm           = sys::SPUDGPU_FORMAT_B8G8R8A8_UNORM,
    B8G8R8A8UnormSrgb       = sys::SPUDGPU_FORMAT_B8G8R8A8_UNORM_SRGB,
    D32Float                = sys::SPUDGPU_FORMAT_D32_FLOAT,
    D24UnormS8Uint          = sys::SPUDGPU_FORMAT_D24_UNORM_S8_UINT,
    R32Float                = sys::SPUDGPU_FORMAT_R32_FLOAT,
    R32Uint                 = sys::SPUDGPU_FORMAT_R32_UINT,
    R16Uint                 = sys::SPUDGPU_FORMAT_R16_UINT,
    R8Unorm                 = sys::SPUDGPU_FORMAT_R8_UNORM,
    Bc1Unorm                = sys::SPUDGPU_FORMAT_BC1_UNORM,
    Bc1UnormSrgb            = sys::SPUDGPU_FORMAT_BC1_UNORM_SRGB,
    Bc3Unorm                = sys::SPUDGPU_FORMAT_BC3_UNORM,
    Bc3UnormSrgb            = sys::SPUDGPU_FORMAT_BC3_UNORM_SRGB,
    Bc5Unorm                = sys::SPUDGPU_FORMAT_BC5_UNORM,
    Bc7Unorm                = sys::SPUDGPU_FORMAT_BC7_UNORM,
    Bc7UnormSrgb            = sys::SPUDGPU_FORMAT_BC7_UNORM_SRGB,
}

impl Format {
    fn to_raw(self) -> sys::SPUDGPU_FORMAT {
        self as u32
    }
}

// ----------------------------------------------------------------
// MemoryKind  (one kind per buffer: these do not combine)
// ----------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MemoryKind(u32);

impl MemoryKind {
    pub const DEVICE:          Self = Self(sys::SPUDGPU_MEMORY_KIND_DEVICE);
    pub const UPLOAD:          Self = Self(sys::SPUDGPU_MEMORY_KIND_UPLOAD);
    pub const READBACK:        Self = Self(sys::SPUDGPU_MEMORY_KIND_READBACK);
    pub const DEVICE_MAPPABLE: Self = Self(sys::SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE);
}

// ----------------------------------------------------------------
// BufferUsage  (bitflags-style struct)
// ----------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct BufferUsage(u32);

impl BufferUsage {
    pub const NONE:    Self = Self(sys::SPUDGPU_BUFFER_USAGE_NONE);
    pub const VERTEX:  Self = Self(sys::SPUDGPU_BUFFER_USAGE_VERTEX);
    pub const INDEX:   Self = Self(sys::SPUDGPU_BUFFER_USAGE_INDEX);
    pub const UNIFORM: Self = Self(sys::SPUDGPU_BUFFER_USAGE_UNIFORM);
    pub const STORAGE: Self = Self(sys::SPUDGPU_BUFFER_USAGE_STORAGE);
}

impl std::ops::BitOr for BufferUsage {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self { Self(self.0 | rhs.0) }
}

// ----------------------------------------------------------------
// ImageUsage  (bitflags-style struct)
// ----------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ImageUsage(u32);

impl ImageUsage {
    pub const NONE:                     Self = Self(sys::SPUDGPU_IMAGE_USAGE_NONE);
    pub const SAMPLED:                  Self = Self(sys::SPUDGPU_IMAGE_USAGE_SAMPLED);
    pub const COLOR_ATTACHMENT:         Self = Self(sys::SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT);
    pub const DEPTH_STENCIL_ATTACHMENT: Self = Self(sys::SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT);
    pub const STORAGE:                  Self = Self(sys::SPUDGPU_IMAGE_USAGE_STORAGE);
    pub const TRANSFER_SRC:             Self = Self(sys::SPUDGPU_IMAGE_USAGE_TRANSFER_SRC);
    pub const TRANSFER_DST:             Self = Self(sys::SPUDGPU_IMAGE_USAGE_TRANSFER_DST);
}

impl std::ops::BitOr for ImageUsage {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self { Self(self.0 | rhs.0) }
}

// ----------------------------------------------------------------
// PresentMode
// ----------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PresentMode {
    Immediate,
    Fifo,
    Mailbox,
}

impl PresentMode {
    fn to_raw(self) -> sys::SPUDGPU_PRESENT_MODE {
        match self {
            PresentMode::Immediate => sys::SPUDGPU_PRESENT_MODE_IMMEDIATE,
            PresentMode::Fifo      => sys::SPUDGPU_PRESENT_MODE_FIFO,
            PresentMode::Mailbox   => sys::SPUDGPU_PRESENT_MODE_MAILBOX,
        }
    }
}

// ----------------------------------------------------------------
// Instance
// ----------------------------------------------------------------

/// The root SpudGPU context. Owns the underlying native API instance.
/// Destroyed automatically when dropped.
pub struct Instance(sys::spudgpu_instance);

impl Instance {
    pub fn new(
        api: NativeApi,
        app_name: &str,
        app_version: u32,
        engine_name: &str,
        engine_version: u32,
    ) -> Result<Self> {
        let app_name_c    = CString::new(app_name).map_err(|_| SpudGpuError::InvalidArgument("app_name contains null byte"))?;
        let engine_name_c = CString::new(engine_name).map_err(|_| SpudGpuError::InvalidArgument("engine_name contains null byte"))?;

        let raw = unsafe {
            sys::spudgpu_init(
                api.to_raw(),
                app_name_c.as_ptr(),
                app_version,
                engine_name_c.as_ptr(),
                engine_version,
            )
        };

        if raw.is_null() {
            Err(SpudGpuError::InitFailed)
        } else {
            Ok(Self(raw))
        }
    }

    pub fn enumerate_devices(&self) -> Result<Vec<Device>> {
        let mut raw_devices: *mut sys::spudgpu_device = std::ptr::null_mut();
        let mut device_count: u32 = 0;

        let ok = unsafe {
            sys::spudgpu_enumerate_devices(
                self.0,
                &mut raw_devices,
                &mut device_count,
            )
        };

        if !ok || device_count == 0 || raw_devices.is_null() {
            return Err(SpudGpuError::NoDevicesFound);
        }

        // Wrap each raw pointer as a Device.
        // The array belongs to the instance, which frees it, so the
        // handles are copied out of it and it is never freed here.
        let devices = (0..device_count as usize)
            .map(|i| Device(unsafe { *raw_devices.add(i) }))
            .collect();

        Ok(devices)
    }

    pub fn native_api(&self) -> sys::SPUDGPU_NATIVE_API {
        unsafe { sys::spudgpu_get_native_gpu_api(self.0) }
    }
}

impl Drop for Instance {
    fn drop(&mut self) {
        unsafe { sys::spudgpu_terminate(self.0) }
    }
}

// Instance holds a raw pointer; we assert it is safe to send across threads.
// SpudGPU instances are not shared — only one thread should own one at a time.
unsafe impl Send for Instance {}

// ----------------------------------------------------------------
// Device
// ----------------------------------------------------------------

/// A physical GPU device. Borrowed from the array returned by
/// `Instance::enumerate_devices` — does not own or free the pointer.
pub struct Device(sys::spudgpu_device);

unsafe impl Send for Device {}
unsafe impl Sync for Device {}

// ----------------------------------------------------------------
// BufferDesc  (Rust-friendly version of spudgpu_buffer_desc)
// ----------------------------------------------------------------

pub struct BufferDesc {
    pub usage: BufferUsage,
    pub memory_kind: MemoryKind,
    pub size: u64,
}

// ----------------------------------------------------------------
// Buffer
// ----------------------------------------------------------------

/// An allocated GPU buffer. Destroyed automatically when dropped.
pub struct Buffer {
    raw: sys::spudgpu_buffer,
    // Keep the device pointer alive for cleanup
    device: sys::spudgpu_device,
}

impl Buffer {
    pub fn new(device: &Device, desc: &BufferDesc) -> Result<Self> {
        if desc.size == 0 {
            return Err(SpudGpuError::InvalidArgument("buffer size must be > 0"));
        }

        let raw_desc = sys::spudgpu_buffer_desc {
            usage: desc.usage.0,
            memory_kind: desc.memory_kind.0,
            gpu_address_location: 0,
            size: desc.size,
        };

        let raw = unsafe { sys::spudgpu_create_buffer(device.0, &raw_desc) };

        if raw.is_null() {
            Err(SpudGpuError::AllocationFailed("spudgpu_create_buffer returned null"))
        } else {
            Ok(Self { raw, device: device.0 })
        }
    }

    /// Returns the GPU virtual address assigned after creation.
    pub fn gpu_address(&self) -> u64 {
        unsafe { sys::spudgpu_get_buffer_desc(self.raw) }.gpu_address_location
    }

    /// Returns the allocated size in bytes.
    pub fn size(&self) -> u64 {
        unsafe { sys::spudgpu_get_buffer_desc(self.raw) }.size
    }

    pub fn raw(&self) -> sys::spudgpu_buffer {
        self.raw
    }
}

impl Drop for Buffer {
    fn drop(&mut self) {
        unsafe { sys::spudgpu_destroy_buffer(self.device, self.raw) }
    }
}

unsafe impl Send for Buffer {}

// ----------------------------------------------------------------
// ImageDesc  (Rust-friendly version of spudgpu_image_desc)
// ----------------------------------------------------------------

pub struct ImageDesc {
    pub usage: ImageUsage,
    pub format: Format,
    pub width: u32,
    pub height: u32,
    /// Depth of a 3D image; set to 1 for 2D images.
    pub depth: u32,
    pub array_layers: u32,
    pub mip_levels: u32,
}

// ----------------------------------------------------------------
// Image
// ----------------------------------------------------------------

/// An allocated GPU image/texture. Destroyed automatically when dropped.
pub struct Image {
    raw: sys::spudgpu_image,
    device: sys::spudgpu_device,
}

impl Image {
    pub fn new(device: &Device, desc: &ImageDesc) -> Result<Self> {
        let raw_desc = sys::spudgpu_image_desc {
            usage: desc.usage.0,
            type_: 0,   // derived from usage on the C side currently
            gpu_address_location: 0,
            format: desc.format.to_raw(),
            width: desc.width,
            height: desc.height,
            depth: desc.depth,
            array_layers: desc.array_layers,
            mip_levels: desc.mip_levels,
        };

        let raw = unsafe { sys::spudgpu_create_image(device.0, &raw_desc) };

        if raw.is_null() {
            Err(SpudGpuError::AllocationFailed("spudgpu_create_image returned null"))
        } else {
            Ok(Self { raw, device: device.0 })
        }
    }

    pub fn raw(&self) -> sys::spudgpu_image {
        self.raw
    }
}

impl Drop for Image {
    fn drop(&mut self) {
        unsafe { sys::spudgpu_destroy_image(self.device, self.raw) }
    }
}

unsafe impl Send for Image {}

// ----------------------------------------------------------------
// ImageViewDesc & ImageView
// ----------------------------------------------------------------

pub struct SubresourceRange {
    pub aspect_mask: u64,
    pub base_mip_level: u64,
    pub mip_level_count: u64,
    pub base_array_layer: u64,
    pub array_layer_count: u64,
}

pub struct ImageViewDesc {
    pub view_type: u32,
    pub subresource_range: SubresourceRange,
}

/// A view into a GPU image. Destroyed automatically when dropped.
pub struct ImageView {
    raw: sys::spudgpu_image_view,
    device: sys::spudgpu_device,
}

impl ImageView {
    pub fn new(image: &Image, device: &Device, desc: &ImageViewDesc) -> Result<Self> {
        let raw_desc = sys::spudgpu_image_view_desc {
            parent_image: image.raw,
            type_: desc.view_type,
            subresource_range: sys::spudgpu_image_view_desc_subresource_range {
                aspect_mask:       desc.subresource_range.aspect_mask,
                base_mip_level:    desc.subresource_range.base_mip_level,
                mip_level_count:   desc.subresource_range.mip_level_count,
                base_array_layer:  desc.subresource_range.base_array_layer,
                array_layer_count: desc.subresource_range.array_layer_count,
            },
        };

        let raw = unsafe { sys::spudgpu_create_image_view(image.raw, &raw_desc) };

        if raw.is_null() {
            Err(SpudGpuError::AllocationFailed("spudgpu_create_image_view returned null"))
        } else {
            Ok(Self { raw, device: device.0 })
        }
    }

    pub fn raw(&self) -> sys::spudgpu_image_view {
        self.raw
    }
}

impl Drop for ImageView {
    fn drop(&mut self) {
        unsafe { sys::spudgpu_destroy_image_view(self.device, self.raw) }
    }
}

unsafe impl Send for ImageView {}

// ----------------------------------------------------------------
// Viewport & ScissorRect  (plain data, no lifetime needed)
// ----------------------------------------------------------------

#[derive(Debug, Clone, Copy)]
pub struct Viewport {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
    pub min_depth: f32,
    pub max_depth: f32,
}

impl Viewport {
    fn to_raw(self) -> sys::SPUDGPU_VIEWPORT {
        sys::SPUDGPU_VIEWPORT {
            x: self.x,
            y: self.y,
            width: self.width,
            height: self.height,
            min_depth: self.min_depth,
            max_depth: self.max_depth,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct ScissorRect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

impl ScissorRect {
    fn to_raw(self) -> sys::SPUDGPU_SCISSOR_RECT {
        sys::SPUDGPU_SCISSOR_RECT {
            x: self.x,
            y: self.y,
            width: self.width,
            height: self.height,
        }
    }
}

// ----------------------------------------------------------------
// SwapChainDesc & SwapChain
// ----------------------------------------------------------------

pub struct SwapChainDesc {
    /// Raw OS window handle (HWND, NSWindow*, wl_surface*, etc.)
    pub window_handle: *mut std::ffi::c_void,
    pub width: u32,
    pub height: u32,
    pub buffer_count: u32,
    pub format: Format,
    pub present_mode: PresentMode,
    pub fullscreen: bool,
}

pub struct SwapChain(sys::spudgpu_swap_chain);

impl SwapChain {
    pub fn new(device: &Device, desc: &SwapChainDesc) -> Result<Self> {
        let raw_desc = sys::spudgpu_swap_chain_desc {
            window_handle: desc.window_handle,
            width: desc.width,
            height: desc.height,
            buffer_count: desc.buffer_count,
            format: desc.format.to_raw(),
            present_mode: desc.present_mode.to_raw(),
            fullscreen: desc.fullscreen,
        };

        let raw = unsafe { sys::spudgpu_create_swap_chain(device.0, &raw_desc) };

        if raw.is_null() {
            Err(SpudGpuError::AllocationFailed("spudgpu_create_swap_chain returned null"))
        } else {
            Ok(Self(raw))
        }
    }

    pub fn acquire_next_image(&self) -> u32 {
        unsafe { sys::spudgpu_swap_chain_acquire_next_image(self.0) }
    }

    pub fn present(&self) {
        unsafe { sys::spudgpu_swap_chain_present(self.0) }
    }
}

impl Drop for SwapChain {
    fn drop(&mut self) {
        unsafe { sys::spudgpu_destroy_swap_chain(self.0) }
    }
}

unsafe impl Send for SwapChain {}

// ----------------------------------------------------------------
// CommandList
// ----------------------------------------------------------------

/// A recording command list. Call `begin` before recording,
/// `end` before submission.
pub struct CommandList(sys::spudgpu_command_list);

impl CommandList {
    /// Wraps a raw command list handle. The caller is responsible
    /// for ensuring the pointer is valid.
    pub unsafe fn from_raw(raw: sys::spudgpu_command_list) -> Self {
        Self(raw)
    }

    pub fn begin(&self) {
        unsafe { sys::spudgpu_begin_command_list(self.0) }
    }

    pub fn end(&self) {
        unsafe { sys::spudgpu_end_command_list(self.0) }
    }

    pub fn set_viewports(&self, first: u32, viewports: &[Viewport]) {
        let raw: Vec<sys::SPUDGPU_VIEWPORT> = viewports.iter().map(|v| v.to_raw()).collect();
        unsafe { sys::spudgpu_set_viewports(self.0, first, raw.len() as u32, raw.as_ptr()) }
    }

    pub fn set_scissor_rects(&self, first: u32, rects: &[ScissorRect]) {
        let raw: Vec<sys::SPUDGPU_SCISSOR_RECT> = rects.iter().map(|r| r.to_raw()).collect();
        unsafe { sys::spudgpu_set_scissor_rects(self.0, first, raw.len() as u32, raw.as_ptr()) }
    }

    pub fn draw(&self, vertex_count: u32, start_vertex: u32) {
        unsafe { sys::spudgpu_draw(self.0, vertex_count, start_vertex) }
    }

    pub fn draw_indexed(&self, index_count: u32, start_index: u32, base_vertex: u32) {
        unsafe { sys::spudgpu_draw_indexed(self.0, index_count, start_index, base_vertex) }
    }

    pub fn draw_instanced(
        &self,
        vertex_count_per_instance: u32,
        instance_count: u32,
        start_vertex: u32,
        start_instance: u32,
    ) {
        unsafe {
            sys::spudgpu_draw_instanced(
                self.0,
                vertex_count_per_instance,
                instance_count,
                start_vertex,
                start_instance,
            )
        }
    }

    pub fn draw_indexed_instanced(
        &self,
        index_count_per_instance: u32,
        instance_count: u32,
        start_index: u32,
        base_vertex: u32,
        start_instance: u32,
    ) {
        unsafe {
        sys::spudgpu_draw_indexed_instanced(
            self.0,
            index_count_per_instance,
            instance_count,
            start_index,
            base_vertex,
            start_instance,
        )
    }
    }
}