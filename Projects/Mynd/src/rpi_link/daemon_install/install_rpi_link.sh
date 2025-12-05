#!/bin/bash
#
# Installation script for Mynd RPi Link Daemon
#
# This script installs the daemon, configuration, and systemd service
# on a Raspberry Pi running Moode OS or similar Linux distribution.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAEMON_SCRIPT="mynd_rpi_link.py"
CONFIG_FILE="mynd_rpi_link.conf"
SERVICE_FILE="mynd-rpi-link.service"
SUDOERS_FILE="mynd-rpi-link.sudoers"

INSTALL_DIR="/usr/local/bin"
CONFIG_DIR="/etc"
SYSTEMD_DIR="/etc/systemd/system"
SUDOERS_DIR="/etc/sudoers.d"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "Mynd RPi Link Daemon Installation"
echo "=================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}ERROR: This script must be run as root (use sudo)${NC}"
    exit 1
fi

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}ERROR: Python 3 is not installed${NC}"
    exit 1
fi

# Check if required Python packages are installed
echo "Checking Python dependencies..."

NEEDS_UPDATE=false
PACKAGES_TO_INSTALL=""

# Check for pyserial (python3-serial in apt)
if ! python3 -c "import serial" 2>/dev/null; then
    echo -e "${YELLOW}WARNING: pyserial not found. Will install python3-serial${NC}"
    PACKAGES_TO_INSTALL="${PACKAGES_TO_INSTALL} python3-serial"
    NEEDS_UPDATE=true
fi

# Check for requests (python3-requests in apt)
if ! python3 -c "import requests" 2>/dev/null; then
    echo -e "${YELLOW}WARNING: requests not found. Will install python3-requests${NC}"
    PACKAGES_TO_INSTALL="${PACKAGES_TO_INSTALL} python3-requests"
    NEEDS_UPDATE=true
fi

# Install missing packages if any
if [ -n "$PACKAGES_TO_INSTALL" ]; then
    if command -v apt-get &> /dev/null; then
        if [ "$NEEDS_UPDATE" = true ]; then
            echo "Updating package list..."
            apt-get update -qq
        fi
        echo "Installing Python packages..."
        apt-get install -y "$PACKAGES_TO_INSTALL" || {
            echo -e "${RED}ERROR: Failed to install Python packages: $PACKAGES_TO_INSTALL${NC}"
            echo -e "${YELLOW}You may need to install them manually using pip3 with --break-system-packages${NC}"
            exit 1
        }
        echo -e "${GREEN}✓${NC} Installed Python dependencies"
    else
        echo -e "${RED}ERROR: apt-get not found. Please install packages manually: $PACKAGES_TO_INSTALL${NC}"
        exit 1
    fi
else
    echo -e "${GREEN}✓${NC} All Python dependencies are installed"
fi

# Install daemon script
echo "Installing daemon script..."
cp "${SCRIPT_DIR}/${DAEMON_SCRIPT}" "${INSTALL_DIR}/${DAEMON_SCRIPT}"
chmod +x "${INSTALL_DIR}/${DAEMON_SCRIPT}"
echo -e "${GREEN}✓${NC} Installed ${INSTALL_DIR}/${DAEMON_SCRIPT}"

# Install configuration file
echo "Installing configuration file..."
if [ -f "${CONFIG_DIR}/${CONFIG_FILE}" ]; then
    echo -e "${YELLOW}WARNING: Configuration file already exists at ${CONFIG_DIR}/${CONFIG_FILE}${NC}"
    read -p "Overwrite? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        cp "${SCRIPT_DIR}/${CONFIG_FILE}" "${CONFIG_DIR}/${CONFIG_FILE}"
        echo -e "${GREEN}✓${NC} Updated ${CONFIG_DIR}/${CONFIG_FILE}"
    else
        echo -e "${YELLOW}  Skipped (keeping existing configuration)${NC}"
    fi
else
    cp "${SCRIPT_DIR}/${CONFIG_FILE}" "${CONFIG_DIR}/${CONFIG_FILE}"
    echo -e "${GREEN}✓${NC} Installed ${CONFIG_DIR}/${CONFIG_FILE}"
fi

# Install systemd service
echo "Installing systemd service..."
cp "${SCRIPT_DIR}/${SERVICE_FILE}" "${SYSTEMD_DIR}/${SERVICE_FILE}"
systemctl daemon-reload
echo -e "${GREEN}✓${NC} Installed systemd service"

# Configure sudoers (with user confirmation)
echo ""
echo "Sudoers Configuration"
echo "-------------------"
echo "The daemon needs permission to run 'sudo poweroff' without a password."
echo "This will be configured in /etc/sudoers.d/mynd-rpi-link"
echo ""
read -p "Configure sudoers? (Y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    # Get the user from the service file
    SERVICE_USER=$(grep "^User=" "${SYSTEMD_DIR}/${SERVICE_FILE}" | cut -d'=' -f2)
    
    if [ -z "$SERVICE_USER" ]; then
        SERVICE_USER="pi"
        echo -e "${YELLOW}WARNING: Could not determine user from service file, using 'pi'${NC}"
    fi
    
    echo "Using user: ${SERVICE_USER}"
    
    # Create sudoers file with correct user
    sed "s/^pi ALL=/^${SERVICE_USER} ALL=/" "${SCRIPT_DIR}/${SUDOERS_FILE}" > "${SUDOERS_DIR}/mynd-rpi-link"
    chmod 0440 "${SUDOERS_DIR}/mynd-rpi-link"
    
    # Verify sudoers syntax
    if visudo -c -f "${SUDOERS_DIR}/mynd-rpi-link" 2>/dev/null; then
        echo -e "${GREEN}✓${NC} Configured sudoers for user ${SERVICE_USER}"
    else
        echo -e "${RED}ERROR: Sudoers syntax check failed${NC}"
        rm -f "${SUDOERS_DIR}/mynd-rpi-link"
        exit 1
    fi
else
    echo -e "${YELLOW}  Skipped (you can configure sudoers manually later)${NC}"
fi

# Enable and start service
echo ""
read -p "Enable and start the service now? (Y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    systemctl enable mynd-rpi-link.service
    systemctl start mynd-rpi-link.service
    echo -e "${GREEN}✓${NC} Service enabled and started"
    
    # Show status
    sleep 1
    systemctl status mynd-rpi-link.service --no-pager || true
else
    echo -e "${YELLOW}  Service installed but not started. Run:${NC}"
    echo "    sudo systemctl enable mynd-rpi-link.service"
    echo "    sudo systemctl start mynd-rpi-link.service"
fi

echo ""
echo -e "${GREEN}Installation complete!${NC}"
echo ""
echo "Useful commands:"
echo "  Check status:    sudo systemctl status mynd-rpi-link"
echo "  View logs:       sudo journalctl -u mynd-rpi-link -f"
echo "  Restart service: sudo systemctl restart mynd-rpi-link"
echo "  Stop service:    sudo systemctl stop mynd-rpi-link"

