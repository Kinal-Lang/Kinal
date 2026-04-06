cmake_minimum_required(VERSION 3.20)
project(kinal_compiler LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# LLVM C API is required for the compiler backend.
# Build LLVM separately and point CMake at it:
#   -DLLVM_DIR=<install>/lib/cmake/llvm
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM_DIR = ${LLVM_DIR}")

find_library(KINAL_LLVM_C_LIBRARY
    NAMES
        LLVM-C
        LLVM
        LLVM-${LLVM_VERSION_MAJOR}
        LLVM-${LLVM_PACKAGE_VERSION}
    PATHS ${LLVM_LIBRARY_DIRS}
)

if (NOT KINAL_LLVM_C_LIBRARY)
    if (TARGET LLVM)
        set(KINAL_LLVM_C_LIBRARY LLVM)
    else()
        message(FATAL_ERROR "unable to find an LLVM C API/runtime library in ${LLVM_LIBRARY_DIRS}")
    endif()
endif()

set(KINAL_COMPILER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_util.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_source.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_diag.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_diag_registry.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_lexer.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_ast.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_builtins.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_parser.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_sema.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_std.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_format.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_codegen.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_knc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_link.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kn_driver.c"
)

set(KINAL_HOST_PLATFORM_SOURCES)
if (NOT WIN32)
    list(APPEND KINAL_HOST_PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/runtime/src/kn_platform_host.c"
    )
endif()

add_executable(kinal ${KINAL_COMPILER_SOURCES} ${KINAL_HOST_PLATFORM_SOURCES})
if (WIN32)
    target_sources(kinal PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/kinal.rc"
    )
endif()
target_include_directories(kinal PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/runtime/include"
    ${LLVM_INCLUDE_DIRS}
)

add_library(kinal-host STATIC ${KINAL_COMPILER_SOURCES})
target_include_directories(kinal-host PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/runtime/include"
    ${LLVM_INCLUDE_DIRS}
)

# LLVMConfig.cmake provides compile definitions as a single string with `-D...`,
# treat them as compile options.
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_options(kinal PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_options(kinal-host PRIVATE ${LLVM_DEFINITIONS_LIST})

# Keep the host CRT intact; the runtime shim only applies to compiled user code.
target_compile_definitions(kinal PRIVATE
    KN_NO_MEM_OVERRIDE
    KN_NO_FLTUSED
    _CRT_SECURE_NO_WARNINGS
    KN_LLVM_BIN="${LLVM_TOOLS_BINARY_DIR}"
)
target_compile_definitions(kinal-host PRIVATE
    KN_HOSTLIB
    KN_NO_FLTUSED
    _CRT_SECURE_NO_WARNINGS
)

if (MSVC)
    target_compile_options(kinal PRIVATE /W4)
    target_compile_options(kinal-host PRIVATE /W4)
else()
    target_compile_options(kinal PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(kinal-host PRIVATE -Wall -Wextra -Wpedantic -ffreestanding -fno-builtin -fno-stack-protector)
endif()

target_link_libraries(kinal PRIVATE ${KINAL_LLVM_C_LIBRARY})

if (WIN32)
    target_link_libraries(kinal PRIVATE kernel32 user32)
elseif (UNIX)
    target_link_libraries(kinal PRIVATE dl)
endif()
