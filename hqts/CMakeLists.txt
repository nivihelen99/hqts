cmake_minimum_required(VERSION 3.14)
project(HQTS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Dependencies ---
# Boost (for PolicyTree's multi_index_container)
# For multi_index_container, typically only headers are needed.
# However, finding components like system/thread prepares for future Boost usage.
# If vcpkg is used, this will be found via toolchain file. Otherwise, system install.
find_package(Boost 1.71.0 COMPONENTS system thread) # multi_index is header-only
# If Boost is found but components are not critical for header-only parts,
# one might use find_package(Boost 1.71.0) and then link components if needed by specific targets.
# For now, this ensures Boost headers are found.

# GoogleTest (will be added in tests/CMakeLists.txt, but can be found here too for reference)
# Using FetchContent for GTest is a robust way if not installed system-wide.
# For this phase, tests/CMakeLists.txt will handle GTest directly.

# --- Options ---
option(HQTS_ENABLE_TESTS "Build test suite" ON)
option(HQTS_ENABLE_EXAMPLES "Build example applications" OFF) # Placeholder for future

# --- Project Structure ---
add_subdirectory(src)

if(HQTS_ENABLE_TESTS)
    enable_testing() # Enables CTest
    add_subdirectory(tests)
endif()

# Example of adding an examples directory if enabled
# if(HQTS_ENABLE_EXAMPLES)
#    add_subdirectory(examples)
# endif()

# --- Compiler Warnings (Optional but recommended) ---
if(MSVC)
    # MSVC specific warnings
    # /W4 is like -Wall, /WX treats warnings as errors
    add_compile_options(/W4 /WX)
else()
    # GCC/Clang specific warnings
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough=5)
    # -Werror can be removed for development if too strict initially
endif()

message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "Project Version: ${PROJECT_VERSION}")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_CXX_STANDARD: ${CMAKE_CXX_STANDARD}")

if(Boost_FOUND)
    message(STATUS "Boost found: YES (version ${Boost_VERSION})")
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
else()
    message(WARNING "Boost not found. PolicyTree and other Boost-dependent parts might not compile.")
endif()

# Output status of test building
if(HQTS_ENABLE_TESTS)
    message(STATUS "HQTS Tests: ENABLED")
else()
    message(STATUS "HQTS Tests: DISABLED")
endif()
