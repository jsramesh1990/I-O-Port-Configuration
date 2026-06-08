#ifndef JETSON_RS485_HPP
#define JETSON_RS485_HPP

#include "uart.hpp"
#include "gpio.hpp"
#include <memory>

class RS485 {
public:
    struct Config {
        int baudrate = 9600;
        bool auto_direction = true;
        unsigned int rts_pin = 0;  // GPIO pin for RTS control
        int turn_around_delay_us = 100;  // Delay after direction change
    };
    
    RS485(const std::string& device, const Config& config);
    ~RS485();
    
    bool open();
    void close();
    
    // Modbus RTU compatible
    ssize_t write(const uint8_t* data, size_t len);
    ssize_t read(uint8_t* buffer, size_t len, int timeout_ms = 100);
    
    // Direction control
    void setTransmitMode();
    void setReceiveMode();
    void setAutoDirection(bool enable);
    
    // Utility
    bool isOpen() const;
    void flush();
    
private:
    std::unique_ptr<UART> uart_;
    std::unique_ptr<GPIO> rts_pin_;
    Config config_;
    bool auto_direction_;
    
    void enableTransmit();
    void enableReceive();
    void delayMicroseconds(int us);
};

#endif // JETSON_RS485_HPP
