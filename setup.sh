#!/bin/bash

# Search Engine Core Setup Script
# This script installs prerequisites and configures the environment

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to install Docker on Linux
install_docker_linux() {
    log_info "Installing Docker..."

    # Update package index
    sudo apt-get update

    # Install required packages
    sudo apt-get install -y \
        ca-certificates \
        curl \
        gnupg \
        lsb-release

    # Add Docker's official GPG key
    sudo mkdir -p /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg

    # Set up the repository
    echo \
      "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
      $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

    # Install Docker Engine
    sudo apt-get update
    sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin

    # Start and enable Docker service
    sudo systemctl start docker
    sudo systemctl enable docker

    # Add current user to docker group (optional, requires logout/login)
    sudo usermod -aG docker $USER

    log_success "Docker installed successfully!"
    log_warning "Please log out and log back in for Docker group changes to take effect, or run: newgrp docker"
}

# Function to check Docker installation
check_docker() {
    log_info "Checking Docker installation..."

    if command_exists docker; then
        log_success "Docker is already installed"

        # Check if Docker Compose is available
        if docker compose version >/dev/null 2>&1; then
            log_success "Docker Compose (plugin) is available"
        elif command_exists docker-compose; then
            log_success "Docker Compose (standalone) is available"
        else
            log_error "Docker Compose not found. Installing..."
            sudo apt-get update && sudo apt-get install -y docker-compose-plugin
        fi

        # Test Docker
        if sudo docker run --rm hello-world >/dev/null 2>&1; then
            log_success "Docker is working correctly"
        else
            log_error "Docker installation test failed"
            exit 1
        fi
    else
        log_warning "Docker not found. Installing Docker..."
        install_docker_linux
    fi
}

# Function to create .env file
create_env_file() {
    if [ -f ".env" ]; then
        log_info ".env file already exists. Skipping creation."
        return
    fi

    log_info "Creating .env file with default configuration..."

    cat > .env << 'EOF'
# Search Engine Core Environment Configuration
# This file contains environment variables for Docker Compose
# DO NOT commit this file to version control - it may contain sensitive information

# Logging Configuration
LOG_LEVEL=debug

# Base URL Configuration
BASE_URL=http://localhost:3000

# SMTP Email Configuration
# IMPORTANT: Configure these settings for email functionality
SMTP_HOST=smtp.gmail.com
SMTP_PORT=587
SMTP_USE_TLS=true
SMTP_USE_SSL=false
SMTP_TIMEOUT=60
SMTP_CONNECTION_TIMEOUT=20
SMTP_USERNAME=your-email@gmail.com
SMTP_PASSWORD=your-app-password
FROM_EMAIL=noreply@hatef.ir
FROM_NAME=Hatef.ir Search Engine

# Email Service Configuration
EMAIL_SERVICE_ENABLED=true
EMAIL_ASYNC_ENABLED=false

# Crawler Configuration
MAX_CONCURRENT_SESSIONS=5
SPA_RENDERING_ENABLED=true
SPA_RENDERING_TIMEOUT=60000
BROWSERLESS_URL=http://browserless:3000
DEFAULT_REQUEST_TIMEOUT=60000

# Redis Search Configuration
REDIS_SEARCH_ENABLED=true

# Crawler Scheduler Configuration
SCHEDULER_TIMEZONE=Asia/Tehran
CRAWLER_WARMUP_ENABLED=true
CRAWLER_WARMUP_SCHEDULE=50,100,200,400,800
CRAWLER_WARMUP_START_HOUR=0
CRAWLER_WARMUP_END_HOUR=23
CRAWLER_JITTER_MIN=30
CRAWLER_JITTER_MAX=60
CRAWLER_TASK_INTERVAL=60
CRAWLER_MAX_RETRIES=3
CRAWLER_RETRY_DELAY=300

# Redis Sync Configuration
REDIS_SYNC_MODE=incremental
REDIS_SYNC_INTERVAL=3600
REDIS_INCREMENTAL_WINDOW=24
REDIS_SYNC_BATCH_SIZE=100

# Flower Dashboard Authentication
FLOWER_BASIC_AUTH=admin:admin123
EOF

    log_success ".env file created successfully!"
}

# Function to show SMTP configuration instructions
show_smtp_instructions() {
    echo
    log_warning "IMPORTANT: SMTP Configuration Required"
    echo
    echo "The .env file has been created with placeholder SMTP settings."
    echo "To enable email functionality, you need to configure real SMTP credentials:"
    echo
    echo "1. For Gmail:"
    echo "   - Set SMTP_USERNAME=your-gmail@gmail.com"
    echo "   - Set SMTP_PASSWORD=your-gmail-app-password (not your regular password)"
    echo "   - Enable 2-Factor Authentication on your Google account"
    echo "   - Generate an App Password: https://support.google.com/accounts/answer/185833"
    echo
    echo "2. For other email providers:"
    echo "   - Update SMTP_HOST, SMTP_PORT, SMTP_USERNAME, and SMTP_PASSWORD accordingly"
    echo
    echo "3. Edit the .env file:"
    echo "   nano .env"
    echo
}

# Main setup function
main() {
    echo
    log_info "🔍 Search Engine Core Setup Script"
    log_info "=================================="
    echo

    # Check if running on Linux
    if [[ "$OSTYPE" != "linux-gnu"* ]]; then
        log_error "This script is designed for Linux systems only."
        log_info "For other systems, please install Docker manually and create the .env file."
        exit 1
    fi

    # Check if running as root (not recommended for Docker installation)
    if [[ $EUID -eq 0 ]]; then
        log_warning "Running as root is not recommended for Docker installation."
        log_info "Consider running as a regular user with sudo privileges."
    fi

    # Install Docker if needed
    check_docker

    # Create .env file
    create_env_file

    # Show instructions
    show_smtp_instructions

    echo
    log_success "✅ Setup completed successfully!"
    echo
    log_info "Next steps:"
    echo "1. Configure your SMTP settings in .env file"
    echo "2. Run: docker compose up"
    echo "3. Access the application at http://localhost:3000"
    echo
}

# Run main function
main "$@"
