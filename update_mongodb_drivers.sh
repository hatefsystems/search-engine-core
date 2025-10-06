#!/bin/bash

# MongoDB Drivers Update Script for Ubuntu
# This script updates MongoDB C and C++ drivers to the latest versions
# C Driver: 1.30.3 -> 2.1.1
# C++ Driver: r4.0.0 -> r4.1.2

set -e  # Exit on any error

echo "🚀 MongoDB Drivers Update Script"
echo "================================="
echo "Current versions to be updated:"
echo "  - MongoDB C Driver: 1.30.3 -> 2.1.1"
echo "  - MongoDB C++ Driver: r4.0.0 -> r4.1.2"
echo ""

# Check if running as root
if [[ $EUID -eq 0 ]]; then
   echo "⚠️  Running as root detected. This is allowed in development environments."
   echo "   In production, consider running as regular user with sudo privileges."
   echo ""
fi

# Check if sudo is available
if ! command -v sudo &> /dev/null; then
    echo "❌ sudo command not found. Please install sudo or run as root."
    exit 1
fi

# Create temporary directory
TEMP_DIR="/tmp/mongodb-update-$(date +%s)"
echo "📁 Creating temporary directory: $TEMP_DIR"
mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

# Function to cleanup on exit
cleanup() {
    echo ""
    echo "🧹 Cleaning up temporary files..."
    cd /
    rm -rf "$TEMP_DIR"
    echo "✅ Cleanup completed"
}
trap cleanup EXIT

# Function to download with progress
download_with_progress() {
    local url="$1"
    local output="$2"
    local description="$3"
    
    echo "⬇️  Downloading $description..."
    echo "   URL: $url"
    
    # Get file size for progress calculation
    local file_size=$(curl -sI "$url" | grep -i content-length | awk '{print $2}' | tr -d '\r')
    if [[ -n "$file_size" ]]; then
        local size_mb=$((file_size / 1024 / 1024))
        echo "   Size: ${size_mb}MB"
    fi
    
    # Download with progress bar
    wget --progress=bar:force:noscroll \
         --show-progress \
         --timeout=30 \
         --tries=3 \
         -O "$output" \
         "$url"
    
    # Check if download was successful
    if [[ ! -f "$output" ]]; then
        echo "❌ Failed to download $output"
        exit 1
    fi
    
    # Show file size after download
    local downloaded_size=$(stat -c%s "$output" 2>/dev/null || echo "0")
    local downloaded_mb=$((downloaded_size / 1024 / 1024))
    echo "✅ Downloaded $description successfully (${downloaded_mb}MB)"
}

# Function to check if download was successful
check_download() {
    local file="$1"
    if [[ ! -f "$file" ]]; then
        echo "❌ Failed to download $file"
        exit 1
    fi
    echo "✅ Downloaded $file successfully"
}

# Function to build and install driver
build_and_install() {
    local driver_name="$1"
    local tar_file="$2"
    local cmake_options="$3"
    
    echo ""
    echo "🔨 Building and installing $driver_name..."
    
    # Extract
    echo "📦 Extracting $tar_file..."
    tar xzf "$tar_file"
    
    # Get directory name (remove .tar.gz)
    local dir_name="${tar_file%.tar.gz}"
    
    # Create build directory
    cd "$dir_name"
    mkdir -p cmake-build
    cd cmake-build
    
    # Configure with CMake
    echo "⚙️  Configuring $driver_name with CMake..."
    cmake .. $cmake_options
    
    # Build
    echo "🔨 Building $driver_name..."
    cmake --build . -j$(nproc)
    
    # Install
    echo "📥 Installing $driver_name..."
    sudo cmake --build . --target install
    
    echo "✅ $driver_name installed successfully"
    cd "$TEMP_DIR"
}

echo ""
echo "🗑️  Removing old MongoDB drivers..."

# Remove old headers
echo "  - Removing old headers..."
sudo rm -rf /usr/local/include/mongocxx
sudo rm -rf /usr/local/include/bsoncxx
sudo rm -rf /usr/local/include/mongocxx-3.9
sudo rm -rf /usr/local/include/bsoncxx-3.9

# Remove old libraries
echo "  - Removing old libraries..."
sudo rm -rf /usr/local/lib/libmongoc*
sudo rm -rf /usr/local/lib/libbson*
sudo rm -rf /usr/local/lib/libmongocxx*
sudo rm -rf /usr/local/lib/libbsoncxx*

# Remove old pkg-config files
echo "  - Removing old pkg-config files..."
sudo rm -rf /usr/local/lib/pkgconfig/libmongoc-1.0.pc
sudo rm -rf /usr/local/lib/pkgconfig/libbson-1.0.pc
sudo rm -rf /usr/local/lib/pkgconfig/libmongocxx.pc
sudo rm -rf /usr/local/lib/pkgconfig/libbsoncxx.pc

# Remove old CMake files
echo "  - Removing old CMake files..."
sudo rm -rf /usr/local/lib/cmake/libmongoc-1.0
sudo rm -rf /usr/local/lib/cmake/libbson-1.0
sudo rm -rf /usr/local/lib/cmake/mongocxx
sudo rm -rf /usr/local/lib/cmake/bsoncxx

echo "✅ Old drivers removed successfully"

# Update package lists
echo ""
echo "📦 Updating package lists..."
# Wait for any existing apt processes to complete
while fuser /var/lib/dpkg/lock-frontend >/dev/null 2>&1; do
    echo "   Waiting for other package manager processes to complete..."
    sleep 2
done

# Remove any stale locks
sudo rm -f /var/lib/apt/lists/lock /var/cache/apt/archives/lock /var/lib/dpkg/lock-frontend /var/lib/dpkg/lock

sudo apt-get update

# Install required dependencies
echo ""
echo "📥 Installing required dependencies..."
sudo apt-get install -y \
    wget \
    curl \
    build-essential \
    cmake \
    pkg-config \
    libssl-dev \
    zlib1g-dev \
    libuv1-dev

echo ""
download_with_progress \
    "https://github.com/mongodb/mongo-c-driver/releases/download/2.1.1/mongo-c-driver-2.1.1.tar.gz" \
    "mongo-c-driver-2.1.1.tar.gz" \
    "MongoDB C Driver 2.1.1"

echo ""
download_with_progress \
    "https://github.com/mongodb/mongo-cxx-driver/releases/download/r4.1.2/mongo-cxx-driver-r4.1.2.tar.gz" \
    "mongo-cxx-driver-r4.1.2.tar.gz" \
    "MongoDB C++ Driver r4.1.2"

# Build and install MongoDB C Driver
build_and_install "MongoDB C Driver" "mongo-c-driver-2.1.1.tar.gz" ""

# Build and install MongoDB C++ Driver
build_and_install "MongoDB C++ Driver" "mongo-cxx-driver-r4.1.2.tar.gz" \
    "-DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_INSTALL_PREFIX=/usr/local \
     -DCMAKE_PREFIX_PATH=/usr/local \
     -DBSONCXX_POLY_USE_BOOST=0 \
     -DCMAKE_CXX_STANDARD=20"

# Update library cache
echo ""
echo "🔄 Updating library cache..."
sudo ldconfig

# Verify installation
echo ""
echo "🔍 Verifying installation..."

# Check C Driver version
if command -v pkg-config &> /dev/null; then
    if pkg-config --exists libmongoc-2.0; then
        C_VERSION=$(pkg-config --modversion libmongoc-2.0)
        echo "✅ MongoDB C Driver: $C_VERSION"
    else
        echo "⚠️  MongoDB C Driver pkg-config not found (this is normal for 2.x)"
    fi
else
    echo "⚠️  pkg-config not available for version check"
fi

# Check if headers are installed
if [[ -d "/usr/local/include/mongocxx/v_noabi" ]]; then
    echo "✅ MongoDB C++ Driver headers: Installed"
else
    echo "❌ MongoDB C++ Driver headers: Not found"
fi

# Check if libraries are installed
if [[ -f "/usr/local/lib/libmongocxx.so" ]] && [[ -f "/usr/local/lib/libmongoc-2.0.so" ]]; then
    echo "✅ MongoDB libraries: Installed"
else
    echo "❌ MongoDB libraries: Not found"
fi

echo ""
echo "🎉 MongoDB drivers update completed!"
echo ""
echo "📋 Summary:"
echo "  - MongoDB C Driver: Updated to 2.1.1"
echo "  - MongoDB C++ Driver: Updated to r4.1.2"
echo "  - Headers: /usr/local/include/mongocxx/v_noabi/mongocxx"
echo "  - Libraries: /usr/local/lib/"
echo ""
echo "⚠️  Important Notes:"
echo "  1. C Driver 2.x has breaking changes - review your code"
echo "  2. Minimum MongoDB Server version is now 4.2"
echo "  3. CMake target names have changed (mongoc::static instead of mongo::mongoc_static)"
echo "  4. Some deprecated APIs have been removed"
echo ""
echo "🔧 Next steps:"
echo "  1. Update your CMakeLists.txt if using old target names"
echo "  2. Test your application for compatibility"
echo "  3. Update any deprecated API usage in your code"
echo ""
echo "✅ Script completed successfully!"
