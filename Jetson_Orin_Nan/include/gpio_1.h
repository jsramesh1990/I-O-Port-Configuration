#ifndef JETSON_GPIO_HPP
#define JETSON_GPIO_HPP

#include <string>
#include <functional>
#include <memory>
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
    
    GPIO(unsigned int pin, Direction direction, Value initial = Value::LOW);
    ~GPIO();
    
    // Basic I/O operations
    void write(Value value);
    Value read();
    
    // Configuration
    void setDirection(Direction direction);
    void setEdge(Edge edge);
    void setPull(Pull pull);
    
    // Interrupt handling
    using Callback = std::function<void()>;
    void registerCallback(Callback callback);
    void waitForInterrupt(int timeout_ms = -1);
    
    // Utility
    unsigned int getPin() const { return pin_; }
    std::string getChip() const { return chip_name_; }
    
    // Static methods
    static bool isPinValid(unsigned int pin);
    static std::vector<unsigned int> getAvailablePins();
    
private:
    unsigned int pin_;
    std::string chip_name_;
    struct gpiod_chip* chip_;
    struct gpiod_line* line_;
    Callback callback_;
    
    void initChip();
    void releaseLine();
    static void interruptHandler(int fd, void* data);
};

#endif // JETSON_GPIO_HPP
