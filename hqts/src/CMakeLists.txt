# This library will contain all core HQTS logic
add_library(hqts_core STATIC
    # Core components
    core/token_bucket.cpp
    core/shaping_policy.cpp
    core/flow_context.cpp
    scheduler/strict_priority_scheduler.cpp # Added
    scheduler/wrr_scheduler.cpp             # Added
    scheduler/drr_scheduler.cpp             # Added
    scheduler/hfsc_scheduler.cpp            # Added

    # Policy components (if any .cpp files existed, e.g. policy_tree.cpp)
    # policy/policy_tree.cpp

    # Dataplane components (if any .cpp files existed, e.g. flow_table.cpp)
    # dataplane/flow_table.cpp

    # Main application logic (if main.cpp is part of the library)
    # If main.cpp is to be a separate executable, it should be in add_executable()
    # For now, assuming it might contain functions used by tests or other executables.
    # main.cpp # Removed as it will be part of an executable target later
)

target_include_directories(hqts_core PUBLIC
    # To find hqts/core/..., hqts/policy/... etc. from hqts_core itself or from consumers (like tests)
    ${CMAKE_SOURCE_DIR}/include
    # To find Boost headers (e.g. <boost/multi_index_container.hpp>)
    ${Boost_INCLUDE_DIRS}
    PRIVATE
    # For includes within src that are not meant to be propagated (e.g. if src had subdirs with internal headers)
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link hqts_core against Boost if it uses compiled Boost libraries.
# For header-only parts like multi_index, include directories (added above via Boost_INCLUDE_DIRS) are sufficient.
# If hqts_core directly used, e.g., Boost.Thread or Boost.System, linking ${Boost_LIBRARIES} would be necessary.
# Since multi_index is header-only, explicit linking might not be needed unless other Boost components are used.
# However, if find_package specified COMPONENTS that are compiled, they appear in Boost_LIBRARIES.
target_link_libraries(hqts_core PUBLIC
    ${Boost_LIBRARIES} # Links components like system, thread if found by find_package
)

# Set C++ standard for this target (already set globally, but good for clarity/override)
target_compile_features(hqts_core PUBLIC cxx_std_17)

# Add compile definitions if needed (e.g., for macros)
# target_compile_definitions(hqts_core PUBLIC SOME_MACRO_SPECIFIC_TO_LIB)

message(STATUS "Configuring hqts_core library...")
message(STATUS "hqts_core sources: ${CMAKE_CURRENT_SOURCE_DIR}/core/token_bucket.cpp;...") # Placeholder for actual list
message(STATUS "hqts_core includes: ${CMAKE_SOURCE_DIR}/include; ${Boost_INCLUDE_DIRS}")
