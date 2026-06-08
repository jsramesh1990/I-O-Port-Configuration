#ifndef JETSON_SPI_HPP
#define JETSON_SPI_HPP

#include <string>
#include <vector>
#include <cstdint>

class SPI {
public:
    enum class Mode {
        MODE_0 = 0,  // CPOL=0, CPHA=0
        MODE_1 = 1,  // CPOL=0, CPHA=1
        MODE_2 = 2,  // CPOL=1, CPHA=0
        MODE_3 = 3   // CPOL=1, CPHA=1
    };
    
    struct Config {
        Mode mode = Mode::MODE_0;
        uint32_t speed = 1000000;  // Hz
        uint8_t bits_per_word = 8;
        bool lsb_first = false;
        bool cs_change = false;
        uint8_t cs_pin = 0;  // 0 = CS0, 1 = CS1
    };
    
    SPI(const std::string& device, Mode mode, uint32_t speed);
    explicit SPI(const std::string& device);  // Use defaults
    ~SPI();
    
    bool open();
    void close();
    
    // Transfer operations
    int transfer(const uint8_t* tx, uint8_t* rx, size_t length);
    int transfer(const std::vector<uint8_t>& tx, std::vector<uint8_t>& rx);
    int write(const uint8_t* data, size_t length);
    int read(uint8_t* data, size_t length);
    
    // Configuration
    void setMode(Mode mode);
    void setSpeed(uint32_t speed);
    void setBitsPerWord(uint8_t bits);
    void setConfig(const Config& config);
    
    // Advanced features
    void setDelay(uint16_t delay_us);  // Inter-frame delay
    bool setMaxSpeed(uint32_t max_speed);
    
    std::string getDevice() const { return device_; }
    
private:
    std::string device_;
    int fd_;
    Config config_;
    
    void configure();
    void selectChip();
    void deselectChip();
};

#endif // JETSON_SPI_HPP
