#!/bin/bash
# configure_pins.sh - Configure pin muxing for Jetson Orin Nano

set -e

echo "========================================="
echo "Jetson Orin Nano Pin Configuration Tool"
echo "========================================="

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Function to display menu
show_menu() {
    echo -e "${BLUE}Select pin configuration:${NC}"
    echo "1. Default (GPIO mode for all pins)"
    echo "2. UART1 mode (pins 8,10 as TX/RX)"
    echo "3. SPI1 mode (pins 19,21,23,24 as SPI)"
    echo "4. I2C1 mode (pins 3,5 as I2C)"
    echo "5. I2C2 mode (pins 27,28 as I2C)"
    echo "6. PWM mode (pins 32,33 as PWM)"
    echo "7. Camera mode (CSI camera interface)"
    echo "8. Custom configuration"
    echo "9. Show current configuration"
    echo "0. Exit"
    echo -n "Enter choice: "
}

# Function to configure pin mux using device tree
configure_pinmux() {
    local function=$1
    local pins=$2
    
    echo -e "${GREEN}Configuring pins: $pins as $function${NC}"
    
    # Write to pinmux sysfs (if available)
    for pin in $pins; do
        if [ -f "/sys/kernel/debug/tegra_pinctrl/pinmux/pin$pin" ]; then
            echo "$function" > /sys/kernel/debug/tegra_pinctrl/pinmux/pin$pin
            echo -e "${GREEN}Pin $pin configured as $function${NC}"
        else
            echo -e "${YELLOW}Warning: Pin $pin configuration not found${NC}"
        fi
    done
}

# Function for UART configuration
configure_uart() {
    echo -e "${GREEN}Configuring UART1 on pins 8 (TX) and 10 (RX)${NC}"
    configure_pinmux "uart1_tx" "8"
    configure_pinmux "uart1_rx" "10"
    
    # Enable UART in kernel
    systemctl enable nvgetty || true
    systemctl stop nvgetty || true
    
    # Configure UART parameters
    stty -F /dev/ttyTHS1 115200 cs8 -cstopb -parenb -crtscts
    echo -e "${GREEN}UART configured at 115200 baud${NC}"
}

# Function for SPI configuration
configure_spi() {
    echo -e "${GREEN}Configuring SPI1 on pins 19(MOSI),21(MISO),23(SCK),24(CS0)${NC}"
    configure_pinmux "spi1_mosi" "19"
    configure_pinmux "spi1_miso" "21"
    configure_pinmux "spi1_sck" "23"
    configure_pinmux "spi1_cs0" "24"
    
    # Enable SPI device
    echo "spidev" > /sys/class/spi_master/spi1/device/new_device
    echo -e "${GREEN}SPI configured${NC}"
}

# Function for I2C configuration
configure_i2c() {
    local bus=$1
    local sda_pin=$2
    local scl_pin=$3
    
    echo -e "${GREEN}Configuring I2C$bus on pins $sda_pin(SDA) and $scl_pin(SCL)${NC}"
    configure_pinmux "i2c${bus}_sda" "$sda_pin"
    configure_pinmux "i2c${bus}_scl" "$scl_pin"
    
    # Enable I2C bus
    echo "i2c-$bus" > /sys/bus/i2c/devices/i2c-$bus/new_device
    echo -e "${GREEN}I2C$bus configured${NC}"
    
    # Scan I2C bus
    echo -e "${YELLOW}Scanning I2C bus $bus...${NC}"
    i2cdetect -y $bus 2>/dev/null || echo "No devices found"
}

# Function for PWM configuration
configure_pwm() {
    echo -e "${GREEN}Configuring PWM on pins 32(PWM0) and 33(PWM1)${NC}"
    configure_pinmux "pwm0" "32"
    configure_pinmux "pwm1" "33"
    
    # Export PWM channels
    for i in 0 1; do
        if [ ! -d "/sys/class/pwm/pwmchip0/pwm$i" ]; then
            echo $i > /sys/class/pwm/pwmchip0/export 2>/dev/null || true
        fi
    done
    
    echo -e "${GREEN}PWM configured${NC}"
}

# Function for camera configuration
configure_camera() {
    echo -e "${GREEN}Configuring CSI Camera interface${NC}"
    
    # Enable camera drivers
    modprobe nvhost-vi
    modprobe nvhost-vic
    
    # Configure I2C for camera
    configure_i2c 3 0 0  # I2C3 for camera
    
    # Load camera sensor driver (example for IMX219)
    modprobe imx219
    
    echo -e "${GREEN}Camera interface configured${NC}"
    echo -e "${YELLOW}Camera devices: /dev/video0, /dev/video1${NC}"
}

# Function to show current configuration
show_config() {
    echo -e "${BLUE}Current Pin Configuration:${NC}"
    echo "====================================="
    
    # Show GPIO status
    echo -e "${GREEN}GPIO Pins:${NC}"
    for pin in 7 11 12 13 15 16 18 22 26 29 31 35 36 37 38 40; do
        if [ -d "/sys/class/gpio/gpio$pin" ]; then
            dir=$(cat /sys/class/gpio/gpio$pin/direction 2>/dev/null || echo "unknown")
            val=$(cat /sys/class/gpio/gpio$pin/value 2>/dev/null || echo "unknown")
            echo "  Pin $pin: direction=$dir, value=$val"
        else
            echo "  Pin $pin: not exported"
        fi
    done
    
    # Show UART status
    echo -e "${GREEN}UART Ports:${NC}"
    for uart in /dev/ttyTHS*; do
        if [ -e "$uart" ]; then
            echo "  $uart: available"
        fi
    done
    
    # Show I2C status
    echo -e "${GREEN}I2C Buses:${NC}"
    for i2c in /dev/i2c-*; do
        if [ -e "$i2c" ]; then
            echo "  $i2c: available"
            i2cdetect -y ${i2c#/dev/i2c-} 2>/dev/null | head -2
        fi
    done
    
    # Show SPI status
    echo -e "${GREEN}SPI Devices:${NC}"
    for spi in /dev/spidev*; do
        if [ -e "$spi" ]; then
            echo "  $spi: available"
        fi
    done
    
    # Show PWM status
    echo -e "${GREEN}PWM Channels:${NC}"
    if [ -d "/sys/class/pwm/pwmchip0" ]; then
        ls /sys/class/pwm/pwmchip0/pwm* 2>/dev/null | while read pwm; do
            if [ -f "$pwm/enable" ]; then
                en=$(cat "$pwm/enable")
                period=$(cat "$pwm/period")
                duty=$(cat "$pwm/duty_cycle")
                echo "  $(basename $pwm): enable=$en, period=$period, duty=$duty"
            fi
        done
    fi
}

# Function for custom configuration
custom_config() {
    echo -e "${BLUE}Custom Pin Configuration${NC}"
    echo -n "Enter pin number: "
    read pin
    echo -n "Enter function (gpio/uart/spi/i2c/pwm): "
    read func
    
    case $func in
        gpio)
            echo $pin > /sys/class/gpio/export 2>/dev/null
            echo -n "Set direction (in/out): "
            read dir
            echo $dir > /sys/class/gpio/gpio$pin/direction
            ;;
        uart)
            configure_pinmux "uart${pin}_tx" "$pin"
            ;;
        spi)
            configure_pinmux "spi${pin}_mosi" "$pin"
            ;;
        i2c)
            configure_pinmux "i2c${pin}_sda" "$pin"
            ;;
        pwm)
            configure_pinmux "pwm${pin}" "$pin"
            ;;
        *)
            echo -e "${RED}Unknown function${NC}"
            ;;
    esac
}

# Function to reset all pins to default
reset_all() {
    echo -e "${YELLOW}Resetting all pins to default GPIO mode${NC}"
    
    # Unexport all GPIOs
    for pin in $(ls /sys/class/gpio/gpio* 2>/dev/null | grep -o '[0-9]*$'); do
        echo $pin > /sys/class/gpio/unexport 2>/dev/null
    done
    
    # Reset pinmux
    echo "gpio" > /sys/kernel/debug/tegra_pinctrl/pinmux/*
    
    echo -e "${GREEN}All pins reset to GPIO mode${NC}"
}

# Main loop
while true; do
    show_menu
    read choice
    
    case $choice in
        1)
            reset_all
            ;;
        2)
            configure_uart
            ;;
        3)
            configure_spi
            ;;
        4)
            configure_i2c 1 3 5
            ;;
        5)
            configure_i2c 2 27 28
            ;;
        6)
            configure_pwm
            ;;
        7)
            configure_camera
            ;;
        8)
            custom_config
            ;;
        9)
            show_config
            ;;
        0)
            echo -e "${GREEN}Exiting...${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}Invalid choice${NC}"
            ;;
    esac
    
    echo
    echo -n "Press Enter to continue..."
    read
    clear
done
