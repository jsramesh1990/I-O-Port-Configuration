#include "i2c.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <cstring>
#include <cerrno>

I2C::I2C(const std::string& device, uint8_t slave_address)
    : device_(device), fd_(-1), slave_address_(slave_address),
      speed_(Speed::STANDARD), retries_(3), timeout_ms_(1000),
      clock_stretch_(true), last_error_(Error::NONE) {}

I2C::~I2C() {
    close();
}

bool I2C::open() {
    fd_ = ::open(device_.c_str(), O_RDWR);
    if (fd_ < 0) {
        last_error_ = Error::BUS_BUSY;
        return false;
    }
    
    // Set slave address
    if (!setSlave(slave_address_)) {
        close();
        return false;
    }
    
    // Set retries
    ioctl(fd_, I2C_RETRIES, retries_);
    
    // Set timeout
    ioctl(fd_, I2C_TIMEOUT, timeout_ms_);
    
    return true;
}

void I2C::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool I2C::setSlaveAddress(uint8_t address) {
    slave_address_ = address;
    if (fd_ >= 0) {
        return setSlave(address);
    }
    return true;
}

bool I2C::setSlave(uint8_t address) {
    if (ioctl(fd_, I2C_SLAVE, address) < 0) {
        last_error_ = Error::NACK;
        return false;
    }
    return true;
}

int I2C::writeByte(uint8_t reg, uint8_t data) {
    uint8_t buffer[2] = {reg, data};
    return writeRaw(buffer, 2);
}

int I2C::readByte(uint8_t reg, uint8_t* data) {
    return writeReadRaw(&reg, 1, data, 1);
}

int I2C::writeWord(uint8_t reg, uint16_t data) {
    uint8_t buffer[3] = {reg, static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data & 0xFF)};
    return writeRaw(buffer, 3);
}

int I2C::readWord(uint8_t reg, uint16_t* data) {
    uint8_t buffer[2];
    int result = writeReadRaw(&reg, 1, buffer, 2);
    if (result == 2) {
        *data = (buffer[0] << 8) | buffer[1];
    }
    return result;
}

int I2C::writeBlock(uint8_t reg, const uint8_t* data, size_t len) {
    uint8_t* buffer = new uint8_t[len + 1];
    buffer[0] = reg;
    memcpy(buffer + 1, data, len);
    
    int result = writeRaw(buffer, len + 1);
    delete[] buffer;
    return result;
}

int I2C::readBlock(uint8_t reg, uint8_t* data, size_t len) {
    return writeReadRaw(&reg, 1, data, len);
}

int I2C::writeRaw(const uint8_t* data, size_t len) {
    if (fd_ < 0) return -1;
    
    if (!waitForBus()) {
        last_error_ = Error::BUS_BUSY;
        return -1;
    }
    
    ssize_t bytes = ::write(fd_, data, len);
    if (bytes != static_cast<ssize_t>(len)) {
        last_error_ = Error::NACK;
        return -1;
    }
    
    return bytes;
}

int I2C::readRaw(uint8_t* data, size_t len) {
    if (fd_ < 0) return -1;
    
    ssize_t bytes = ::read(fd_, data, len);
    if (bytes != static_cast<ssize_t>(len)) {
        last_error_ = Error::NACK;
        return -1;
    }
    
    return bytes;
}

int I2C::writeReadRaw(const uint8_t* write_data, size_t write_len,
                      uint8_t* read_data, size_t read_len) {
    if (fd_ < 0) return -1;
    
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data packets;
    
    msgs[0].addr = slave_address_;
    msgs[0].flags = 0;
    msgs[0].len = write_len;
    msgs[0].buf = const_cast<uint8_t*>(write_data);
    
    msgs[1].addr = slave_address_;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = read_len;
    msgs[1].buf = read_data;
    
    packets.msgs = msgs;
    packets.nmsgs = 2;
    
    if (ioctl(fd_, I2C_RDWR, &packets) < 0) {
        last_error_ = Error::NACK;
        return -1;
    }
    
    return read_len;
}

int I2C::executeTransactions(const std::vector<Transaction>& transactions) {
    if (fd_ < 0) return -1;
    
    std::vector<struct i2c_msg> msgs(transactions.size());
    
    for (size_t i = 0; i < transactions.size(); i++) {
        msgs[i].addr = slave_address_;
        msgs[i].flags = (transactions[i].type == TransactionType::READ) ? I2C_M_RD : 0;
        msgs[i].len = transactions[i].len;
        msgs[i].buf = transactions[i].data;
    }
    
    struct i2c_rdwr_ioctl_data packets;
    packets.msgs = msgs.data();
    packets.nmsgs = msgs.size();
    
    if (ioctl(fd_, I2C_RDWR, &packets) < 0) {
        last_error_ = Error::NACK;
        return -1;
    }
    
    return 0;
}

int8_t I2C::smbusQuickCommand(bool read) {
    return i2c_smbus_access(read ? I2C_SMBUS_READ : I2C_SMBUS_WRITE,
                            0, I2C_SMBUS_QUICK, nullptr);
}

int8_t I2C::smbusReceiveByte() {
    union i2c_smbus_data data;
    int result = i2c_smbus_access(I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data);
    return result < 0 ? -1 : (data.byte & 0xFF);
}

bool I2C::smbusSendByte(uint8_t command) {
    union i2c_smbus_data data;
    data.byte = command;
    return i2c_smbus_access(I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE, &data) >= 0;
}

int8_t I2C::smbusReadByteData(uint8_t command) {
    union i2c_smbus_data data;
    int result = i2c_smbus_access(I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA, &data);
    return result < 0 ? -1 : (data.byte & 0xFF);
}

bool I2C::smbusWriteByteData(uint8_t command, uint8_t value) {
    union i2c_smbus_data data;
    data.byte = value;
    return i2c_smbus_access(I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA, &data) >= 0;
}

int16_t I2C::smbusReadWordData(uint8_t command) {
    union i2c_smbus_data data;
    int result = i2c_smbus_access(I2C_SMBUS_READ, command, I2C_SMBUS_WORD_DATA, &data);
    return result < 0 ? -1 : (data.word & 0xFFFF);
}

bool I2C::smbusWriteWordData(uint8_t command, uint16_t value) {
    union i2c_smbus_data data;
    data.word = value;
    return i2c_smbus_access(I2C_SMBUS_WRITE, command, I2C_SMBUS_WORD_DATA, &data) >= 0;
}

int I2C::smbusReadBlockData(uint8_t command, uint8_t* data, size_t max_len) {
    union i2c_smbus_data smbus_data;
    int result = i2c_smbus_access(I2C_SMBUS_READ, command, I2C_SMBUS_BLOCK_DATA, &smbus_data);
    
    if (result < 0) return -1;
    
    size_t len = smbus_data.block[0];
    if (len > max_len) len = max_len;
    memcpy(data, smbus_data.block + 1, len);
    
    return len;
}

bool I2C::smbusWriteBlockData(uint8_t command, const uint8_t* data, size_t len) {
    if (len > 32) return false;
    
    union i2c_smbus_data smbus_data;
    smbus_data.block[0] = len;
    memcpy(smbus_data.block + 1, data, len);
    
    return i2c_smbus_access(I2C_SMBUS_WRITE, command, I2C_SMBUS_BLOCK_DATA, &smbus_data) >= 0;
}

void I2C::setSpeed(Speed speed) {
    speed_ = speed;
    // Speed is usually set via device tree or ioctl
    if (fd_ >= 0) {
        unsigned long funcs;
        ioctl(fd_, I2C_FUNCS, &funcs);
        // Set bus speed if supported
    }
}

void I2C::setRetries(int retries) {
    retries_ = retries;
    if (fd_ >= 0) {
        ioctl(fd_, I2C_RETRIES, retries);
    }
}

void I2C::setTimeout(int timeout_ms) {
    timeout_ms_ = timeout_ms;
    if (fd_ >= 0) {
        ioctl(fd_, I2C_TIMEOUT, timeout_ms);
    }
}

void I2C::setClockStretch(bool enable) {
    clock_stretch_ = enable;
}

std::vector<uint8_t> I2C::scanBus() {
    std::vector<uint8_t> devices;
    
    if (fd_ < 0) return devices;
    
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (setSlave(addr)) {
            if (smbusQuickCommand(true) >= 0) {
                devices.push_back(addr);
            }
        }
        usleep(10000);
    }
    
    // Restore original address
    setSlave(slave_address_);
    
    return devices;
}

std::map<uint8_t, std::string> I2C::scanWithIdentification() {
    std::map<uint8_t, std::string> devices;
    
    auto addresses = scanBus();
    for (uint8_t addr : addresses) {
        devices[addr] = identifyDevice(addr);
    }
    
    return devices;
}

bool I2C::probeDevice(uint8_t address) {
    uint8_t old_addr = slave_address_;
    bool found = setSlave(address) && smbusQuickCommand(true) >= 0;
    setSlave(old_addr);
    return found;
}

std::string I2C::identifyDevice(uint8_t address) {
    // Try to read device ID from common registers
    uint8_t old_addr = slave_address_;
    setSlave(address);
    
    std::string id;
    
    // Check for common EEPROM (0x50-0x57)
    if (address >= 0x50 && address <= 0x57) {
        id = "EEPROM";
    }
    // Check for common sensors
    else if (address == 0x18 || address == 0x19) {
        id = "Accelerometer (MPU6050?)";
    }
    else if (address == 0x40 || address == 0x41) {
        id = "Temperature/Humidity (Si7021?)";
    }
    else if (address == 0x68 || address == 0x69) {
        id = "RTC (DS3231?)";
    }
    else if (address == 0x76 || address == 0x77) {
        id = "Pressure (BME280?)";
    }
    else if (address == 0x48 || address == 0x49 || address == 0x4A || address == 0x4B) {
        id = "ADC (ADS1115?)";
    }
    else {
        id = "Unknown device";
    }
    
    setSlave(old_addr);
    return id;
}

bool I2C::waitForBus() {
    // I2C bus is usually ready immediately
    return true;
}

void I2C::recoverBus() {
    if (fd_ < 0) return;
    
    // Generate 9 clock pulses to reset stuck bus
    for (int i = 0; i < 9; i++) {
        ioctl(fd_, I2C_PEC, 0);
        usleep(10);
    }
}

ssize_t I2C::i2c_smbus_access(uint8_t read_write, uint8_t command,
                              int size, union i2c_smbus_data* data) {
    struct i2c_smbus_ioctl_data args;
    
    args.read_write = read_write;
    args.command = command;
    args.size = size;
    args.data = data;
    
    return ioctl(fd_, I2C_SMBUS, &args);
}

std::string I2C::errorToString(Error error) const {
    switch (error) {
        case Error::NONE: return "No error";
        case Error::BUS_BUSY: return "Bus busy";
        case Error::ARBITRATION_LOST: return "Arbitration lost";
        case Error::NACK: return "No acknowledge";
        case Error::TIMEOUT: return "Timeout";
        case Error::INVALID_PARAM: return "Invalid parameter";
        default: return "Unknown error";
    }
}

std::vector<std::string> I2C::getAvailableBuses() {
    std::vector<std::string> buses;
    
    for (int i = 0; i < 10; i++) {
        std::string path = "/dev/i2c-" + std::to_string(i);
        if (access(path.c_str(), R_OK | W_OK) == 0) {
            buses.push_back(path);
        }
    }
    
    return buses;
}

bool I2C::isBusAvailable(const std::string& device) {
    int fd = ::open(device.c_str(), O_RDWR);
    if (fd < 0) return false;
    close(fd);
    return true;
}
