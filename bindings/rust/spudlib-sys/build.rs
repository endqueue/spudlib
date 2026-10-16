use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));

    // Walk up from bindings/rust/spudlib-sys/ to the repo root
    // Adjust this if your layout differs
    let root = manifest_dir
        .join("../../..")
        .canonicalize()
        .expect("Could not resolve repo root");

    let vulkan_backends_src = root.join("src/gpu/backends/vulkan");

    println!("cargo:warning=Looking for C sources in: {}", vulkan_backends_src.display());

    // ----------------------------------------------------------------
    // 1. Compile the SpudLib C sources into a static library
    // ----------------------------------------------------------------
    let mut build = cc::Build::new();

    build
        .include(&root)
        .include(root.join("include"))
        .include(&vulkan_backends_src)
        .file(vulkan_backends_src.join("spudgpuvulkancontext.c"))
        .file(vulkan_backends_src.join("spudgpuvulkanswapchain.c"))
        .file(vulkan_backends_src.join("spudgpuvulkanbuffer.c"))
        .file(vulkan_backends_src.join("spudgpuvulkanimage.c"))
        .file(vulkan_backends_src.join("spudgpuvulkanshader.c"))
        // The pipeline checks every backend shares.
        .file(root.join("src/gpu/spudgpushared.c"))
        .flag_if_supported("-std=c23");

    if cfg!(feature = "vulkan") {
        build.define("SPUDGPU_COMPILE_VULKAN_API", "1");
    }

    if cfg!(feature = "metal") {
        build.define("SPUDGPU_COMPILE_METAL_API", "1");
    }

    build.compile("spudlib");

    // ----------------------------------------------------------------
    // 2. Link platform Vulkan library
    // ----------------------------------------------------------------
    #[cfg(feature = "vulkan")]
    {
        println!("cargo:rustc-link-lib=vulkan");
    }

    // ----------------------------------------------------------------
    // 3. Tell Cargo to re-run this script if C sources change
    // ----------------------------------------------------------------
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed={}", root.join("include/spudgpu.h").display());
    println!("cargo:rerun-if-changed={}", root.join("src/gpu/backends/vulkan/spudgpuvulkan.h").display());
    println!("cargo:rerun-if-changed={}", root.join("src/gpu/backends/vulkan/spudgpuvulkancontext.c").display());
    println!("cargo:rerun-if-changed={}", root.join("src/gpu/backends/vulkan/spudgpuvulkanswapchain.c").display());
    println!("cargo:rerun-if-changed={}", root.join("src/gpu/backends/vulkan/spudgpuvulkanbuffer.c").display());
    println!("cargo:rerun-if-changed={}", root.join("src/gpu/backends/vulkan/spudgpuvulkanimage.c").display());
    println!("cargo:rerun-if-changed={}", root.join("src/gpu/backends/vulkan/spudgpuvulkanshader.c").display());
    println!("cargo:rerun-if-changed={}", root.join("src/gpu/spudgpushared.c").display());
}