#!/bin/bash
# test_all_peripherals.sh - Comprehensive test suite for all peripherals

set -e

echo "========================================="
echo "Jetson Orin Nano Peripheral Test Suite"
echo "========================================="

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Test results tracking
PASSED=0
FAILED=0
SKIPPED=0

# Function to print test result
print_result() {
    local test_name=$1
    local result=$2
    local message=$3
    
    if [ "$result" == "PASS" ]; then
        echo -e "${GREEN}✓ $test_name: PASS${NC}"
        ((PASSED++))
    elif [ "$result" == "FAIL" ]; then
        echo -e "${RED}✗ $test_name: FAIL - $message${NC}"
        ((FAILED++))
    else
        echo -e "${YELLOW}○ $test_name: SKIPPED - $message${NC}"
        ((SKIPPED++))
    fi
}

# Function to check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then 
        echo -e "${YELLOW}Some tests require root privileges. Run with sudo for full testing.${NC}"
        return 1
    fi
    return 0
}

# Test GPIO
test_gpio() {
    echo -e "\n${BLUE}=== Testing GPIO ===${NC}"
    
    # Check if GPIO sysfs is available
    if [ ! -d "/sys/class/gpio" ]; then
        print_result "GPIO" "SKIP" "GPIO sysfs not available"
        return
    fi
    
    # Test GPIO 18 (available on header)
    local test_pin=18
    
    # Export GPIO
    echo $test_pin > /sys/class/gpio/export 2>/dev/null
    if [ ! -d "/sys/class/gpio/gpio$test_pin" ]; then
        print_result "GPIO" "FAIL" "Cannot export GPIO $test_pin"
        return
    fi
    
    # Set as output
    echo "out" > /sys/class/gpio/gpio$test_pin/direction
    
    # Test toggling
    echo "1" > /sys/class/gpio/gpio$test_pin/value
    local val1=$(cat /sys/class/gpio/gpio$test_pin/value)
    usleep 100000
    echo "0" > /sys/class/gpio/gpio$test_pin/value
    local val2=$(cat /sys/class/gpio/gpio$test_pin/value)
    
    if [ "$val1" -eq 1 ] && [ "$val2" -eq 0 ]; then
        print_result "GPIO" "PASS" "Pin $test_pin toggled successfully"
    else
        print_result "GPIO" "FAIL" "Pin toggling failed"
    fi
    
    # Cleanup
    echo $test_pin > /sys/class/gpio/unexport 2>/dev/null
}

# Test UART
test_uart() {
    echo -e "\n${BLUE}=== Testing UART ===${NC}"
    
    local uart_dev="/dev/ttyTHS1"
    
    if [ ! -e "$uart_dev" ]; then
        print_result "UART" "SKIP" "$uart_dev not available"
        return
    fi
    
    # Check permissions
    if [ ! -r "$uart_dev" ] || [ ! -w "$uart_dev" ]; then
        print_result "UART" "FAIL" "Permission denied for $uart_dev"
        return
    fi
    
    # Test loopback (requires hardware loopback connector)
    print_result "UART" "SKIP" "Manual test required (connect TX to RX)"
    
    # Show UART configuration
    stty -F $uart_dev -a 2>/dev/null | head -1
}

# Test SPI
test_spi() {
    echo -e "\n${BLUE}=== Testing SPI ===${NC}"
    
    local spi_dev="/dev/spidev0.0"
    
    if [ ! -e "$spi_dev" ]; then
        print_result "SPI" "SKIP" "$spi_dev not available"
        return
    fi
    
    # Check if spidev_test is available
    if command -v spidev_test &> /dev/null; then
        # Run SPI loopback test
        if spidev_test -D $spi_dev -v 2>&1 | grep -q "00 11 22 33"; then
            print_result "SPI" "PASS" "Loopback test passed"
        else
            print_result "SPI" "FAIL" "Loopback test failed (requires hardware loopback)"
        fi
    else
        print_result "SPI" "SKIP" "spidev_test not installed"
        echo "Install with: sudo apt install spi-tools"
    fi
}

# Test I2C
test_i2c() {
    echo -e "\n${BLUE}=== Testing I2C ===${NC}"
    
    local i2c_bus=1
    local i2c_dev="/dev/i2c-$i2c_bus"
    
    if [ ! -e "$i2c_dev" ]; then
        print_result "I2C" "SKIP" "$i2c_dev not available"
        return
    fi
    
    # Check if i2cdetect is available
    if command -v i2cdetect &> /dev/null; then
        # Scan I2C bus
        local devices=$(i2cdetect -y $i2c_bus 2>/dev/null | grep -E "[0-9a-f]{2}" | wc -l)
        
        if [ $devices -gt 0 ]; then
            print_result "I2C" "PASS" "Found $devices devices on bus $i2c_bus"
            i2cdetect -y $i2c_bus
        else
            print_result "I2C" "WARN" "No devices found on bus $i2c_bus"
        fi
    else
        print_result "I2C" "SKIP" "i2c-tools not installed"
        echo "Install with: sudo apt install i2c-tools"
    fi
}

# Test PWM
test_pwm() {
    echo -e "\n${BLUE}=== Testing PWM ===${NC}"
    
    if [ ! -d "/sys/class/pwm" ]; then
        print_result "PWM" "SKIP" "PWM sysfs not available"
        return
    fi
    
    # Check for PWM chip
    if [ -d "/sys/class/pwm/pwmchip0" ]; then
        # Export PWM channel 0
        if [ ! -d "/sys/class/pwm/pwmchip0/pwm0" ]; then
            echo 0 > /sys/class/pwm/pwmchip0/export 2>/dev/null
            sleep 1
        fi
        
        if [ -d "/sys/class/pwm/pwmchip0/pwm0" ]; then
            # Set period and duty cycle
            echo 1000000 > /sys/class/pwm/pwmchip0/pwm0/period 2>/dev/null
            echo 500000 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle 2>/dev/null
            echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable 2>/dev/null
            
            print_result "PWM" "PASS" "PWM0 configured (period=1ms, duty=50%)"
            
            # Disable after test
            echo 0 > /sys/class/pwm/pwmchip0/pwm0/enable 2>/dev/null
            echo 0 > /sys/class/pwm/pwmchip0/unexport 2>/dev/null
        else
            print_result "PWM" "FAIL" "Cannot export PWM channel"
        fi
    else
        print_result "PWM" "SKIP" "No PWM chip found"
    fi
}

# Test Ethernet
test_ethernet() {
    echo -e "\n${BLUE}=== Testing Ethernet ===${NC}"
    
    # Check for eth0 interface
    if [ -d "/sys/class/net/eth0" ]; then
        # Check link status
        if [ -f "/sys/class/net/eth0/operstate" ]; then
            state=$(cat /sys/class/net/eth0/operstate)
            if [ "$state" == "up" ]; then
                # Get IP address
                ip=$(ip -4 addr show eth0 | grep -oP '(?<=inet\s)\d+(\.\d+){3}' | head -1)
                print_result "Ethernet" "PASS" "Link is up, IP: ${ip:-none}"
            else
                print_result "Ethernet" "WARN" "Link is down"
            fi
        else
            print_result "Ethernet" "FAIL" "Cannot read link status"
        fi
    else
        print_result "Ethernet" "SKIP" "eth0 interface not found"
    fi
}

# Test USB
test_usb() {
    echo -e "\n${BLUE}=== Testing USB ===${NC}"
    
    if command -v lsusb &> /dev/null; then
        local usb_devices=$(lsusb | wc -l)
        if [ $usb_devices -gt 0 ]; then
            print_result "USB" "PASS" "Found $usb_devices USB devices"
            lsusb
        else
            print_result "USB" "WARN" "No USB devices found"
        fi
    else
        print_result "USB" "SKIP" "lsusb not available"
        echo "Install with: sudo apt install usbutils"
    fi
}

# Test PCIe
test_pcie() {
    echo -e "\n${BLUE}=== Testing PCIe ===${NC}"
    
    if command -v lspci &> /dev/null; then
        local pcie_devices=$(lspci | wc -l)
        if [ $pcie_devices -gt 0 ]; then
            print_result "PCIe" "PASS" "Found $pcie_devices PCIe devices"
            lspci
        else
            print_result "PCIe" "WARN" "No PCIe devices found"
        fi
    else
        print_result "PCIe" "SKIP" "lspci not available"
        echo "Install with: sudo apt install pciutils"
    fi
}

# Test CAN
test_can() {
    echo -e "\n${BLUE}=== Testing CAN ===${NC}"
    
    # Check for CAN interface
    if ip link show can0 &>/dev/null; then
        # Bring up CAN interface
        ip link set can0 up type can bitrate 500000 2>/dev/null
        if [ $? -eq 0 ]; then
            print_result "CAN" "PASS" "CAN interface configured (500kbps)"
            ip link set can0 down 2>/dev/null
        else
            print_result "CAN" "FAIL" "Cannot bring up CAN interface"
        fi
    else
        print_result "CAN" "SKIP" "No CAN interface found"
        echo "Add CAN interface with: sudo ip link add dev can0 type can"
    fi
}

# Test Camera
test_camera() {
    echo -e "\n${BLUE}=== Testing CSI Camera ===${NC}"
    
    # Check for video devices
    if [ -d "/sys/class/video4linux" ]; then
        local cameras=$(ls -1 /dev/video* 2>/dev/null | wc -l)
        if [ $cameras -gt 0 ]; then
            print_result "Camera" "PASS" "Found $cameras video devices"
            ls -la /dev/video*
        else
            print_result "Camera" "WARN" "No video devices found"
        fi
    else
        print_result "Camera" "SKIP" "Video4Linux not available"
    fi
}

# Test M.2
test_m2() {
    echo -e "\n${BLUE}=== Testing M.2 ===${NC}"
    
    # Check for NVMe device
    if [ -d "/sys/class/nvme" ]; then
        local nvme_devices=$(ls -1 /sys/class/nvme | wc -l)
        if [ $nvme_devices -gt 0 ]; then
            print_result "M.2" "PASS" "Found $nvme_devices NVMe device(s)"
            for dev in /sys/class/nvme/nvme*; do
                if [ -f "$dev/model" ]; then
                    model=$(cat $dev/model)
                    echo "  Model: $model"
                fi
            done
        else
            print_result "M.2" "WARN" "No NVMe devices found"
        fi
    else
        print_result "M.2" "SKIP" "NVMe subsystem not available"
    fi
}

# Test I2C devices (specific sensors)
test_i2c_sensors() {
    echo -e "\n${BLUE}=== Testing I2C Sensors ===${NC}"
    
    # Common sensor addresses
    local sensors="0x18 0x19 0x40 0x41 0x48 0x49 0x68 0x69 0x76 0x77"
    
    for addr in $sensors; do
        if i2cget -y 1 $addr 0x00 &>/dev/null; then
            print_result "Sensor 0x$addr" "PASS" "Device found"
        fi
    done
}

# Performance test
test_performance() {
    echo -e "\n${BLUE}=== Performance Tests ===${NC}"
    
    # CPU performance
    echo -e "${YELLOW}CPU Info:${NC}"
    grep "model name" /proc/cpuinfo | head -1
    echo "Cores: $(nproc)"
    
    # Memory
    echo -e "${YELLOW}Memory:${NC}"
    free -h
    
    # Disk performance (if NVMe present)
    if [ -d "/sys/class/nvme" ]; then
        echo -e "${YELLOW}NVMe Performance:${NC}"
        dd if=/dev/nvme0n1 of=/dev/null bs=1M count=100 2>&1 | grep -o "[0-9.]\+ MB/s"
    fi
    
    # Network performance
    if [ -d "/sys/class/net/eth0" ]; then
        echo -e "${YELLOW}Network Speed:${NC}"
        cat /sys/class/net/eth0/speed 2>/dev/null || echo "Unknown"
    fi
}

# Generate report
generate_report() {
    echo -e "\n${BLUE}=========================================${NC}"
    echo -e "${BLUE}Test Summary${NC}"
    echo -e "${BLUE}=========================================${NC}"
    echo -e "${GREEN}PASSED: $PASSED${NC}"
    echo -e "${RED}FAILED: $FAILED${NC}"
    echo -e "${YELLOW}SKIPPED: $SKIPPED${NC}"
    echo -e "${BLUE}TOTAL: $((PASSED + FAILED + SKIPPED))${NC}"
    
    # Save report to file
    local report_file="/tmp/jetson_test_report_$(date +%Y%m%d_%H%M%S).txt"
    {
        echo "Jetson Orin Nano Test Report"
        echo "Date: $(date)"
        echo "====================================="
        echo "PASSED: $PASSED"
        echo "FAILED: $FAILED"
        echo "SKIPPED: $SKIPPED"
        echo "====================================="
        echo "System Information:"
        uname -a
        echo ""
        echo "Jetson Information:"
        cat /etc/nv_tegra_release 2>/dev/null || echo "Not available"
    } > $report_file
    
    echo -e "\n${GREEN}Report saved to: $report_file${NC}"
}

# Main test execution
main() {
    echo -e "${BLUE}System Information:${NC}"
    echo "Kernel: $(uname -r)"
    echo "Jetson: $(cat /etc/nv_tegra_release 2>/dev/null | head -1 || echo 'Unknown')"
    echo "Date: $(date)"
    
    # Run all tests
    test_gpio
    test_uart
    test_spi
    test_i2c
    test_pwm
    test_ethernet
    test_usb
    test_pcie
    test_can
    test_camera
    test_m2
    test_i2c_sensors
    test_performance
    
    # Generate report
    generate_report
}

# Run main function
main
