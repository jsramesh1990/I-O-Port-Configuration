#include "gpio.hpp"
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <cerrno>

// Static member initialization
volatile uint32_t* GPIO::gpio_base_ = nullptr;

GPIO::GPIO(unsigned int pin, Direction direction, Value initial)
    : pin_(pin), direction_(direction), edge_(Edge::NONE), pull_(Pull::DISABLE),
      interrupts_enabled_(false), interrupt_running_(false), chip_(nullptr), line_(nullptr) {
    
    initChip();
    configureLine();
    setDirection(direction);
    
    if (direction == Direction::OUTPUT) {
        write(initial);
    }
}

GPIO::~GPIO() {
    disableInterrupts();
    releaseLine();
    if (chip_) {
        gpiod_chip_close(chip_);
    }
}

void GPIO::initChip() {
    chip_name_ = pinToGpioChip(pin_);
    chip_ = gpiod_chip_open_by_name(chip_name_.c_str());
    if (!chip_) {
        throw std::runtime_error("Failed to open GPIO chip: " + chip_name_);
    }
    
    line_ = gpiod_chip_get_line(chip_, pin_);
    if (!line_) {
        throw std::runtime_error("Failed to get GPIO line: " + std::to_string(pin_));
    }
}

void GPIO::configureLine() {
    struct gpiod_line_request_config config;
    memset(&config, 0, sizeof(config));
    
    config.consumer = "jetson_peripheral";
    
    if (direction_ == Direction::INPUT) {
        config.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
    } else {
        config.request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT;
    }
    
    if (gpiod_line_request(line_, &config, 0) < 0) {
        throw std::runtime_error("Failed to request GPIO line");
    }
    
    // Configure pull-up/pull-down
    if (pull_ != Pull::DISABLE) {
        int flags = 0;
        if (pull_ == Pull::PULL_UP) flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;
        else flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN;
        gpiod_line_release(line_);
        config.flags = flags;
        gpiod_line_request(line_, &config, 0);
    }
    
    initFastAccess();
    pin_mask_ = 1 << (pin_ % 32);
}

void GPIO::releaseLine() {
    if (line_) {
        gpiod_line_release(line_);
        line_ = nullptr;
    }
}

void GPIO::write(Value value) {
    if (direction_ != Direction::OUTPUT) {
        throw std::runtime_error("GPIO not configured as output");
    }
    int result = gpiod_line_set_value(line_, static_cast<int>(value));
    if (result < 0) {
        throw std::runtime_error("Failed to write GPIO value");
    }
}

GPIO::Value GPIO::read() {
    int value = gpiod_line_get_value(line_);
    if (value < 0) {
        throw std::runtime_error("Failed to read GPIO value");
    }
    return static_cast<Value>(value);
}

void GPIO::toggle() {
    Value current = read();
    write(current == Value::HIGH ? Value::LOW : Value::HIGH);
}

void GPIO::setDirection(Direction direction) {
    direction_ = direction;
    gpiod_line_release(line_);
    configureLine();
}

void GPIO::setEdge(Edge edge) {
    edge_ = edge;
    if (interrupts_enabled_) {
        disableInterrupts();
        enableInterrupts();
    }
}

void GPIO::setPull(Pull pull) {
    pull_ = pull;
    gpiod_line_release(line_);
    configureLine();
}

void GPIO::setDriveStrength(DriveStrength strength) {
    // Drive strength is platform-specific
    // This is a simplified implementation
    std::string path = "/sys/kernel/debug/gpio/" + std::to_string(pin_) + "/drive";
    std::ofstream file(path);
    if (file.is_open()) {
        file << static_cast<int>(strength);
    }
}

void GPIO::setDebounceTime(uint32_t microseconds) {
    std::string path = "/sys/kernel/debug/gpio/" + std::to_string(pin_) + "/debounce";
    std::ofstream file(path);
    if (file.is_open()) {
        file << microseconds;
    }
}

void GPIO::registerCallback(Callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = callback;
}

void GPIO::unregisterCallback() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = nullptr;
}

void GPIO::enableInterrupts() {
    if (interrupts_enabled_) return;
    
    interrupts_enabled_ = true;
    interrupt_running_ = true;
    interrupt_thread_ = std::thread(&GPIO::interruptLoop, this);
}

void GPIO::disableInterrupts() {
    interrupts_enabled_ = false;
    interrupt_running_ = false;
    if (interrupt_thread_.joinable()) {
        interrupt_thread_.join();
    }
}

bool GPIO::waitForInterrupt(int timeout_ms) {
    struct gpiod_line_event event;
    int ret = gpiod_line_event_wait(line_, timeout_ms < 0 ? nullptr : 
                                    &(struct timespec){timeout_ms / 1000, (timeout_ms % 1000) * 1000000});
    if (ret > 0) {
        gpiod_line_event_read(line_, &event);
        return true;
    }
    return false;
}

void GPIO::interruptLoop() {
    while (interrupt_running_) {
        if (waitForInterrupt(100)) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (callback_) {
                callback_();
            }
        }
    }
}

void GPIO::initFastAccess() {
    if (gpio_base_) return;
    
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) return;
    
    // GPIO base address for Tegra (varies by SoC)
    gpio_base_ = (volatile uint32_t*)mmap(NULL, 0x10000, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, fd, 0x2200000);
    close(fd);
}

void GPIO::fastWrite(Value value) {
    if (!gpio_base_ || direction_ != Direction::OUTPUT) {
        write(value);
        return;
    }
    
    if (value == Value::HIGH) {
        gpio_base_[2] = pin_mask_;  // SET register
    } else {
        gpio_base_[3] = pin_mask_;  // CLR register
    }
}

GPIO::Value GPIO::fastRead() {
    if (!gpio_base_) {
        return read();
    }
    return (gpio_base_[1] & pin_mask_) ? Value::HIGH : Value::LOW;
}

bool GPIO::isPinValid(unsigned int pin) {
    return pin < getPinCount();
}

std::vector<unsigned int> GPIO::getAvailablePins() {
    std::vector<unsigned int> pins;
    for (unsigned int i = 0; i < getPinCount(); i++) {
        std::string path = "/sys/class/gpio/gpio" + std::to_string(i);
        if (access(path.c_str(), F_OK) == 0) {
            pins.push_back(i);
        }
    }
    return pins;
}

std::string GPIO::pinToGpioChip(unsigned int pin) {
    // Jetson Orin Nano GPIO mapping
    if (pin < 32) return "gpiochip0";
    if (pin < 64) return "gpiochip1";
    return "gpiochip0";
}

unsigned int GPIO::getPinCount() {
    return 128;  // Orin Nano has 128 GPIOs
}
