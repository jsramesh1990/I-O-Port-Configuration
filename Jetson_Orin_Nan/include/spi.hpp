#ifndef JETSON_SPI_HPP
#define JETSON_SPI_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>
#include <linux/spi/spidev.h>

class SPI {
public:
    enum class Mode {
        MODE_0 = SPI_MODE_0,  // CPOL=0, CPHA=0
        MODE_1 = SPI_MODE_1,  // CPOL=0, CPHA=1
        MODE_2 = SPI_MODE_2,  // CPOL=1, CPHA=0
        MODE_3 = SPI_MODE_3   // CPOL=1, CPHA=1
    };
    
    enum class BitOrder {
        MSB_FIRST = 0,
        LSB_FIRST = 1
    };
    
    struct Config {
        Mode mode = Mode::MODE_0;
        uint32_t speed = 1000000;      // Hz
        uint8_t bits_per_word = 8;
        BitOrder bit_order = BitOrder::MSB_FIRST;
        uint16_t delay_us = 0;
        bool cs_change = false;
        uint8_t cs_pin = 0;            // 0 = CS0, 1 = CS1
        bool lsb_first = false;
        bool loopback = false;
        bool no_cs = false;
        bool ready = false;
        bool tx_dual = false;
        bool tx_quad = false;
        bool rx_dual = false;
        bool rx_quad = false;
    };
    
    // Constructor/Destructor
    SPI(const std::string& device, Mode mode, uint32_t speed);
    explicit SPI(const std::string& device);
    ~SPI();
    
    // Device operations
    bool open();
    void close();
    bool isOpen() const { return fd_ >= 0; }
    
    // Basic transfer
    int transfer(const uint8_t* tx, uint8_t* rx, size_t length);
    int transfer(const std::vector<uint8_t>& tx, std::vector<uint8_t>& rx);
    int write(const uint8_t* data, size_t length);
    int read(uint8_t* data, size_t length);
    uint8_t transferByte(uint8_t data);
    uint16_t transferWord(uint16_t data);
    
    // Multi-transfer (for CS change between transfers)
    int transferMultiple(const std::vector<std::vector<uint8_t>>& transfers,
                        std::vector<std::vector<uint8_t>>& responses);
    
    // Configuration
    void setMode(Mode mode);
    void setSpeed(uint32_t speed);
    void setBitsPerWord(uint8_t bits);
    void setBitOrder(BitOrder order);
    void setDelay(uint16_t delay_us);
    void setConfig(const Config& config);
    Config getConfig() const;
    
    // DMA operations
    void enableDMA(bool enable);
    ssize_t dmaTransfer(const uint8_t* tx, uint8_t* rx, size_t length);
    
    // Async operations
    using TransferCallback = std::function<void(const uint8_t*, size_t)>;
    void startAsyncRead(TransferCallback callback);
    void stopAsyncRead();
    
    // GPIO CS control (manual chip select)
    void setManualCS(bool enable, unsigned int cs_pin = 0);
    void assertCS();
    void deassertCS();
    
    // Utility
    std::string getDevice() const { return device_; }
    int getFileDescriptor() const { return fd_; }
    uint32_t getMaxSpeed() const;
    
    // Static methods
    static std::vector<std::string> getAvailableDevices();
    static bool testLoopback(const std::string& device, uint32_t speed = 1000000);
    
private:
    std::string device_;
    int fd_;
    Config config_;
    bool manual_cs_;
    std::unique_ptr<GPIO> cs_gpio_;
    bool dma_enabled_;
    std::thread async_thread_;
    std::atomic<bool> async_running_;
    TransferCallback async_callback_;
    
    void configure();
    void selectChip();
    void deselectChip();
    void asyncReadLoop();
    void setupGPIOForCS(unsigned int cs_pin);
};

#endif // JETSON_SPI_HPP
