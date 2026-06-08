#ifndef JETSON_GPIO_HPP
#define JETSON_GPIO_HPP

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <gpiod.h>

class GPIO {
public:
    enum class Direction {
        INPUT,
        OUTPUT
    };
    
    enum class Value {
        LOW = 0,
        HIGH = 1
    };
    
    enum class Edge {
        NONE,
        RISING,
        FALLING,
        BOTH
    };
    
    enum class Pull {
        DISABLE,
        PULL_UP,
        PULL_DOWN
    };
    
    enum class DriveStrength {
        WEAK = 0,
        MEDIUM = 1,
        STRONG = 2,
        MAX = 3
    };
    
    // Constructor/Destructor
    GPIO(unsigned int pin, Direction direction = Direction::INPUT, Value initial = Value::LOW);
    ~GPIO();
    
    // Basic I/O operations
    void write(Value value);
    Value read();
    void toggle();
    
    // Configuration
    void setDirection(Direction direction);
    void setEdge(Edge edge);
    void setPull(Pull pull);
    void setDriveStrength(DriveStrength strength);
    void setDebounceTime(uint32_t microseconds);
    
    // Interrupt handling
    using Callback = std::function<void()>;
    void registerCallback(Callback callback);
    void unregisterCallback();
    bool waitForInterrupt(int timeout_ms = -1);
    void enableInterrupts();
    void disableInterrupts();
    
    // Utility functions
    unsigned int getPin() const { return pin_; }
    std::string getChip() const { return chip_name_; }
    Direction getDirection() const { return direction_; }
    bool isInterruptEnabled() const { return interrupts_enabled_; }
    
    // Static methods
    static bool isPinValid(unsigned int pin);
    static std::vector<unsigned int> getAvailablePins();
    static std::string pinToGpioChip(unsigned int pin);
    static unsigned int getPinCount();
    
    // Fast direct register access (for high performance)
    void fastWrite(Value value);
    Value fastRead();
    
private:
    unsigned int pin_;
    std::string chip_name_;
    struct gpiod_chip* chip_;
    struct gpiod_line* line_;
    Direction direction_;
    Edge edge_;
    Pull pull_;
    bool interrupts_enabled_;
    Callback callback_;
    std::thread interrupt_thread_;
    std::atomic<bool> interrupt_running_;
    std::mutex callback_mutex_;
    
    void initChip();
    void releaseLine();
    void configureLine();
    void interruptLoop();
    static void interruptHandler(int fd, void* data);
    
    // Fast register access (memory-mapped)
    static volatile uint32_t* gpio_base_;
    static void initFastAccess();
    uint32_t pin_mask_;
};

#endif // JETSON_GPIO_HPP
