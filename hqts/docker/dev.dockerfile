# Placeholder for dev.dockerfile
# This Dockerfile is for creating a development environment for HQTS.
# It should include all tools needed for development (compilers, linters, debuggers, etc.).

FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# Install common development tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    gcc \
    g++ \
    gdb \
    clang \
    clang-tidy \
    clang-format \
    cppcheck \
    valgrind \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    python3 \
    python3-pip \
    lcov \
    # Add any other development tools you need (e.g., specific library dev packages)
    libssl-dev \
    libcurl4-openssl-dev && \
    rm -rf /var/lib/apt/lists/*

# Install vcpkg (optional, if used for dependency management)
# RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg
# RUN /opt/vcpkg/bootstrap-vcpkg.sh
# ENV VCPKG_ROOT=/opt/vcpkg
# ENV PATH="/opt/vcpkg:${PATH}"
# ENV VCPKG_DEFAULT_TRIPLET=x64-linux # Or your target triplet

# Create a non-root user for development (optional, but good practice)
ARG USERNAME=devuser
ARG USER_UID=1000
ARG USER_GID=${USER_UID}
RUN groupadd --gid ${USER_GID} ${USERNAME} && \
    useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/${USERNAME} && \
    chmod 0440 /etc/sudoers.d/${USERNAME}

# Set up working directory
WORKDIR /app
# COPY --chown=${USERNAME}:${USERNAME} . /app # Copy app source after user creation

# If using vcpkg, copy vcpkg.json and install dependencies as root before switching user
# COPY vcpkg.json /app/vcpkg.json
# RUN cd /app && vcpkg install --triplet x64-linux

# Switch to non-root user
# USER ${USERNAME}

# Set up environment for the user
# ENV PATH="/home/${USERNAME}/.local/bin:${PATH}"

# Default command for the development container
CMD ["/bin/bash"]

# Instructions for use:
# 1. Build the dev image:
#    docker build -t hqts-dev -f docker/dev.dockerfile .
# 2. Run the dev container:
#    docker run -it --rm -v $(pwd):/app -w /app hqts-dev
#    (Mount current directory into /app in the container)
#
# Inside the container, you can:
# - Use scripts: ./scripts/build.sh
# - Run CMake directly: cmake -B build -S . && cmake --build build
# - Use linters, debuggers, etc.
