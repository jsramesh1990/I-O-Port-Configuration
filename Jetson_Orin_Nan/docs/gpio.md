# GPIO (General Purpose Input/Output)

## Overview

The Jetson Orin Nano provides 28 GPIO pins on the 40-pin header, each capable of digital input/output with interrupt support. GPIOs operate at 3.3V logic levels but are 5V tolerant.

## Hardware Specifications

| Parameter | Value |
|-----------|-------|
| Number of GPIOs | 28 |
| Logic Level | 3.3V (5V tolerant) |
| Max Frequency | 50 MHz (theoretical) |
| Practical Frequency | 25 MHz |
| Output Current | 2mA typical, 20mA max |
| Input Leakage | <1μA |
| Pull-up/Down | 10kΩ - 100kΩ configurable |
| Interrupt Latency | ~40-60 μs |

## Pin Mapping

| Pin | GPIO# | Default Function | Alternate Functions |
|-----|-------|------------------|---------------------|
| 3 | - | I2C1_SDA | - |
| 5 | - | I2C1_SCL | - |
| 7 | 8 | GPIO8 | SPI1_CS1 |
| 8 | - | UART1_TX | - |
| 10 | - | UART1_RX | - |
| 11 | 17 | GPIO17 | SPI1_CS0 |
| 12 | 18 | GPIO18 | I2S1_SDIN |
| 13 | 27 | GPIO27 | I2S1_FS |
| 15 | 22 | GPIO22 | I2S1_DOUT |
| 16 | 23 | GPIO23 | SPI1_MOSI |
| 18 | 24 | GPIO24 | SPI1_MISO |
| 19 | - | SPI1_MOSI | GPIO9 |
| 21 | - | SPI1_MISO | GPIO10 |
| 22 | 25 | GPIO25 | SPI1_SCK |
| 23 | - | SPI1_SCK | GPIO11 |
| 24 | - | SPI1_CS0 | GPIO12 |
| 26 | 12 | GPIO12 | - |
| 27 | - | I2C2_SDA | GPIO13 |
| 28 | - | I2C2_SCL | GPIO14 |
| 29 | 5 | GPIO5 | - |
| 31 | 6 | GPIO6 | - |
| 32 | - | PWM0 | GPIO0 |
| 33 | - | PWM1 | GPIO1 |
| 35 | 19 | GPIO19 | - |
| 36 | 16 | GPIO16 | - |
| 37 | 26 | GPIO26 | SPI1_CS1 |
| 38 | 20 | GPIO20 | - |
| 40 | 21 | GPIO21 | - |

## GPIO Banks and Controllers

The GPIOs are organized into banks:

GPIO Bank A: Pins 0-7 (GPIO0 - GPIO7)
GPIO Bank B: Pins 8-15 (GPIO8 - GPIO15)
GPIO Bank C: Pins 16-23 (GPIO16 - GPIO23)
GPIO Bank D: Pins 24-31 (GPIO24 - GPIO31)
text


## Register Map

Base Address: 0x2200000

| Register | Offset | Description |
|----------|--------|-------------|
| CNF | 0x00 | Configuration register |
| DAT | 0x04 | Data register |
| SET | 0x08 | Set output register |
| CLR | 0x0C | Clear output register |
| INT_EN | 0x10 | Interrupt enable |
| INT_TYPE | 0x14 | Interrupt type |
| INT_POL | 0x18 | Interrupt polarity |
| INT_STATUS | 0x1C | Interrupt status |

## Configuration Modes

### Input Mode
```cpp
GPIO gpio(18, GPIO::Direction::INPUT);
gpio.setPull(GPIO::Pull::PULL_UP);  // Enable internal pull-up
gpio.setEdge(GPIO::Edge::RISING);   // Interrupt on rising edge

Output Mode
cpp

GPIO led(18, GPIO::Direction::OUTPUT);
led.write(GPIO::Value::HIGH);
gpio.setDriveStrength(12);  // Set output drive (0-31)

Interrupt Configuration
Edge Detection

    RISING: Interrupt on 0→1 transition

    FALLING: Interrupt on 1→0 transition

    BOTH: Interrupt on any transition

    NONE: No interrupts

Debouncing

Hardware debouncing is available with configurable time:
cpp

gpio.setDebounceTime(10000);  // 10ms debounce

Performance Characteristics
Switching Speed Test
cpp

// Maximum toggle rate benchmark
void benchmark_speed() {
    GPIO pin(18, GPIO::Direction::OUTPUT);
    auto start = std::chrono::high_resolution_clock::now();
    
    for(int i = 0; i < 1000000; i++) {
        pin.write(GPIO::Value::HIGH);
        pin.write(GPIO::Value::LOW);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double freq = (2000000.0 / duration.count()) * 1e6;  // 2 edges per cycle
    std::cout << "Max frequency: " << freq / 1e6 << " MHz\n";
}

Expected Results:

    Direct register access: ~25 MHz

    sysfs interface: ~1 MHz

    libgpiod: ~5 MHz

Electrical Characteristics
Output Voltage Levels
Parameter	Min	Typ	Max
VOH (High)	2.4V	3.3V	3.5V
VOL (Low)	0V	0.1V	0.4V
IOH (Source)	-	2mA	20mA
IOL (Sink)	-	2mA	20mA
Input Voltage Levels
Parameter	Min	Typ	Max
VIH (High)	2.0V	3.3V	5.5V
VIL (Low)	0V	0V	0.8V
Hysteresis	-	0.3V	-
Advanced Features
GPIO Bit-banging
cpp

// Software SPI using GPIOs
class SoftwareSPI {
    GPIO mosi, miso, sck, cs;
    
public:
    void transfer(uint8_t byte) {
        cs.write(GPIO::Value::LOW);
        for(int i = 7; i >= 0; i--) {
            mosi.write((byte >> i) & 1 ? GPIO::Value::HIGH : GPIO::Value::LOW);
            sck.write(GPIO::Value::HIGH);
            delayMicroseconds(1);
            sck.write(GPIO::Value::LOW);
            delayMicroseconds(1);
        }
        cs.write(GPIO::Value::HIGH);
    }
};

GPIO for Protocol Decoding
cpp

// Simple logic analyzer
class LogicAnalyzer {
    std::vector<uint64_t> timestamps;
    std::vector<bool> states;
    
public:
    void capture(GPIO& pin, int duration_ms) {
        auto start = std::chrono::high_resolution_clock::now();
        bool last_state = pin.read() == GPIO::Value::HIGH;
        
        while(true) {
            bool current_state = pin.read() == GPIO::Value::HIGH;
            if(current_state != last_state) {
                auto now = std::chrono::high_resolution_clock::now();
                timestamps.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count());
                states.push_back(current_state);
                last_state = current_state;
            }
            
            if(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= duration_ms)
                break;
        }
    }
};

Troubleshooting
Common Issues

    Pin doesn't respond

        Check pin mux configuration

        Verify permissions: ls -l /dev/gpiochip*

        Test with gpioinfo command

    Intermittent readings

        Enable pull-up/pull-down

        Add external debouncing capacitor

        Check ground connections

    Slow switching speed

        Use direct register access

        Disable unnecessary kernel drivers

        Set CPU governor to performance

Debug Commands
bash

# Check GPIO chip info
gpioinfo

# Monitor GPIO changes
gpiomon gpiochip0 18

# Set GPIO value
gpioset gpiochip0 18=1

# Get GPIO value
gpioget gpiochip0 18

Best Practices

    Always use pull-up/down resistors for inputs to avoid floating states

    Add series resistors (100-330Ω) for outputs to limit current

    Use level shifters when interfacing with 5V devices

    Implement debouncing for mechanical switches

    Release GPIOs when program exits to avoid conflicts

    Check current limits when driving LEDs or loads

Example: Industrial Sensor Interface
cpp

#include <gpio.hpp>
#include <chrono>
#include <thread>

class IndustrialSensor {
    GPIO trigger_pin;
    GPIO echo_pin;
    
public:
    IndustrialSensor() : 
        trigger_pin(18, GPIO::Direction::OUTPUT),
        echo_pin(23, GPIO::Direction::INPUT) {}
    
    float measureDistance() {
        // Send 10us pulse
        trigger_pin.write(GPIO::Value::HIGH);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        trigger_pin.write(GPIO::Value::LOW);
        
        // Wait for echo
        auto start = std::chrono::high_resolution_clock::now();
        while(echo_pin.read() == GPIO::Value::LOW);
        
        auto pulse_start = std::chrono::high_resolution_clock::now();
        while(echo_pin.read() == GPIO::Value::HIGH);
        
        auto pulse_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(pulse_end - pulse_start);
        
        // Speed of sound = 343 m/s = 0.0343 cm/μs
        // Distance = (duration * 0.0343) / 2
        return duration.count() * 0.01715;
    }
};

Performance Optimization
Fast Register Access
cpp

// Memory-mapped GPIO access
class FastGPIO {
    volatile uint32_t* base_addr;
    uint32_t pin_mask;
    
public:
    FastGPIO(unsigned int pin) {
        // Map GPIO registers (requires /dev/mem access)
        int fd = open("/dev/mem", O_RDWR | O_SYNC);
        base_addr = (uint32_t*)mmap(NULL, 0x1000, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, 0x2200000);
        pin_mask = 1 << pin;
    }
    
    void set() {
        base_addr[2] = pin_mask;  // SET register
    }
    
    void clear() {
        base_addr[3] = pin_mask;  // CLR register
    }
    
    bool get() {
        return base_addr[1] & pin_mask;  // DAT register
    }
};

This comprehensive GPIO documentation covers all aspects needed for industrial and embedded development on the Jetson Orin Nano.
