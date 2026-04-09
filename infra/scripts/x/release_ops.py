from __future__ import annotations

import os
import shutil
import stat
import tarfile
import zipfile
from pathlib import Path

from .context import (
    LOCALES_DIR,
    ROOT,
    STDPKG_DIR,
    exe_name,
    host_tag,
    is_windows,
    build_dir,
    read_version,
)
from .llvm import detect_llvm_dir, llvm_bin_dir, resolve_cmake_env
from .runtime_build import (
    build_cjson_for_available_targets,
    build_civetweb_for_available_targets,
    build_kinal_vm_binary,
    build_runtime_for_host,
    copy_cjson_bundle_files,
    copy_civetweb_bundle_files,
    copy_host_linker_tools,
    copy_llvm_dev_files,
    copy_llvm_runtime_files,
    sync_io_json_native_assets,
    sync_io_web_native_assets,
    write_linux_compiler_launcher,
)
from .util import copy_tree, find_artifact, read_json, run, write_text


def _rmtree(path: Path) -> None:
    """shutil.rmtree wrapper that handles read-only files (e.g. .git/objects on Windows)."""
    def _on_error(_func, fpath, _exc_info):
        os.chmod(fpath, stat.S_IWRITE)
        os.unlink(fpath)
    shutil.rmtree(path, onerror=_on_error)


def write_bundle_readme(dest: Path) -> None:
    version = read_version()
    text = "\n".join(
        [
            f"Kinal {version} ({host_tag()})",
            "",
            "Kinal is a systems programming language with its own compiler and virtual machine.",
            "",
            "Quick Start:",
            f"  {exe_name('kinal')} hello.kn",
            "",
            "Tools:",
            f"  - {exe_name('kinal')}             : compiler",
            f"  - {exe_name('kinalvm')}           : bytecode virtual machine",
            f"  - {exe_name('kinal-lsp-server')}  : language server (LSP)",
            "",
            "Contents:",
            "  - compiler and tools",
            "  - standard library (libs/std/)",
            "  - runtime (libs/runtime/)",
            "  - localization assets (infra/assets/locales/)",
            "  - platform-specific runtime libraries",
            "", 
            "Notes:",
            "  - This bundle targets the current host platform only.",
            "  - LLVM tools/runtime are included when required.",
            "  - Zig is discovered locally or bootstrapped on demand.",
            "",
            "Documentation:",
            "  Local: docs/",
            "  Online: https://kinal.org/docs",
            "",
            "Resources:",
            "  Website: https://kinal.org",
            "  GitHub:  https://github.com/Kinal-Lang/Kinal",
        ]
    )
    write_text(dest / "README.txt", text)


def official_stdpkg_manifests() -> list[Path]:
    manifests = sorted(STDPKG_DIR.glob("*/*/package.knpkg.json"))
    return [p for p in manifests if p.is_file()]


def build_official_stdpkg_klibs(compiler: Path, llvm_bin: Path | None = None) -> None:
    manifests = official_stdpkg_manifests()
    if not manifests:
        return
    if llvm_bin is not None:
        build_civetweb_for_available_targets(llvm_bin)
        build_cjson_for_available_targets(llvm_bin)
    sync_io_web_native_assets(STDPKG_DIR)
    sync_io_json_native_assets(STDPKG_DIR)
    for manifest in manifests:
        data = read_json(manifest)
        name = data.get("name", manifest.parent.parent.name)
        lib_dir = manifest.parent / "lib"
        lib_dir.mkdir(parents=True, exist_ok=True)
        out_klib = lib_dir / f"{name}.klib"
        run([compiler, "pkg", "build", "--manifest", manifest, "-o", out_klib], cwd=ROOT)


def _create_kinalvm_stdpkg(compiler: Path, bundle_stdpkg: Path, *, include_sources: bool = True) -> None:
    """Create the IO.Kinal.VM stdpkg in the release bundle.

    This is called AFTER kinalvm has been built, so there is no conflict
    between auto-discovered modules and the stdpkg package.  The source
    of truth is always apps/kinalvm/src/IO/Kinal/VM/.
    """
    import json as _json

    vm_lib_src = ROOT / "apps" / "kinalvm" / "src" / "IO" / "Kinal" / "VM"
    ver_dir = bundle_stdpkg / "IO.Kinal.VM" / "1.0.0"
    ver_dir.mkdir(parents=True, exist_ok=True)

    # Copy library sources (not Main.kn)
    dest_src = ver_dir / "src" / "IO" / "Kinal" / "VM"
    if dest_src.exists():
        _rmtree(dest_src)
    shutil.copytree(vm_lib_src, dest_src)

    # Write manifest
    manifest = ver_dir / "package.knpkg.json"
    manifest.write_text(
        _json.dumps(
            {
                "kind": "package",
                "name": "IO.Kinal.VM",
                "version": "1.0.0",
                "summary": "KinalVM runtime package for embedded VM execution.",
                "source_root": "src/IO/Kinal/VM",
                "klib": "lib/IO.Kinal.VM.klib",
                "entry": "src/IO/Kinal/VM/Binary.kn",
            },
            indent=2,
            ensure_ascii=False,
        )
        + "\n",
        encoding="utf-8",
    )

    # Build klib
    lib_dir = ver_dir / "lib"
    lib_dir.mkdir(parents=True, exist_ok=True)
    out_klib = lib_dir / "IO.Kinal.VM.klib"
    run([compiler, "pkg", "build", "--manifest", manifest, "-o", out_klib], cwd=ver_dir)

    # Optionally strip sources for release
    if not include_sources:
        src_root = ver_dir / "src"
        if src_root.exists():
            _rmtree(src_root)


def clean_legacy_source_artifacts() -> None:
    legacy_dirs = [
        ROOT / "apps" / "kinal-lsp" / "server" / "build",
        ROOT / "apps" / "kinal-lsp" / "server" / "build-clang-release",
    ]
    for path in legacy_dirs:
        if path.exists():
            _rmtree(path)
            print(f"[OK] removed {path}")


def stage_bundle(build_type: str, dest: Path) -> Path:
    env, llvm_dir = resolve_cmake_env()
    del env
    bdir = build_dir(build_type)
    compiler = find_artifact(bdir, exe_name("kinal"))
    hostlib = find_artifact(bdir, "kinal-host.lib") if is_windows() else None
    lsp = find_artifact(bdir, exe_name("kinal-lsp-server"))
    llvm_bin = llvm_bin_dir(llvm_dir)

    if dest.exists():
        _rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    shutil.copy2(compiler, dest / compiler.name)
    if hostlib:
        shutil.copy2(hostlib, dest / hostlib.name)
    shutil.copy2(lsp, dest / lsp.name)
    copy_llvm_runtime_files(dest, llvm_bin, llvm_dir)
    copy_llvm_dev_files(dest, llvm_dir)
    copy_tree(LOCALES_DIR, dest / "locales")
    copy_tree(ROOT / "docs", dest / "docs")
    build_runtime_for_host(dest / "runtime", llvm_bin)
    build_official_stdpkg_klibs(compiler, llvm_bin)
    copy_tree(STDPKG_DIR, dest / "stdpkg")
    if build_type.lower() == "release":
        for src_dir in (dest / "stdpkg").glob("*/*/src"):
            if src_dir.is_dir():
                _rmtree(src_dir)
    copy_civetweb_bundle_files(dest)
    copy_cjson_bundle_files(dest)
    copy_host_linker_tools(dest / "linker", llvm_bin)
    write_linux_compiler_launcher(dest)
    build_kinal_vm_binary(dest / compiler.name, dest)
    # Create IO.Kinal.VM stdpkg AFTER kinalvm is built so auto-discovery
    # doesn't conflict with the package.
    _create_kinalvm_stdpkg(
        dest / compiler.name,
        dest / "stdpkg",
        include_sources=(build_type.lower() != "release"),
    )
    write_bundle_readme(dest)
    return dest


def zip_directory(src_dir: Path, zip_path: Path, root_name: str | None = None) -> Path:
    if zip_path.exists():
        zip_path.unlink()
    archive_root = root_name or src_dir.name
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(src_dir.rglob("*")):
            if path.is_file():
                arcname = str(Path(archive_root) / path.relative_to(src_dir)).replace("\\", "/")
                zf.write(path, arcname=arcname)
    return zip_path


def tar_directory(src_dir: Path, tar_path: Path, *, compression: str = "gz", root_name: str | None = None) -> Path:
    if tar_path.exists():
        tar_path.unlink()
    archive_root = root_name or src_dir.name
    if compression == "gz":
        mode = "w:gz"
    elif compression == "xz":
        mode = "w:xz"
    elif compression in ("", "none"):
        mode = "w"
    else:
        raise ValueError(f"unsupported tar compression: {compression}")
    with tarfile.open(tar_path, mode) as tf:
        tf.add(src_dir, arcname=archive_root)
    return tar_path


def sha256_file(path: Path) -> str:
    import hashlib

    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()
