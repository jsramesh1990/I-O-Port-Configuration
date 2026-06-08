#include "rs485.hpp"
#include <cstring>
#include <chrono>

RS485::RS485(const std::string& device, const Config& config)
    : config_(config), auto_direction_(config.auto_direction),
      transmitting_(false), slave_address_(0), last_error_(Error::NONE),
      slave_running_(false) {
    
    uart_ = std::make_unique<UART>(device, config.baudrate);
    
    // Configure UART for RS485
    if (config.auto_direction) {
        uart_->enableRS485Mode(true);
        uart_->setRS485Delay(config.turn_around_delay_us);
    } else {
        // Setup GPIO pins for direction control
        if (config.rts_pin != 0) {
            rts_pin_ = std::make_unique<GPIO>(config.rts_pin, GPIO::Direction::OUTPUT);
        }
        if (config.de_pin != 0) {
            de_pin_ = std::make_unique<GPIO>(config.de_pin, GPIO::Direction::OUTPUT);
        }
        if (config.re_pin != 0) {
            re_pin_ = std::make_unique<GPIO>(config.re_pin, GPIO::Direction::OUTPUT);
        }
    }
}

RS485::~RS485() {
    close();
}

bool RS485::open() {
    if (!uart_->open()) return false;
    
    // Configure UART settings
    UART::Config uart_config;
    uart_config.baudrate = config_.baudrate;
    uart_config.data_bits = config_.data_bits;
    uart_config.parity = config_.parity;
    uart_config.stop_bits = config_.stop_bits;
    uart_->setConfig(uart_config);
    
    // Start in receive mode
    setReceiveMode();
    
    return true;
}

void RS485::close() {
    stopSlaveMode();
    uart_->close();
}

bool RS485::isOpen() const {
    return uart_->isOpen();
}

void RS485::flush() {
    uart_->flush(true, true);
}

ssize_t RS485::write(const uint8_t* data, size_t len) {
    if (!uart_->isOpen()) return -1;
    
    setTransmitMode();
    
    ssize_t written = uart_->write(data, len);
    uart_->drain();  // Wait for all data to be sent
    
    delayMicroseconds(config_.turn_around_delay_us);
    setReceiveMode();
    
    if (written > 0) {
        stats_.frames_sent++;
    }
    
    return written;
}

ssize_t RS485::write(const std::vector<uint8_t>& data) {
    return write(data.data(), data.size());
}

ssize_t RS485::read(uint8_t* buffer, size_t len, int timeout_ms) {
    if (!uart_->isOpen()) return -1;
    
    setReceiveMode();
    
    ssize_t bytes = uart_->readTimeout(buffer, len, timeout_ms);
    
    if (bytes > 0) {
        stats_.frames_received++;
    } else if (bytes == 0) {
        last_error_ = Error::TIMEOUT;
        stats_.timeout_errors++;
    }
    
    return bytes;
}

ssize_t RS485::read(std::vector<uint8_t>& buffer, size_t max_len, int timeout_ms) {
    buffer.resize(max_len);
    ssize_t bytes = read(buffer.data(), max_len, timeout_ms);
    if (bytes > 0) buffer.resize(bytes);
    return bytes;
}

void RS485::setTransmitMode() {
    if (transmitting_) return;
    
    if (auto_direction_) {
        // Hardware handles direction automatically
    } else if (rts_pin_) {
        rts_pin_->write(GPIO::Value::HIGH);
    } else if (de_pin_) {
        de_pin_->write(GPIO::Value::HIGH);
        if (re_pin_) re_pin_->write(GPIO::Value::HIGH);
    }
    
    transmitting_ = true;
    delayMicroseconds(config_.turn_around_delay_us);
}

void RS485::setReceiveMode() {
    if (!transmitting_) return;
    
    if (auto_direction_) {
        // Hardware handles direction automatically
    } else if (rts_pin_) {
        rts_pin_->write(GPIO::Value::LOW);
    } else if (de_pin_) {
        de_pin_->write(GPIO::Value::LOW);
        if (re_pin_) re_pin_->write(GPIO::Value::LOW);
    }
    
    transmitting_ = false;
}

void RS485::setAutoDirection(bool enable) {
    auto_direction_ = enable;
    uart_->enableRS485Mode(enable);
}

void RS485::delayMicroseconds(int us) {
    usleep(us);
}

ssize_t RS485::modbusWrite(const uint8_t* data, size_t len) {
    // Add CRC to Modbus frame
    std::vector<uint8_t> frame(data, data + len);
    uint16_t crc = calculateCRC(data, len);
    frame.push_back(crc & 0xFF);
    frame.push_back((crc >> 8) & 0xFF);
    
    return write(frame.data(), frame.size());
}

ssize_t RS485::modbusRead(uint8_t* buffer, size_t max_len, int timeout_ms) {
    ssize_t bytes = read(buffer, max_len, timeout_ms);
    
    if (bytes > 2) {
        if (!verifyCRC(buffer, bytes)) {
            last_error_ = Error::CRC_ERROR;
            stats_.crc_errors++;
            return -1;
        }
        bytes -= 2;  // Remove CRC from length
    }
    
    return bytes;
}

uint16_t RS485::calculateCRC(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

bool RS485::verifyCRC(const uint8_t* data, size_t len) {
    if (len < 2) return false;
    
    uint16_t received_crc = data[len - 2] | (data[len - 1] << 8);
    uint16_t calculated_crc = calculateCRC(data, len - 2);
    
    return received_crc == calculated_crc;
}

void RS485::setSlaveAddress(uint8_t address) {
    slave_address_ = address;
}

bool RS485::sendToSlave(uint8_t address, const uint8_t* data, size_t len) {
    if (address == slave_address_) {
        // This is a message for us
        return false;
    }
    
    return write(data, len);
}

void RS485::setConfig(const Config& config) {
    config_ = config;
    uart_->setBaudrate(config.baudrate);
    uart_->setDataBits(config.data_bits);
    uart_->setParity(config.parity);
    uart_->setStopBits(config.stop_bits);
    
    if (config.auto_direction) {
        uart_->enableRS485Mode(true);
        uart_->setRS485Delay(config.turn_around_delay_us);
    }
}

RS485::Config RS485::getConfig() const {
    return config_;
}

void RS485::setBaudrate(int baudrate) {
    config_.baudrate = baudrate;
    uart_->setBaudrate(baudrate);
}

void RS485::setTurnAroundDelay(int delay_us) {
    config_.turn_around_delay_us = delay_us;
    if (auto_direction_) {
        uart_->setRS485Delay(delay_us);
    }
}

void RS485::resetStatistics() {
    stats_ = Statistics();
}

void RS485::startSlaveMode(FrameCallback callback) {
    if (slave_running_) return;
    
    slave_callback_ = callback;
    slave_running_ = true;
    slave_thread_ = std::thread(&RS485::slaveLoop, this);
}

void RS485::stopSlaveMode() {
    slave_running_ = false;
    if (slave_thread_.joinable()) {
        slave_thread_.join();
    }
}

void RS485::slaveLoop() {
    uint8_t buffer[256];
    
    while (slave_running_) {
        ssize_t bytes = read(buffer, sizeof(buffer), 100);
        
        if (bytes > 0 && bytes >= 2) {
            uint8_t address = buffer[0];
            
            if (address == slave_address_ || address == 0) {
                // Message is for us (0 is broadcast)
                if (slave_callback_) {
                    slave_callback_(address, buffer + 1, bytes - 1);
                }
            }
        }
    }
}

void RS485::enableTransmit() {
    setTransmitMode();
}

void RS485::enableReceive() {
    setReceiveMode();
}
