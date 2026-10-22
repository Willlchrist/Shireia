#!/usr/bin/env python3
"""Exercise the production Vulkan layer's resource lifetimes without a GPU.

Compile its actual methods against an instrumented resource/timeline fixture.
Two negative controls verify that both missing waits and early destruction fail.
Usage: python run_present_lifetime.py --cxx C:/msys64/ucrt64/bin/g++.exe
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def method(source, name):
    start = source.index("void Layer::" + name + "(")
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="c++")
    args = parser.parse_args()
    source = (ROOT / "src/video_core/renderer_vulkan/present/layer.cpp").read_text(encoding="utf-8")
    methods = method(source, "SetAntiAliasPass") + "\n" + method(source, "RefreshResources")
    missing_wait = methods.replace("scheduler.Wait(tick);", "(void)tick;")
    early_destroy = methods.replace(
        "    ReleaseRawImages();\n    // The old AA images may still be referenced by earlier frames.\n"
        "    anti_alias.emplace<std::monostate>();",
        "    anti_alias.emplace<std::monostate>();\n    ReleaseRawImages();",
    )
    if missing_wait == methods or early_destroy == methods:
        raise RuntimeError("Negative controls no longer match the production methods")
    env = dict(os.environ)
    env["PATH"] = str(Path(args.cxx).resolve().parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="shireia-present-lifetime-") as tmp:
        folder = Path(tmp)
        for label, text, expected_error in (
            ("missing-wait", missing_wait, "AA switched before"),
            ("early-destroy", early_destroy, "Framebuffer resized before"),
            ("production", methods, None),
        ):
            (folder / "present_methods.inc").write_text(text, encoding="utf-8")
            executable = folder / (label + (".exe" if os.name == "nt" else ""))
            subprocess.run([args.cxx, "-std=c++20", "-I", str(folder),
                            str(ROOT / "tests/graphics_settings/present_lifetime_unit.cpp"),
                            "-o", str(executable)], check=True, env=env)
            result = subprocess.run([str(executable)], capture_output=True, text=True, env=env)
            if expected_error:
                if result.returncode == 0 or expected_error not in result.stderr:
                    raise RuntimeError(f"Negative control {label} did not fail as expected: {result.stderr}")
                print(f"PASS: {label} negative control detected unsafe destruction")
            elif result.returncode:
                raise RuntimeError(result.stdout + result.stderr)
            else:
                print(result.stdout.strip())


if __name__ == "__main__":
    main()
