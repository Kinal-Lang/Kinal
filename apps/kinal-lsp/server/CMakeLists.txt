cmake_minimum_required(VERSION 3.20)
project(kinal_lsp_server LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KN_COMPILER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../kinal")
set(KN_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/runtime")

if (NOT DEFINED KINAL_GENERATED_INCLUDE_DIR)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../../VERSION" _version_lines)
    foreach(_line IN LISTS _version_lines)
        if(_line MATCHES "^kinal=([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            set(KN_VERSION_MAJOR   ${CMAKE_MATCH_1})
            set(KN_VERSION_MINOR   ${CMAKE_MATCH_2})
            set(KN_VERSION_PATCH   ${CMAKE_MATCH_3})
            set(KN_VERSION_STRING  "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
        elseif(_line MATCHES "^kinalvm=([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            set(KN_VM_VERSION_MAJOR  ${CMAKE_MATCH_1})
            set(KN_VM_VERSION_MINOR  ${CMAKE_MATCH_2})
            set(KN_VM_VERSION_PATCH  ${CMAKE_MATCH_3})
            set(KN_VM_VERSION_STRING "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
        endif()
    endforeach()
    set(KINAL_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${KINAL_GENERATED_INCLUDE_DIR}/kn")
    configure_file(
        "${KN_COMPILER_DIR}/include/kn/version.h.in"
        "${KINAL_GENERATED_INCLUDE_DIR}/kn/version.h"
        @ONLY
    )
endif()

set(KN_FRONTEND_SOURCES
    "${KN_COMPILER_DIR}/src/kn_util.c"
    "${KN_COMPILER_DIR}/src/kn_source.c"
    "${KN_COMPILER_DIR}/src/kn_diag.c"
    "${KN_COMPILER_DIR}/src/kn_diag_registry.c"
    "${KN_COMPILER_DIR}/src/kn_lexer.c"
    "${KN_COMPILER_DIR}/src/kn_ast.c"
    "${KN_COMPILER_DIR}/src/kn_builtins.c"
    "${KN_COMPILER_DIR}/src/kn_parser.c"
    "${KN_COMPILER_DIR}/src/kn_sema.c"
    "${KN_COMPILER_DIR}/src/kn_std.c"
)

set(KN_FRONTEND_HOST_PLATFORM_SOURCES)
if (NOT WIN32)
    list(APPEND KN_FRONTEND_HOST_PLATFORM_SOURCES
        "${KN_RUNTIME_DIR}/src/kn_platform_host.c"
    )
endif()

add_library(kinal-frontend STATIC ${KN_FRONTEND_SOURCES} ${KN_FRONTEND_HOST_PLATFORM_SOURCES})
target_include_directories(kinal-frontend PUBLIC
    "${KINAL_GENERATED_INCLUDE_DIR}"
    "${KN_COMPILER_DIR}/include"
    "${KN_RUNTIME_DIR}/include"
)
target_compile_definitions(kinal-frontend PRIVATE KN_NO_MEM_OVERRIDE KN_NO_FLTUSED)

add_executable(kinal-lsp-server src/main.cpp)
target_include_directories(kinal-lsp-server PRIVATE
    "${KINAL_GENERATED_INCLUDE_DIR}"
    "${KN_COMPILER_DIR}/include"
    "${KN_RUNTIME_DIR}/include"
)
target_link_libraries(kinal-lsp-server PRIVATE kinal-frontend)

if (WIN32)
    target_link_libraries(kinal-lsp-server PRIVATE kernel32 user32)
elseif (UNIX)
    target_link_libraries(kinal-lsp-server PRIVATE dl)
endif()

if (MSVC)
    target_compile_options(kinal-frontend PRIVATE /W4)
    target_compile_options(kinal-lsp-server PRIVATE /W4 /EHsc)
else()
    target_compile_options(kinal-frontend PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(kinal-lsp-server PRIVATE -Wall -Wextra -Wpedantic)
endif()
