#!/bin/bash

# Placeholder for install.sh
# This script will contain commands to install the HQTS project.

set -e # Exit immediately if a command exits with a non-zero status.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build" # Assuming build was done here
INSTALL_PREFIX="/usr/local" # Or custom path

# Check if project has been built
if [ ! -d "${BUILD_DIR}" ] || [ -z "$(ls -A ${BUILD_DIR})" ]; then
    echo "Build directory is empty or does not exist. Please build the project first."
    echo "Run ./scripts/build.sh"
    exit 1
fi

# Parse arguments (e.g., -p for install prefix)
while getopts "p:" opt; do
  case ${opt} in
    p )
      INSTALL_PREFIX=$OPTARG
      ;;
    \? )
      echo "Invalid option: $OPTARG" 1>&2
      exit 1
      ;;
  esac
done

echo "Installing HQTS project..."
echo "Install prefix: ${INSTALL_PREFIX}"

cd "${BUILD_DIR}"

# Install using CMake
# Ensure CMAKE_INSTALL_PREFIX was set correctly during configure step, or set it now.
# If CMAKE_INSTALL_PREFIX was set during configure:
# sudo cmake --build . --target install
# Or, if you need to specify prefix now (less common for install target):
sudo cmake --install . --prefix "${INSTALL_PREFIX}"

echo "Installation completed successfully."
echo "HQTS installed to ${INSTALL_PREFIX}"

cd "${PROJECT_DIR}"
