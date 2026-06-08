#include "spi.hpp"
#include "gpio.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cerrno>

SPI::SPI(const std::string& device, Mode mode, uint32_t speed)
    : device_(device), fd_(-1), manual_cs_(false), dma_enabled_(false), async_running_(false) {
    config_.mode = mode;
    config_.speed = speed;
}

SPI::SPI(const std::string& device)
    : SPI(device, Mode::MODE_0, 1000000) {}

SPI::~SPI() {
    close();
}

bool SPI::open() {
    fd_ = ::open(device_.c_str(), O_RDWR);
    if (fd_ < 0) {
        return false;
    }
    
    configure();
    return true;
}

void SPI::close() {
    stopAsyncRead();
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void SPI::configure() {
    // Set SPI mode
    uint8_t mode = static_cast<uint8_t>(config_.mode);
    ioctl(fd_, SPI_IOC_WR_MODE, &mode);
    ioctl(fd_, SPI_IOC_RD_MODE, &mode);
    
    // Set bits per word
    ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &config_.bits_per_word);
    ioctl(fd_, SPI_IOC_RD_BITS_PER_WORD, &config_.bits_per_word);
    
    // Set max speed
    ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &config_.speed);
    ioctl(fd_, SPI_IOC_RD_MAX_SPEED_HZ, &config_.speed);
    
    // Set LSB first if needed
    if (config_.lsb_first) {
        ioctl(fd_, SPI_IOC_WR_LSB_FIRST, &config_.lsb_first);
    }
}

void SPI::selectChip() {
    if (!manual_cs_) return;
    if (cs_gpio_) {
        cs_gpio_->write(GPIO::Value::LOW);
    }
}

void SPI::deselectChip() {
    if (!manual_cs_) return;
    if (cs_gpio_) {
        cs_gpio_->write(GPIO::Value::HIGH);
    }
}

int SPI::transfer(const uint8_t* tx, uint8_t* rx, size_t length) {
    if (fd_ < 0) return -1;
    
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = length;
    tr.speed_hz = config_.speed;
    tr.delay_usecs = config_.delay_us;
    tr.bits_per_word = config_.bits_per_word;
    tr.cs_change = config_.cs_change ? 1 : 0;
    
    if (config_.tx_dual) tr.tx_nbits = 2;
    if (config_.tx_quad) tr.tx_nbits = 4;
    if (config_.rx_dual) tr.rx_nbits = 2;
    if (config_.rx_quad) tr.rx_nbits = 4;
    
    selectChip();
    int result = ioctl(fd_, SPI_IOC_MESSAGE(1), &tr);
    deselectChip();
    
    return result;
}

int SPI::transfer(const std::vector<uint8_t>& tx, std::vector<uint8_t>& rx) {
    rx.resize(tx.size());
    return transfer(tx.data(), rx.data(), tx.size());
}

int SPI::write(const uint8_t* data, size_t length) {
    return transfer(data, nullptr, length);
}

int SPI::read(uint8_t* data, size_t length) {
    std::vector<uint8_t> tx(length, 0xFF);
    return transfer(tx.data(), data, length);
}

uint8_t SPI::transferByte(uint8_t data) {
    uint8_t rx;
    transfer(&data, &rx, 1);
    return rx;
}

uint16_t SPI::transferWord(uint16_t data) {
    uint8_t tx[2] = {static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data & 0xFF)};
    uint8_t rx[2];
    transfer(tx, rx, 2);
    return (rx[0] << 8) | rx[1];
}

int SPI::transferMultiple(const std::vector<std::vector<uint8_t>>& transfers,
                          std::vector<std::vector<uint8_t>>& responses) {
    if (fd_ < 0) return -1;
    
    std::vector<spi_ioc_transfer> tr(transfers.size());
    
    for (size_t i = 0; i < transfers.size(); i++) {
        memset(&tr[i], 0, sizeof(spi_ioc_transfer));
        tr[i].tx_buf = (unsigned long)transfers[i].data();
        tr[i].len = transfers[i].size();
        tr[i].speed_hz = config_.speed;
        tr[i].delay_usecs = config_.delay_us;
        tr[i].bits_per_word = config_.bits_per_word;
        tr[i].cs_change = (i < transfers.size() - 1) ? 1 : 0;
    }
    
    responses.resize(transfers.size());
    for (size_t i = 0; i < transfers.size(); i++) {
        responses[i].resize(transfers[i].size());
        tr[i].rx_buf = (unsigned long)responses[i].data();
    }
    
    return ioctl(fd_, SPI_IOC_MESSAGE(transfers.size()), tr.data());
}

void SPI::setMode(Mode mode) {
    config_.mode = mode;
    if (fd_ >= 0) {
        uint8_t m = static_cast<uint8_t>(mode);
        ioctl(fd_, SPI_IOC_WR_MODE, &m);
    }
}

void SPI::setSpeed(uint32_t speed) {
    config_.speed = speed;
    if (fd_ >= 0) {
        ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    }
}

void SPI::setBitsPerWord(uint8_t bits) {
    config_.bits_per_word = bits;
    if (fd_ >= 0) {
        ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits);
    }
}

void SPI::setBitOrder(BitOrder order) {
    config_.bit_order = order;
    config_.lsb_first = (order == BitOrder::LSB_FIRST);
    if (fd_ >= 0) {
        uint8_t lsb = config_.lsb_first ? 1 : 0;
        ioctl(fd_, SPI_IOC_WR_LSB_FIRST, &lsb);
    }
}

void SPI::setDelay(uint16_t delay_us) {
    config_.delay_us = delay_us;
}

void SPI::setConfig(const Config& config) {
    config_ = config;
    if (fd_ >= 0) {
        configure();
    }
}

SPI::Config SPI::getConfig() const {
    return config_;
}

void SPI::enableDMA(bool enable) {
    dma_enabled_ = enable;
    if (fd_ >= 0) {
        ioctl(fd_, SPI_IOC_ENABLE_DMA, &enable);
    }
}

ssize_t SPI::dmaTransfer(const uint8_t* tx, uint8_t* rx, size_t length) {
    if (!dma_enabled_) {
        transfer(tx, rx, length);
        return length;
    }
    
    // DMA transfer implementation
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = length;
    tr.speed_hz = config_.speed;
    tr.delay_usecs = config_.delay_us;
    tr.bits_per_word = config_.bits_per_word;
    tr.cs_change = config_.cs_change ? 1 : 0;
    tr.tx_nbits = 0;
    tr.rx_nbits = 0;
    
    selectChip();
    int result = ioctl(fd_, SPI_IOC_MESSAGE(1), &tr);
    deselectChip();
    
    return result >= 0 ? length : -1;
}

void SPI::startAsyncRead(TransferCallback callback) {
    if (async_running_) return;
    
    async_callback_ = callback;
    async_running_ = true;
    async_thread_ = std::thread(&SPI::asyncReadLoop, this);
}

void SPI::stopAsyncRead() {
    async_running_ = false;
    if (async_thread_.joinable()) {
        async_thread_.join();
    }
}

void SPI::setManualCS(bool enable, unsigned int cs_pin) {
    manual_cs_ = enable;
    if (enable && cs_pin > 0) {
        setupGPIOForCS(cs_pin);
    }
}

void SPI::assertCS() {
    selectChip();
}

void SPI::deassertCS() {
    deselectChip();
}

uint32_t SPI::getMaxSpeed() const {
    uint32_t speed;
    ioctl(fd_, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    return speed;
}

void SPI::asyncReadLoop() {
    uint8_t buffer[4096];
    
    while (async_running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        
        int result = select(fd_ + 1, &fds, NULL, NULL, &timeout);
        if (result > 0 && async_callback_) {
            ssize_t bytes = ::read(fd_, buffer, sizeof(buffer));
            if (bytes > 0) {
                async_callback_(buffer, bytes);
            }
        }
    }
}

void SPI::setupGPIOForCS(unsigned int cs_pin) {
    cs_gpio_ = std::make_unique<GPIO>(cs_pin, GPIO::Direction::OUTPUT);
    cs_gpio_->write(GPIO::Value::HIGH);
}

std::vector<std::string> SPI::getAvailableDevices() {
    std::vector<std::string> devices;
    
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            std::string path = "/dev/spidev" + std::to_string(i) + "." + std::to_string(j);
            if (access(path.c_str(), R_OK | W_OK) == 0) {
                devices.push_back(path);
            }
        }
    }
    
    return devices;
}

bool SPI::testLoopback(const std::string& device, uint32_t speed) {
    SPI spi(device, Mode::MODE_0, speed);
    if (!spi.open()) return false;
    
    // Enable loopback mode
    int loopback = 1;
    ioctl(spi.fd_, SPI_IOC_WR_LOOP, &loopback);
    
    std::vector<uint8_t> tx = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
                                0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    std::vector<uint8_t> rx;
    
    spi.transfer(tx, rx);
    
    spi.close();
    
    return tx == rx;
}
