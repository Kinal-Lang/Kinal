# CMake toolchain file for building LLVM native host tools.
# This intentionally does not assume a bundled Zig inside the repository.

find_program(ZIG_EXECUTABLE NAMES zig zig.exe
    HINTS
        ENV KINAL_LINKER_ZIG
        ENV KINAL_ZIG
)

if(NOT ZIG_EXECUTABLE)
    message(FATAL_ERROR
        "Zig was not found. Install Zig or run `python x.py fetch zig-prebuilt`, "
        "then set KINAL_ZIG/KINAL_LINKER_ZIG if needed.")
endif()

set(CMAKE_C_COMPILER "${ZIG_EXECUTABLE}")
set(CMAKE_CXX_COMPILER "${ZIG_EXECUTABLE}")
set(CMAKE_C_COMPILER_ARG1 "cc")
set(CMAKE_CXX_COMPILER_ARG1 "c++")
set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
