#ifndef JETSON_RS485_HPP
#define JETSON_RS485_HPP

#include "uart.hpp"
#include "gpio.hpp"
#include <memory>
#include <functional>
#include <vector>
#include <cstdint>

class RS485 {
public:
    struct Config {
        int baudrate = 9600;
        bool auto_direction = true;
        unsigned int rts_pin = 0;      // GPIO pin for RTS control
        unsigned int de_pin = 0;       // Driver enable pin
        unsigned int re_pin = 0;       // Receiver enable pin
        int turn_around_delay_us = 100; // Delay after direction change
        int data_bits = 8;
        char parity = 'N';
        int stop_bits = 1;
    };
    
    // Constructor/Destructor
    RS485(const std::string& device, const Config& config);
    ~RS485();
    
    // Device operations
    bool open();
    void close();
    bool isOpen() const;
    void flush();
    
    // Data transfer
    ssize_t write(const uint8_t* data, size_t len);
    ssize_t write(const std::vector<uint8_t>& data);
    ssize_t read(uint8_t* buffer, size_t len, int timeout_ms = 100);
    ssize_t read(std::vector<uint8_t>& buffer, size_t max_len, int timeout_ms = 100);
    
    // Direction control
    void setTransmitMode();
    void setReceiveMode();
    void setAutoDirection(bool enable);
    bool isTransmitting() const { return transmitting_; }
    
    // Modbus RTU specific
    ssize_t modbusWrite(const uint8_t* data, size_t len);
    ssize_t modbusRead(uint8_t* buffer, size_t max_len, int timeout_ms = 100);
    uint16_t calculateCRC(const uint8_t* data, size_t len);
    bool verifyCRC(const uint8_t* data, size_t len);
    
    // Multi-drop addressing
    void setSlaveAddress(uint8_t address);
    uint8_t getSlaveAddress() const { return slave_address_; }
    bool sendToSlave(uint8_t address, const uint8_t* data, size_t len);
    
    // Configuration
    void setConfig(const Config& config);
    Config getConfig() const;
    void setBaudrate(int baudrate);
    void setTurnAroundDelay(int delay_us);
    
    // Error handling
    enum class Error {
        NONE,
        TIMEOUT,
        CRC_ERROR,
        FRAMING_ERROR,
        BUS_CONTENTION
    };
    
    struct Statistics {
        uint32_t frames_sent = 0;
        uint32_t frames_received = 0;
        uint32_t crc_errors = 0;
        uint32_t timeout_errors = 0;
        uint32_t framing_errors = 0;
        uint32_t bus_contentions = 0;
    };
    
    Statistics getStatistics() const { return stats_; }
    void resetStatistics();
    Error getLastError() const { return last_error_; }
    
    // Callback for slave mode
    using FrameCallback = std::function<void(uint8_t address, const uint8_t* data, size_t len)>;
    void startSlaveMode(FrameCallback callback);
    void stopSlaveMode();
    
private:
    std::unique_ptr<UART> uart_;
    std::unique_ptr<GPIO> rts_pin_;
    std::unique_ptr<GPIO> de_pin_;
    std::unique_ptr<GPIO> re_pin_;
    Config config_;
    bool auto_direction_;
    bool transmitting_;
    uint8_t slave_address_;
    Statistics stats_;
    Error last_error_;
    FrameCallback slave_callback_;
    std::thread slave_thread_;
    std::atomic<bool> slave_running_;
    
    void enableTransmit();
    void enableReceive();
    void delayMicroseconds(int us);
    void slaveLoop();
};

#endif // JETSON_RS485_HPP
