#ifndef JETSON_UART_HPP
#define JETSON_UART_HPP

#include <string>
#include <vector>
#include <termios.h>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>

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
    
    // Constructor/Destructor
    UART(const std::string& device, int baudrate = 115200);
    ~UART();
    
    // Device operations
    bool open();
    void close();
    bool isOpen() const { return fd_ >= 0; }
    void flush(bool tx = true, bool rx = true);
    void drain();
    
    // Read/Write operations
    ssize_t write(const uint8_t* data, size_t len);
    ssize_t write(const std::vector<uint8_t>& data);
    ssize_t writeString(const std::string& str);
    ssize_t read(uint8_t* buffer, size_t len);
    ssize_t read(std::vector<uint8_t>& buffer, size_t max_len);
    ssize_t readLine(uint8_t* buffer, size_t max_len, char delimiter = '\n');
    std::string readLine(char delimiter = '\n');
    
    // Timed operations
    ssize_t readTimeout(uint8_t* buffer, size_t len, int timeout_ms);
    ssize_t writeTimeout(const uint8_t* data, size_t len, int timeout_ms);
    
    // Configuration
    void setConfig(const Config& config);
    Config getConfig() const;
    bool setBaudrate(int baudrate);
    bool setDataBits(int bits);
    bool setStopBits(int bits);
    bool setParity(char parity);
    bool setFlowControl(bool hardware, bool software);
    bool setBlocking(bool blocking);
    bool setCustomBaudrate(int baudrate);
    
    // RS485 mode
    void enableRS485Mode(bool enable);
    void setRS485Delay(int delay_us);
    
    // DMA operations
    void enableDMA(bool enable);
    ssize_t dmaWrite(const uint8_t* data, size_t len);
    ssize_t dmaRead(uint8_t* buffer, size_t len);
    
    // Async operations
    using DataCallback = std::function<void(const uint8_t*, size_t)>;
    void startAsyncRead(DataCallback callback);
    void stopAsyncRead();
    
    // Utility
    std::string getDevice() const { return device_; }
    int getFileDescriptor() const { return fd_; }
    std::string getErrorString() const;
    
    // Static methods
    static std::vector<std::string> getAvailablePorts();
    static bool testLoopback(const std::string& device, int baudrate = 115200);
    
private:
    std::string device_;
    int fd_;
    Config config_;
    struct termios original_termios_;
    bool rs485_mode_;
    int rs485_delay_us_;
    std::thread async_thread_;
    std::atomic<bool> async_running_;
    DataCallback async_callback_;
    
    void configureTermios();
    void configureRS485();
    int baudrateToConstant(int baudrate);
    int constantToBaudrate(int constant);
    void asyncReadLoop();
};

#endif // JETSON_UART_HPP
