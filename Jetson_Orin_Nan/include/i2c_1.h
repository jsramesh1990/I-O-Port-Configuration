#ifndef JETSON_I2C_HPP
#define JETSON_I2C_HPP

#include <string>
#include <vector>
#include <cstdint>

class I2C {
public:
    enum class Speed {
        STANDARD = 100000,   // 100 kHz
        FAST = 400000,       // 400 kHz
        FAST_PLUS = 1000000  // 1 MHz
    };
    
    I2C(const std::string& device, uint8_t slave_address);
    ~I2C();
    
    bool open();
    void close();
    
    // Basic I2C operations
    int writeByte(uint8_t reg, uint8_t data);
    int readByte(uint8_t reg, uint8_t* data);
    int writeWord(uint8_t reg, uint16_t data);
    int readWord(uint8_t reg, uint16_t* data);
    int writeBlock(uint8_t reg, const uint8_t* data, size_t len);
    int readBlock(uint8_t reg, uint8_t* data, size_t len);
    
    // Raw transfers
    int writeRaw(const uint8_t* data, size_t len);
    int readRaw(uint8_t* data, size_t len);
    int writeReadRaw(const uint8_t* write_data, size_t write_len,
                     uint8_t* read_data, size_t read_len);
    
    // Configuration
    void setSlaveAddress(uint8_t address);
    void setSpeed(Speed speed);
    void setRetries(int retries);
    void setTimeout(int timeout_ms);
    
    // Device scanning
    static std::vector<uint8_t> scanBus(const std::string& device);
    
    // Utility
    uint8_t getSlaveAddress() const { return slave_address_; }
    std::string getDevice() const { return device_; }
    
private:
    std::string device_;
    int fd_;
    uint8_t slave_address_;
    Speed speed_;
    int retries_;
    int timeout_ms_;
    
    bool setSlave(uint8_t address);
};

#endif // JETSON_I2C_HPP
