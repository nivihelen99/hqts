# Placeholder for Dockerfile
# This Dockerfile will be used to build a production image for HQTS.

# Start from a base image (e.g., Ubuntu, Alpine)
FROM ubuntu:22.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive

# Install build dependencies (compilers, CMake, Ninja, vcpkg dependencies)
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    g++ \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config && \
    rm -rf /var/lib/apt/lists/*

# Install vcpkg (optional, if used for dependency management)
# RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg
# RUN /opt/vcpkg/bootstrap-vcpkg.sh
# ENV VCPKG_ROOT=/opt/vcpkg
# ENV PATH="/opt/vcpkg:${PATH}"

WORKDIR /app

# Copy vcpkg.json and CMakeLists.txt to leverage Docker layer caching for dependencies
COPY vcpkg.json ./
# COPY CMakeLists.txt ./ # If top-level CMakeLists.txt is simple and vcpkg.json defines deps

# Install dependencies using vcpkg
# RUN vcpkg install --triplet x64-linux # Or your target triplet

# Copy the rest of the source code
COPY . .

# Configure and build the project
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake # If using vcpkg
RUN cmake --build build --config Release

# --- Runtime Stage ---
FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies (e.g., shared libraries needed by HQTS)
RUN apt-get update && apt-get install -y \
    # Add runtime libraries here, e.g., libssl-dev, libcurl4
    ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /opt/hqts

# Copy built artifacts from the builder stage
COPY --from=builder /app/build/bin/hqts .  # Adjust path to your executable
# COPY --from=builder /app/config ./config # Example: copy configuration files

# Set the entrypoint or default command
ENTRYPOINT ["./hqts"]
# CMD ["--help"] # Example default command

# Expose any necessary ports
# EXPOSE 8080

# Add labels, environment variables, etc.
LABEL maintainer="your-email@example.com"
LABEL version="0.1.0"
