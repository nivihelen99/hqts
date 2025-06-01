#!/bin/bash

# Placeholder for run-tests.sh
# This script will execute various tests for the HQTS project (unit, integration, etc.).

set -e # Exit immediately if a command exits with a non-zero status.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build" # Assuming tests are run from build directory

# Check if project has been built
if [ ! -d "${BUILD_DIR}" ]; then
    echo "Build directory does not exist. Please build the project first."
    echo "Run ./scripts/build.sh"
    exit 1
fi

echo "Running HQTS tests..."

cd "${BUILD_DIR}"

# Run tests using CTest
# CTest will run tests discovered by CMake (enable_testing() and add_test())
# Options:
#  -V : Verbose output
#  --output-on-failure : Show output from failing tests
#  -R <regex> : Run tests matching regex
#  -E <regex> : Exclude tests matching regex
#  -L <label> : Run tests with a specific label
ctest --output-on-failure -C Debug # Or Release, depending on build config

# Example for running specific test categories if CTest labels are used:
# echo "Running unit tests..."
# ctest -L UNIT --output-on-failure
#
# echo "Running integration tests..."
# ctest -L INTEGRATION --output-on-failure

echo "All tests completed."

cd "${PROJECT_DIR}"
