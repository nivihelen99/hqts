#!/bin/bash

# Placeholder for build.sh
# This script will contain commands to build the HQTS project.

set -e # Exit immediately if a command exits with a non-zero status.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="Debug" # Or "Release"

# Parse arguments (e.g., -c for clean build, -t for build type)
while getopts "ct:" opt; do
  case ${opt} in
    c )
      echo "Cleaning previous build..."
      rm -rf "${BUILD_DIR}"
      ;;
    t )
      BUILD_TYPE=$OPTARG
      ;;
    \? )
      echo "Invalid option: $OPTARG" 1>&2
      exit 1
      ;;
  esac
done

echo "Building HQTS project..."
echo "Project directory: ${PROJECT_DIR}"
echo "Build type: ${BUILD_TYPE}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure CMake
# Add any specific CMake options, e.g., -DENABLE_FEATURE_X=ON
cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -G "Ninja" # Or "Unix Makefiles"

# Build
echo "Running build..."
# ninja # Or make -j$(nproc)
cmake --build . --config ${BUILD_TYPE}

echo "Build completed successfully."
# Add instructions for where the binaries are located, e.g.:
# echo "Binaries can be found in ${BUILD_DIR}/bin"

cd "${PROJECT_DIR}"
