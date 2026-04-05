# LLVM minimal build configuration for Kinal.
#
# Usage:
#   cmake -S <llvm-src>/llvm -B <build> -G Ninja -C <this-file> ...
#
# Keep this file as a CMake "initial cache" file (set(... CACHE ...)).

set(CMAKE_POLICY_DEFAULT_CMP0091 NEW CACHE STRING "")

# Don't build LLVM-C as a DLL (Kinal links statically).
set(LLVM_BUILD_LLVM_C_DYLIB OFF CACHE BOOL "")

# Disable shared library builds when cross-compiling with Zig.
# Zig has issues linking shared libraries for non-native targets,
# but static libraries work fine.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

# Build only what Kinal needs.
set(LLVM_ENABLE_PROJECTS "lld" CACHE STRING "")
set(LLVM_TARGETS_TO_BUILD "X86;AArch64" CACHE STRING "")

# Keep build lean.
# NOTE: We still need a few build-time utilities (like tablegen), but we do not
# need the full LLVM tool suite (opt/llc/llvm-*) for Kinal itself.
set(LLVM_INCLUDE_TOOLS ON CACHE BOOL "")
set(LLVM_BUILD_TOOLS OFF CACHE BOOL "")
set(LLVM_BUILD_UTILS OFF CACHE BOOL "")
set(LLVM_INCLUDE_RUNTIMES OFF CACHE BOOL "")
set(LLVM_BUILD_RUNTIMES OFF CACHE BOOL "")
set(LLVM_BUILD_RUNTIME OFF CACHE BOOL "")

set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "")
set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "")
set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "")
set(LLVM_INCLUDE_DOCS OFF CACHE BOOL "")

set(LLVM_ENABLE_ASSERTIONS OFF CACHE BOOL "")
set(LLVM_ENABLE_BINDINGS OFF CACHE BOOL "")
set(LLVM_ENABLE_OCAMLDOC OFF CACHE BOOL "")
set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "")
set(LLVM_ENABLE_LIBEDIT OFF CACHE BOOL "")

# Avoid optional compression deps unless explicitly enabled.
set(LLVM_ENABLE_ZLIB OFF CACHE BOOL "")
set(LLVM_ENABLE_ZSTD OFF CACHE BOOL "")

# Don't build the monolithic shared LLVM dylib.
set(LLVM_BUILD_LLVM_DYLIB OFF CACHE BOOL "")
set(LLVM_LINK_LLVM_DYLIB OFF CACHE BOOL "")

# Don't build LLVM-C as a DLL (Kinal links statically).
set(LLVM_BUILD_LLVM_C_DYLIB OFF CACHE BOOL "")

# Disable LTO shared library which fails during cross-compilation with Zig
set(LLVM_ENABLE_LTO OFF CACHE BOOL "")

# Skip checking if compiler can produce executables when cross-compiling with Zig.
# This prevents CMake from trying to link Windows libraries when targeting Linux.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY CACHE STRING "")

# Set the target system for cross-compilation to Linux.
# This ensures LLVM_ON_UNIX is defined for POSIX headers.
set(CMAKE_SYSTEM_NAME Linux CACHE STRING "")

# Explicitly specify the LLVM host triple to avoid auto-detection issues
# when cross-compiling on Windows for Linux targets.
set(LLVM_HOST_TRIPLE "x86_64-w64-windows-gnu" CACHE STRING "")

# Disable RPATH handling which causes issues with Ninja on Windows
# The install of targets requires changing RPATH, but this is not supported
# with Ninja on non-ELF platforms (like Windows)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON CACHE BOOL "")
