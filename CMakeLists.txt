cmake_minimum_required(VERSION 3.20)

# Read versions from unified VERSION file (format: key=MAJOR.MINOR.PATCH)
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" _version_lines)
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

project(kinal VERSION ${KN_VERSION_STRING} LANGUAGES C CXX)

if (WIN32)
    enable_language(RC)
endif()

message(STATUS "Kinal version: ${KN_VERSION_STRING}")
message(STATUS "KinalVM version: ${KN_VM_VERSION_STRING}")

# Generate version header
configure_file(
    "${CMAKE_SOURCE_DIR}/apps/kinal/include/kn/version.h.in"
    "${CMAKE_SOURCE_DIR}/apps/kinal/include/kn/version.h"
    @ONLY
)

# Root build:
# - kinal compiler (requires LLVM)
# - kinal LSP server (no LLVM dependency)

add_subdirectory(apps/kinal)
add_subdirectory(apps/kinal-lsp/server)

