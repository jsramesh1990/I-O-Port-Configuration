#include "uart.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <algorithm>

UART::UART(const std::string& device, int baudrate)
    : device_(device), fd_(-1), rs485_mode_(false), rs485_delay_us_(100),
      async_running_(false) {
    config_.baudrate = baudrate;
}

UART::~UART() {
    close();
}

bool UART::open() {
    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        return false;
    }
    
    // Save original termios
    tcgetattr(fd_, &original_termios_);
    
    configureTermios();
    
    if (rs485_mode_) {
        configureRS485();
    }
    
    return true;
}

void UART::close() {
    if (fd_ >= 0) {
        stopAsyncRead();
        tcsetattr(fd_, TCSANOW, &original_termios_);
        ::close(fd_);
        fd_ = -1;
    }
}

void UART::flush(bool tx, bool rx) {
    if (fd_ < 0) return;
    
    int flags = 0;
    if (tx && rx) flags = TCIOFLUSH;
    else if (tx) flags = TCOFLUSH;
    else if (rx) flags = TCIFLUSH;
    
    tcflush(fd_, flags);
}

void UART::drain() {
    if (fd_ >= 0) {
        tcdrain(fd_);
    }
}

ssize_t UART::write(const uint8_t* data, size_t len) {
    if (fd_ < 0) return -1;
    return ::write(fd_, data, len);
}

ssize_t UART::write(const std::vector<uint8_t>& data) {
    return write(data.data(), data.size());
}

ssize_t UART::writeString(const std::string& str) {
    return write(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
}

ssize_t UART::read(uint8_t* buffer, size_t len) {
    if (fd_ < 0) return -1;
    return ::read(fd_, buffer, len);
}

ssize_t UART::read(std::vector<uint8_t>& buffer, size_t max_len) {
    buffer.resize(max_len);
    ssize_t bytes = read(buffer.data(), max_len);
    if (bytes > 0) buffer.resize(bytes);
    return bytes;
}

ssize_t UART::readLine(uint8_t* buffer, size_t max_len, char delimiter) {
    if (fd_ < 0) return -1;
    
    size_t pos = 0;
    while (pos < max_len - 1) {
        ssize_t bytes = ::read(fd_, buffer + pos, 1);
        if (bytes <= 0) break;
        
        if (buffer[pos] == delimiter) {
            buffer[pos] = '\0';
            return pos;
        }
        pos++;
    }
    buffer[pos] = '\0';
    return pos;
}

std::string UART::readLine(char delimiter) {
    std::string line;
    char ch;
    
    while (true) {
        if (::read(fd_, &ch, 1) != 1) break;
        if (ch == delimiter) break;
        line += ch;
    }
    
    return line;
}

ssize_t UART::readTimeout(uint8_t* buffer, size_t len, int timeout_ms) {
    if (fd_ < 0) return -1;
    
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(fd_ + 1, &fds, NULL, NULL, &timeout);
    if (result > 0) {
        return ::read(fd_, buffer, len);
    }
    return 0;
}

ssize_t UART::writeTimeout(const uint8_t* data, size_t len, int timeout_ms) {
    if (fd_ < 0) return -1;
    
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(fd_ + 1, NULL, &fds, NULL, &timeout);
    if (result > 0) {
        return ::write(fd_, data, len);
    }
    return 0;
}

void UART::setConfig(const Config& config) {
    config_ = config;
    if (fd_ >= 0) {
        configureTermios();
    }
}

UART::Config UART::getConfig() const {
    return config_;
}

bool UART::setBaudrate(int baudrate) {
    config_.baudrate = baudrate;
    if (fd_ >= 0) {
        configureTermios();
        return true;
    }
    return false;
}

bool UART::setDataBits(int bits) {
    config_.data_bits = bits;
    if (fd_ >= 0) {
        configureTermios();
        return true;
    }
    return false;
}

bool UART::setStopBits(int bits) {
    config_.stop_bits = bits;
    if (fd_ >= 0) {
        configureTermios();
        return true;
    }
    return false;
}

bool UART::setParity(char parity) {
    config_.parity = parity;
    if (fd_ >= 0) {
        configureTermios();
        return true;
    }
    return false;
}

bool UART::setFlowControl(bool hardware, bool software) {
    config_.hardware_flow = hardware;
    config_.software_flow = software;
    if (fd_ >= 0) {
        configureTermios();
        return true;
    }
    return false;
}

bool UART::setBlocking(bool blocking) {
    if (fd_ < 0) return false;
    
    int flags = fcntl(fd_, F_GETFL, 0);
    if (blocking) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }
    fcntl(fd_, F_SETFL, flags);
    
    if (blocking) {
        config_.vmin = 1;
        config_.vtime = 0;
    } else {
        config_.vmin = 0;
        config_.vtime = 10;
    }
    configureTermios();
    
    return true;
}

bool UART::setCustomBaudrate(int baudrate) {
    struct serial_struct ss;
    if (ioctl(fd_, TIOCGSERIAL, &ss) < 0) return false;
    
    ss.flags = (ss.flags & ~ASYNC_SPD_MASK) | ASYNC_SPD_CUST;
    ss.custom_divisor = (ss.baud_base + (baudrate / 2)) / baudrate;
    
    int close_delay = ss.close_delay;
    int closing_wait = ss.closing_wait;
    
    if (ioctl(fd_, TIOCSSERIAL, &ss) < 0) return false;
    
    ss.close_delay = close_delay;
    ss.closing_wait = closing_wait;
    
    return ioctl(fd_, TIOCSSERIAL, &ss) >= 0;
}

void UART::enableRS485Mode(bool enable) {
    rs485_mode_ = enable;
    if (fd_ >= 0) {
        configureRS485();
    }
}

void UART::setRS485Delay(int delay_us) {
    rs485_delay_us_ = delay_us;
}

void UART::enableDMA(bool enable) {
    if (fd_ < 0) return;
    
    int flags;
    ioctl(fd_, TIOCMGET, &flags);
    if (enable) {
        flags |= TIOCM_DTR;  // DMA enable flag
    } else {
        flags &= ~TIOCM_DTR;
    }
    ioctl(fd_, TIOCMSET, &flags);
}

ssize_t UART::dmaWrite(const uint8_t* data, size_t len) {
    // DMA implementation would go here
    // For now, fall back to normal write
    return write(data, len);
}

ssize_t UART::dmaRead(uint8_t* buffer, size_t len) {
    // DMA implementation would go here
    // For now, fall back to normal read
    return read(buffer, len);
}

void UART::startAsyncRead(DataCallback callback) {
    if (async_running_) return;
    
    async_callback_ = callback;
    async_running_ = true;
    async_thread_ = std::thread(&UART::asyncReadLoop, this);
}

void UART::stopAsyncRead() {
    async_running_ = false;
    if (async_thread_.joinable()) {
        async_thread_.join();
    }
}

void UART::configureTermios() {
    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    
    tio.c_cflag = CLOCAL | CREAD;
    tio.c_lflag = 0;
    tio.c_oflag = 0;
    tio.c_iflag = 0;
    
    // Set baud rate
    int baud_const = baudrateToConstant(config_.baudrate);
    cfsetospeed(&tio, baud_const);
    cfsetispeed(&tio, baud_const);
    
    // Data bits
    tio.c_cflag &= ~CSIZE;
    switch (config_.data_bits) {
        case 5: tio.c_cflag |= CS5; break;
        case 6: tio.c_cflag |= CS6; break;
        case 7: tio.c_cflag |= CS7; break;
        case 8: tio.c_cflag |= CS8; break;
    }
    
    // Stop bits
    if (config_.stop_bits == 2) {
        tio.c_cflag |= CSTOPB;
    }
    
    // Parity
    if (config_.parity == 'E') {
        tio.c_cflag |= PARENB;
    } else if (config_.parity == 'O') {
        tio.c_cflag |= PARENB | PARODD;
    } else if (config_.parity == 'M') {
        tio.c_cflag |= PARENB | CMSPAR;
    } else if (config_.parity == 'S') {
        tio.c_cflag |= PARENB | PARODD | CMSPAR;
    }
    
    // Flow control
    if (config_.hardware_flow) {
        tio.c_cflag |= CRTSCTS;
    }
    if (config_.software_flow) {
        tio.c_iflag |= IXON | IXOFF;
    }
    
    // Read settings
    tio.c_cc[VMIN] = config_.vmin;
    tio.c_cc[VTIME] = config_.vtime;
    
    tcsetattr(fd_, TCSANOW, &tio);
}

void UART::configureRS485() {
    struct serial_rs485 rs485conf;
    memset(&rs485conf, 0, sizeof(rs485conf));
    
    rs485conf.flags |= SER_RS485_ENABLED;
    rs485conf.flags |= SER_RS485_RTS_ON_SEND;
    rs485conf.flags &= ~SER_RS485_RTS_AFTER_SEND;
    rs485conf.delay_rts_before_send = rs485_delay_us_;
    rs485conf.delay_rts_after_send = rs485_delay_us_;
    
    ioctl(fd_, TIOCSRS485, &rs485conf);
}

int UART::baudrateToConstant(int baudrate) {
    switch (baudrate) {
        case 50: return B50;
        case 75: return B75;
        case 110: return B110;
        case 134: return B134;
        case 150: return B150;
        case 200: return B200;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 1800: return B1800;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 500000: return B500000;
        case 576000: return B576000;
        case 921600: return B921600;
        case 1000000: return B1000000;
        case 1152000: return B1152000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
        case 2500000: return B2500000;
        case 3000000: return B3000000;
        case 3500000: return B3500000;
        case 4000000: return B4000000;
        default: return B9600;
    }
}

int UART::constantToBaudrate(int constant) {
    switch (constant) {
        case B50: return 50;
        case B75: return 75;
        case B110: return 110;
        case B134: return 134;
        case B150: return 150;
        case B200: return 200;
        case B300: return 300;
        case B600: return 600;
        case B1200: return 1200;
        case B1800: return 1800;
        case B2400: return 2400;
        case B4800: return 4800;
        case B9600: return 9600;
        case B19200: return 19200;
        case B38400: return 38400;
        case B57600: return 57600;
        case B115200: return 115200;
        case B230400: return 230400;
        case B460800: return 460800;
        case B500000: return 500000;
        case B576000: return 576000;
        case B921600: return 921600;
        case B1000000: return 1000000;
        case B1152000: return 1152000;
        case B1500000: return 1500000;
        case B2000000: return 2000000;
        case B2500000: return 2500000;
        case B3000000: return 3000000;
        case B3500000: return 3500000;
        case B4000000: return 4000000;
        default: return 9600;
    }
}

void UART::asyncReadLoop() {
    uint8_t buffer[4096];
    
    while (async_running_) {
        ssize_t bytes = readTimeout(buffer, sizeof(buffer), 100);
        if (bytes > 0 && async_callback_) {
            async_callback_(buffer, bytes);
        }
    }
}

std::string UART::getErrorString() const {
    return std::string(strerror(errno));
}

std::vector<std::string> UART::getAvailablePorts() {
    std::vector<std::string> ports;
    
    for (int i = 0; i < 10; i++) {
        std::string path = "/dev/ttyTHS" + std::to_string(i);
        if (access(path.c_str(), R_OK | W_OK) == 0) {
            ports.push_back(path);
        }
    }
    
    // Also check USB serial ports
    for (int i = 0; i < 10; i++) {
        std::string path = "/dev/ttyUSB" + std::to_string(i);
        if (access(path.c_str(), R_OK | W_OK) == 0) {
            ports.push_back(path);
        }
        
        path = "/dev/ttyACM" + std::to_string(i);
        if (access(path.c_str(), R_OK | W_OK) == 0) {
            ports.push_back(path);
        }
    }
    
    return ports;
}

bool UART::testLoopback(const std::string& device, int baudrate) {
    UART uart(device, baudrate);
    if (!uart.open()) return false;
    
    const char* test_data = "Hello, UART!";
    uart.writeString(test_data);
    
    char buffer[64];
    ssize_t bytes = uart.readTimeout(reinterpret_cast<uint8_t*>(buffer), sizeof(buffer), 100);
    
    uart.close();
    
    return bytes > 0 && memcmp(test_data, buffer, bytes) == 0;
}
