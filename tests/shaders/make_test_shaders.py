"""Writes tests/spudgpu_test_shaders.h, the SPIR-V of the shaders in this
folder as C arrays, so the tests that draw need no shader compiler to build.

Run from anywhere, with glslc (the Vulkan SDK's) on the PATH or in
VULKAN_SDK, after changing a shader here:

    python tests/shaders/make_test_shaders.py
"""

import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "spudgpu_test_shaders.h")

# (file, C name, extra glslc arguments). The mesh shader needs SPIR-V 1.4 or
# later; 1.5 is what the D3D12 backend has translated before.
SHADERS = [
    ("objects_draw.vert", "spudgpu_test_objects_draw_vert", []),
    ("objects_draw.frag", "spudgpu_test_objects_draw_frag", []),
    ("objects_draw.mesh", "spudgpu_test_objects_draw_mesh", ["--target-env=vulkan1.2"]),
    ("objects_double.comp", "spudgpu_test_objects_double_comp", []),
]


def glslc():
    found = shutil.which("glslc")
    if found:
        return found
    sdk = os.environ.get("VULKAN_SDK")
    for sub in ("Bin", "bin"):
        if sdk:
            path = os.path.join(sdk, sub, "glslc.exe" if os.name == "nt" else "glslc")
            if os.path.exists(path):
                return path
    sys.exit("glslc not found: put it on the PATH or set VULKAN_SDK")


def main():
    compiler = glslc()
    lines = [
        "// Written by tests/shaders/make_test_shaders.py from tests/shaders/: not by hand.",
        "// The SPIR-V of each shader there, as 32-bit words.",
        "",
        "#ifndef SPUDGPU_TEST_SHADERS_H",
        "#define SPUDGPU_TEST_SHADERS_H",
        "",
        "#include <stdint.h>",
        "",
    ]
    with tempfile.TemporaryDirectory() as tmp:
        for source, name, extra in SHADERS:
            spv = os.path.join(tmp, name + ".spv")
            subprocess.run([compiler, *extra, os.path.join(HERE, source), "-o", spv], check=True)
            data = open(spv, "rb").read()
            words = [int.from_bytes(data[i:i + 4], "little") for i in range(0, len(data), 4)]
            lines.append(f"// {source}")
            lines.append(f"static const uint32_t {name}[{len(words)}] = {{")
            for i in range(0, len(words), 8):
                lines.append("\t" + ", ".join(f"0x{w:08x}u" for w in words[i:i + 8]) + ",")
            lines.append("};")
            lines.append("")
    lines.append("#endif")
    with open(OUT, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
