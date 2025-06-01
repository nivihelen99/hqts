# --- GTest ---
# Attempt to find an installed GoogleTest package first.
# This is the preferred method if GTest is available system-wide or via CMake environment (e.g., vcpkg).
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GTest package not found. Fetching from source...")
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        release-1.12.1 # Using a specific stable tag
    )
    # Set options to avoid building gmock_main if only gtest is needed, and to build GTest's shared libs if desired.
    # FetchContent_MakeAvailable will configure and add GTest targets (GTest::GTest, GTest::Main)
    FetchContent_MakeAvailable(googletest)
    message(STATUS "GTest fetched and made available via FetchContent.")
else()
    message(STATUS "Found GTest package.")
    message(STATUS "GTest version: ${GTEST_VERSION}") # Available if FindGTest module sets it
endif()


# --- Test Executable ---
add_executable(run_hqts_tests
    # List all test .cpp files
    unit/core/test_token_bucket.cpp
    unit/policy/test_policy_tree.cpp
    unit/dataplane/test_flow_table.cpp
    unit/scheduler/test_strict_priority_scheduler.cpp # Added
    unit/scheduler/test_wrr_scheduler.cpp             # Added
    unit/scheduler/test_drr_scheduler.cpp             # Added
    unit/scheduler/test_hfsc_scheduler.cpp            # Added
    unit/scheduler/test_aqm_queue.cpp                 # Added
    unit/core/test_traffic_shaper.cpp                 # Added (was missing from explicit list)
    unit/dataplane/test_flow_classifier.cpp           # Added
    # Add new test_*.cpp files here as they are created
)

# Link tests against our library and GTest
target_link_libraries(run_hqts_tests PRIVATE
    hqts_core       # Our project's core library
    GTest::GTest    # GoogleTest library target
    GTest::Main     # GoogleTest main function target (provides main())
    ${Boost_LIBRARIES} # Tests might use Boost indirectly via hqts_core or directly
)

# Include directories for tests
target_include_directories(run_hqts_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include  # To find hqts/core/*.h, hqts/policy/*.h etc.
    # ${Boost_INCLUDE_DIRS} is already part of hqts_core's PUBLIC interface,
    # so it should be inherited. Explicitly adding it here is also fine.
    ${Boost_INCLUDE_DIRS}
    # GTest include directories are automatically handled by target_link_libraries(GTest::GTest)
)

# Add tests to CTest for automatic discovery
# This requires CMake 3.10+
include(GoogleTest)
if(TARGET GTest::GTest) # Ensure GTest targets are available before discovering
    gtest_discover_tests(run_hqts_tests)
    message(STATUS "CTest tests discovered for run_hqts_tests.")
else()
    message(WARNING "GTest targets not found, CTest discovery for run_hqts_tests might fail or be incomplete.")
endif()

message(STATUS "Configuring run_hqts_tests executable...")
message(STATUS "GTest found for tests: ${GTest_FOUND}")
if(TARGET GTest::GTest AND TARGET GTest::Main)
    message(STATUS "GTest::GTest and GTest::Main targets are available.")
else()
    message(WARNING "GTest::GTest or GTest::Main targets are NOT available. Linking tests might fail.")
endif()
