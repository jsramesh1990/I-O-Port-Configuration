#ifndef JETSON_UART_HPP
#define JETSON_UART_HPP

#include <string>
#include <termios.h>

class UART {
public:
    struct Config {
        int baudrate = 115200;
        int data_bits = 8;
        int stop_bits = 1;
        char parity = 'N';  // N, E, O, M, S
        bool hardware_flow = false;
        bool software_flow = false;
        int vmin = 0;
        int vtime = 0;
    };
    
    UART(const std::string& device, int baudrate = 115200);
    ~UART();
    
    bool open();
    void close();
    bool isOpen() const { return fd_ >= 0; }
    
    // Read/write operations
    ssize_t write(const uint8_t* data, size_t len);
    ssize_t read(uint8_t* buffer, size_t len);
    ssize_t readLine(uint8_t* buffer, size_t max_len, char delimiter = '\n');
    
    // Configuration
    void setConfig(const Config& config);
    Config getConfig() const;
    void flush(bool tx = true, bool rx = true);
    void drain();
    
    // Advanced features
    bool setBaudrate(int baudrate);
    bool setBlocking(bool blocking);
    
    std::string getDevice() const { return device_; }
    
private:
    std::string device_;
    int fd_;
    Config config_;
    struct termios original_termios_;
    
    void configureTermios();
    int baudrateToConstant(int baudrate);
};

#endif // JETSON_UART_HPP
