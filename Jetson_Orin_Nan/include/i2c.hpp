#ifndef JETSON_I2C_HPP
#define JETSON_I2C_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <map>

class I2C {
public:
    enum class Speed {
        STANDARD = 100000,    // 100 kHz
        FAST = 400000,        // 400 kHz
        FAST_PLUS = 1000000   // 1 MHz
    };
    
    enum class TransactionType {
        WRITE,
        READ,
        WRITE_READ
    };
    
    struct Transaction {
        TransactionType type;
        uint8_t* data;
        size_t len;
    };
    
    // Constructor/Destructor
    I2C(const std::string& device, uint8_t slave_address = 0x00);
    ~I2C();
    
    // Device operations
    bool open();
    void close();
    bool isOpen() const { return fd_ >= 0; }
    bool setSlaveAddress(uint8_t address);
    
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
    
    // Combined transactions
    int executeTransactions(const std::vector<Transaction>& transactions);
    
    // SMBus operations
    int8_t smbusQuickCommand(bool read = false);
    int8_t smbusReceiveByte();
    bool smbusSendByte(uint8_t command);
    int8_t smbusReadByteData(uint8_t command);
    bool smbusWriteByteData(uint8_t command, uint8_t value);
    int16_t smbusReadWordData(uint8_t command);
    bool smbusWriteWordData(uint8_t command, uint16_t value);
    int smbusReadBlockData(uint8_t command, uint8_t* data, size_t max_len);
    bool smbusWriteBlockData(uint8_t command, const uint8_t* data, size_t len);
    
    // Configuration
    void setSpeed(Speed speed);
    void setRetries(int retries);
    void setTimeout(int timeout_ms);
    void setClockStretch(bool enable);
    Speed getSpeed() const { return speed_; }
    
    // Device scanning
    std::vector<uint8_t> scanBus();
    std::map<uint8_t, std::string> scanWithIdentification();
    
    // Device detection
    bool probeDevice(uint8_t address);
    std::string identifyDevice(uint8_t address);
    
    // Utility
    uint8_t getSlaveAddress() const { return slave_address_; }
    std::string getDevice() const { return device_; }
    int getFileDescriptor() const { return fd_; }
    
    // Error handling
    enum class Error {
        NONE,
        BUS_BUSY,
        ARBITRATION_LOST,
        NACK,
        TIMEOUT,
        INVALID_PARAM
    };
    
    Error getLastError() const { return last_error_; }
    std::string errorToString(Error error) const;
    
    // Static methods
    static std::vector<std::string> getAvailableBuses();
    static bool isBusAvailable(const std::string& device);
    
private:
    std::string device_;
    int fd_;
    uint8_t slave_address_;
    Speed speed_;
    int retries_;
    int timeout_ms_;
    bool clock_stretch_;
    Error last_error_;
    
    bool setSlave(uint8_t address);
    bool waitForBus();
    void recoverBus();
    ssize_t i2c_smbus_access(uint8_t read_write, uint8_t command,
                             int size, union i2c_smbus_data* data);
};

#endif // JETSON_I2C_HPP
