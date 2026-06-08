#!/bin/bash
# setup_permissions.sh - Configure system permissions for Jetson Orin Nano peripherals

set -e

echo "========================================="
echo "Jetson Orin Nano Peripheral Setup Script"
echo "========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Please run as root (sudo)$NC"
    exit 1
fi

echo -e "${GREEN}[1/8] Creating groups...${NC}"
# Create necessary groups if they don't exist
groupadd -f gpio
groupadd -f i2c
groupadd -f spi
groupadd -f uart
groupadd -f pwm
groupadd -f can
groupadd -f video

echo -e "${GREEN}[2/8] Adding current user to groups...${NC}"
# Get the actual user (not root)
REAL_USER=${SUDO_USER:-$USER}
usermod -a -G gpio,i2c,spi,uart,pwm,can,video,dialout,plugdev $REAL_USER
echo -e "${GREEN}Added $REAL_USER to peripheral groups${NC}"

echo -e "${GREEN}[3/8] Setting up udev rules...${NC}"
# Create udev rules file
cat > /etc/udev/rules.d/99-jetson-peripherals.rules << 'EOF'
# GPIO devices
SUBSYSTEM=="gpio", KERNEL=="gpiochip*", GROUP="gpio", MODE="0660"
SUBSYSTEM=="gpio", KERNEL=="gpio*", GROUP="gpio", MODE="0660"

# I2C devices
SUBSYSTEM=="i2c-dev", KERNEL=="i2c-*", GROUP="i2c", MODE="0660"

# SPI devices
SUBSYSTEM=="spidev", KERNEL=="spidev*", GROUP="spi", MODE="0660"

# UART devices
KERNEL=="ttyTHS*", GROUP="uart", MODE="0660"
KERNEL=="ttyUSB*", GROUP="dialout", MODE="0660"
KERNEL=="ttyACM*", GROUP="dialout", MODE="0660"

# PWM devices
SUBSYSTEM=="pwm", KERNEL=="pwm*", GROUP="pwm", MODE="0660"

# CAN devices
KERNEL=="can*", GROUP="can", MODE="0660"

# Video devices (cameras)
SUBSYSTEM=="video4linux", KERNEL=="video*", GROUP="video", MODE="0660"

# USB devices
SUBSYSTEM=="usb", GROUP="plugdev", MODE="0660"
EOF

echo -e "${GREEN}[4/8] Setting up device tree overlays...${NC}"
# Configure device tree overlays for peripherals
cat > /boot/extlinux/extlinux.conf << 'EOF'
TIMEOUT 30
DEFAULT jetson-orin-nano

MENU TITLE Jetson Orin Nano boot options

LABEL jetson-orin-nano
    MENU LABEL Jetson Orin Nano
    LINUX /boot/Image
    INITRD /boot/initrd
    FDT /boot/dtb/tegra234-p3768-0000+p3767-0000-nv.dtb
    APPEND ${cbootargs} quiet root=/dev/mmcblk0p1 rw rootwait rootfstype=ext4 console=ttyTCU0,115200 console=ttyAMA0,115200 fbcon=map:0 net.ifnames=0
EOF

echo -e "${GREEN}[5/8] Configuring sysfs permissions...${NC}"
# Configure sysfs permissions for GPIO
cat > /etc/sysfs.d/99-gpio.conf << 'EOF'
class/gpio/export = 0666
class/gpio/unexport = 0666
class/gpio/gpio*/direction = 0666
class/gpio/gpio*/value = 0666
class/gpio/gpio*/edge = 0666
class/gpio/gpio*/active_low = 0666
EOF

echo -e "${GREEN}[6/8] Setting up CAN interface...${NC}"
# Create CAN interface configuration
cat > /etc/network/interfaces.d/can0 << 'EOF'
auto can0
iface can0 inet manual
    pre-up ip link set $IFACE type can bitrate 500000
    up ip link set $IFACE up
    down ip link set $IFACE down
EOF

echo -e "${GREEN}[7/8] Configuring kernel modules...${NC}"
# Load necessary kernel modules at boot
cat > /etc/modules-load.d/jetson-peripherals.conf << 'EOF'
i2c-dev
spidev
can
can-raw
slcan
mttcan
nvhost-vic
nvhost-vi
EOF

# Enable I2C bus
cat > /etc/modprobe.d/i2c.conf << 'EOF'
options i2c-dev force=1
options i2c-nvidia-gpu bus_timeout=1000
EOF

echo -e "${GREEN}[8/8] Reloading udev rules...${NC}"
# Reload udev rules
udevadm control --reload-rules
udevadm trigger

# Configure sysctl for real-time performance
cat >> /etc/sysctl.conf << 'EOF'
# Real-time performance for peripherals
kernel.sched_rt_runtime_us = 950000
kernel.sched_rt_period_us = 1000000
vm.swappiness = 10
fs.file-max = 2097152
EOF

# Set limits for real-time applications
cat >> /etc/security/limits.conf << 'EOF'
* soft rtprio 99
* hard rtprio 99
* soft memlock unlimited
* hard memlock unlimited
* soft nofile 1048576
* hard nofile 1048576
EOF

echo -e "${GREEN}=========================================${NC}"
echo -e "${GREEN}Setup complete!${NC}"
echo -e "${YELLOW}Please reboot your system for changes to take effect.${NC}"
echo -e "${YELLOW}After reboot, verify with: groups \$USER${NC}"
echo -e "${GREEN}=========================================${NC}"

# Optional: Create test directory
mkdir -p /tmp/jetson-peripheral-tests
chmod 777 /tmp/jetson-peripheral-tests

exit 0
