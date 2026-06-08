# I2C (Inter-Integrated Circuit)

## Overview

The Jetson Orin Nano provides multiple I2C controllers for communication with sensors, EEPROMs, RTCs, and other I2C peripherals. I2C is a multi-master, multi-slave, two-wire bus.

## Hardware Specifications

| Parameter | Value |
|-----------|-------|
| I2C Controllers | 6 (I2C1, I2C2 on header) |
| Max Speed | 1 MHz (Fast Mode Plus) |
| Standard Speeds | 100 kHz, 400 kHz, 1 MHz |
| Voltage | 3.3V |
| Pull-up Resistors | 4.7kΩ (external required) |
| Max Capacitance | 400 pF |
| Max Devices | 127 (7-bit address) |
| Clock Stretching | Supported |
| Arbitration | Multi-master supported |

## I2C Bus Mapping

| Bus | Device Node | Pins on Header | Purpose |
|-----|-------------|----------------|---------|
| I2C1 | /dev/i2c-1 | Pin 3 (SDA), Pin 5 (SCL) | General purpose |
| I2C2 | /dev/i2c-2 | Pin 27 (SDA), Pin 28 (SCL) | General purpose |
| I2C3 | /dev/i2c-3 | Internal | Camera (CSI) |
| I2C4 | /dev/i2c-4 | Internal | PMIC |
| I2C5 | /dev/i2c-5 | Internal | Audio codec |
| I2C6 | /dev/i2c-6 | Internal | Security chip |

## Pin Configuration

### I2C1 (40-pin header)

Pin 3 (I2C1_SDA) ─── Data line (open-drain, requires pull-up)
Pin 5 (I2C1_SCL) ─── Clock line (open-drain, requires pull-up)
text


### I2C2 (40-pin header)

Pin 27 (I2C2_SDA) ─── Data line
Pin 28 (I2C2_SCL) ─── Clock line
text


## Timing Specifications

### Standard Mode (100 kHz)

Parameter Min Typ Max Unit
Clock frequency - 100 - kHz
Clock low time 4.7 - - μs
Clock high time 4.0 - - μs
Start hold time 4.0 - - μs
Start setup time 4.7 - - μs
Data hold time 0 - 3.45 μs
Data setup time 250 - - ns
Stop setup time 4.0 - - μs
Bus free time 4.7 - - μs
text


### Fast Mode (400 kHz)

Parameter Min Typ Max Unit
Clock frequency - 400 - kHz
Clock low time 1.3 - - μs
Clock high time 0.6 - - μs
Start hold time 0.6 - - μs
Start setup time 0.6 - - μs
Data hold time 0 - 0.9 μs
Data setup time 100 - - ns
Stop setup time 0.6 - - μs
Bus free time 1.3 - - μs
text


### Fast Mode Plus (1 MHz)

Parameter Min Typ Max Unit
Clock frequency - 1000 - kHz
Clock low time 0.5 - - μs
Clock high time 0.26 - - μs
Start hold time 0.26 - - μs
Start setup time 0.26 - - μs
Data hold time 0 - 0.45 μs
Data setup time 50 - - ns
Stop setup time 0.26 - - μs
Bus free time 0.5 - - μs
text


## Register Map

Base Address: 0x3160000 (I2C1)

| Register | Offset | Description |
|----------|--------|-------------|
| CONFIG | 0x00 | Configuration register |
| TIMING | 0x04 | Timing configuration |
| FIFO | 0x08 | Data FIFO |
| INT_MASK | 0x0C | Interrupt mask |
| INT_STATUS | 0x10 | Interrupt status |
| TX_FIFO_LEVEL | 0x14 | TX FIFO level |
| RX_FIFO_LEVEL | 0x18 | RX FIFO level |
| PACKET_HEADER | 0x1C | Packet header |
| PACKET_STATUS | 0x20 | Packet status |
| CLK_DIV | 0x24 | Clock divider |

## Implementation

### High-Level I2C Class
```cpp
class I2C {
private:
    int fd;
    std::string device;
    uint8_t slave_addr;
    uint32_t speed;
    bool is_open;
    
    bool setSlaveAddress(uint8_t address) {
        return ioctl(fd, I2C_SLAVE, address) >= 0;
    }
    
public:
    I2C(const std::string& dev, uint32_t bus_speed = 100000) 
        : device(dev), speed(bus_speed), is_open(false) {
        fd = open(device.c_str(), O_RDWR);
        if(fd < 0) {
            throw std::runtime_error("Cannot open I2C device");
        }
        
        // Set bus speed
        ioctl(fd, I2C_FUNCS, &funcs);
        if(funcs & I2C_FUNC_I2C) {
            ioctl(fd, I2C_TIMEOUT, 1);
            ioctl(fd, I2C_RETRIES, 3);
        }
        
        is_open = true;
    }
    
    ~I2C() {
        if(is_open) {
            close(fd);
        }
    }
    
    bool selectSlave(uint8_t address) {
        slave_addr = address;
        return setSlaveAddress(address);
    }
    
    ssize_t writeByte(uint8_t reg, uint8_t data) {
        uint8_t buffer[2] = {reg, data};
        return write(fd, buffer, 2);
    }
    
    ssize_t readByte(uint8_t reg, uint8_t* data) {
        if(write(fd, &reg, 1) != 1) return -1;
        return read(fd, data, 1);
    }
    
    ssize_t writeWord(uint8_t reg, uint16_t data) {
        uint8_t buffer[3] = {reg, (data >> 8) & 0xFF, data & 0xFF};
        return write(fd, buffer, 3);
    }
    
    ssize_t readWord(uint8_t reg, uint16_t* data) {
        if(write(fd, &reg, 1) != 1) return -1;
        
        uint8_t buffer[2];
        ssize_t bytes = read(fd, buffer, 2);
        if(bytes == 2) {
            *data = (buffer[0] << 8) | buffer[1];
        }
        return bytes;
    }
    
    ssize_t writeBlock(uint8_t reg, const uint8_t* data, size_t len) {
        uint8_t* buffer = (uint8_t*)malloc(len + 1);
        buffer[0] = reg;
        memcpy(buffer + 1, data, len);
        
        ssize_t result = write(fd, buffer, len + 1);
        free(buffer);
        return result;
    }
    
    ssize_t readBlock(uint8_t reg, uint8_t* data, size_t len) {
        if(write(fd, &reg, 1) != 1) return -1;
        return read(fd, data, len);
    }
    
    // Combined write/read (for devices with register address)
    ssize_t writeRead(const uint8_t* write_data, size_t write_len,
                     uint8_t* read_data, size_t read_len) {
        struct i2c_msg msgs[2];
        struct i2c_rdwr_ioctl_data packets;
        
        msgs[0].addr = slave_addr;
        msgs[0].flags = 0;
        msgs[0].len = write_len;
        msgs[0].buf = (uint8_t*)write_data;
        
        msgs[1].addr = slave_addr;
        msgs[1].flags = I2C_M_RD;
        msgs[1].len = read_len;
        msgs[1].buf = read_data;
        
        packets.msgs = msgs;
        packets.nmsgs = 2;
        
        return ioctl(fd, I2C_RDWR, &packets);
    }
    
    std::vector<uint8_t> scanBus() {
        std::vector<uint8_t> devices;
        
        for(uint8_t addr = 0x08; addr <= 0x77; addr++) {
            if(setSlaveAddress(addr)) {
                uint8_t dummy;
                if(read(fd, &dummy, 1) >= 0) {
                    devices.push_back(addr);
                }
            }
            usleep(10000);  // Delay between scans
        }
        
        return devices;
    }
};

Advanced I2C with SMBus Support
cpp

class SMBus : public I2C {
public:
    SMBus(const std::string& dev, uint32_t speed = 100000) 
        : I2C(dev, speed) {}
    
    // SMBus Quick Command
    bool quickCommand(bool read = false) {
        struct i2c_smbus_ioctl_data args;
        args.read_write = read ? I2C_SMBUS_READ : I2C_SMBUS_WRITE;
        args.command = 0;
        args.size = I2C_SMBUS_QUICK;
        return ioctl(fd, I2C_SMBUS, &args) >= 0;
    }
    
    // SMBus Receive Byte
    int8_t receiveByte() {
        struct i2c_smbus_ioctl_data args;
        args.read_write = I2C_SMBUS_READ;
        args.command = 0;
        args.size = I2C_SMBUS_BYTE;
        
        if(ioctl(fd, I2C_SMBUS, &args) < 0) return -1;
        return args.data & 0xFF;
    }
    
    // SMBus Send Byte
    bool sendByte(uint8_t command) {
        struct i2c_smbus_ioctl_data args;
        args.read_write = I2C_SMBUS_WRITE;
        args.command = command;
        args.size = I2C_SMBUS_BYTE;
        return ioctl(fd, I2C_SMBUS, &args) >= 0;
    }
    
    // SMBus Read Byte Data
    int8_t readByteData(uint8_t command) {
        struct i2c_smbus_ioctl_data args;
        args.read_write = I2C_SMBUS_READ;
        args.command = command;
        args.size = I2C_SMBUS_BYTE_DATA;
        
        if(ioctl(fd, I2C_SMBUS, &args) < 0) return -1;
        return args.data & 0xFF;
    }
    
    // SMBus Write Byte Data
    bool writeByteData(uint8_t command, uint8_t value) {
        struct i2c_smbus_ioctl_data args;
        args.read_write = I2C_SMBUS_WRITE;
        args.command = command;
        args.size = I2C_SMBUS_BYTE_DATA;
        args.data = value;
        return ioctl(fd, I2C_SMBUS, &args) >= 0;
    }
    
    // SMBus Read Word Data
    int16_t readWordData(uint8_t command) {
        struct i2c_smbus_ioctl_data args;
        args.read_write = I2C_SMBUS_READ;
        args.command = command;
        args.size = I2C_SMBUS_WORD_DATA;
        
        if(ioctl(fd, I2C_SMBUS, &args) < 0) return -1;
        return args.data & 0xFFFF;
    }
    
    // SMBus Write Word Data
    bool writeWordData(uint8_t command, uint16_t value) {
        struct i2c_smbus_ioctl_data args;
        args.read_write = I2C_SMBUS_WRITE;
        args.command = command;
        args.size = I2C_SMBUS_WORD_DATA;
        args.data = value;
        return ioctl(fd, I2C_SMBUS, &args) >= 0;
    }
    
    // SMBus Block Read
    int readBlockData(uint8_t command, uint8_t* data, size_t max_len) {
        union i2c_smbus_data smbus_data;
        struct i2c_smbus_ioctl_data args;
        args.read_write = I2C_SMBUS_READ;
        args.command = command;
        args.size = I2C_SMBUS_BLOCK_DATA;
        args.data = &smbus_data;
        
        if(ioctl(fd, I2C_SMBUS, &args) < 0) return -1;
        
        size_t len = smbus_data.block[0];
        if(len > max_len) len = max_len;
        memcpy(data, smbus_data.block + 1, len);
        return len;
    }
    
    // SMBus Block Write
    bool writeBlockData(uint8_t command, const uint8_t* data, size_t len) {
        if(len > 32) return false;  // SMBus block max is 32 bytes
        
        union i2c_smbus_data smbus_data;
        smbus_data.block[0] = len;
        memcpy(smbus_data.block + 1, data, len);
        
        struct i2c_smbus_ioctl_data args;
        args.read_write = I2C_SMBUS_WRITE;
        args.command = command;
        args.size = I2C_SMBUS_BLOCK_DATA;
        args.data = &smbus_data;
        
        return ioctl(fd, I2C_SMBUS, &args) >= 0;
    }
};

I2C Multiplexer Support
cpp

class I2CMultiplexer {
    I2C i2c;
    uint8_t mux_address;
    uint8_t current_channel;
    
public:
    I2CMultiplexer(const std::string& dev, uint8_t mux_addr)
        : i2c(dev, 100000), mux_address(mux_addr), current_channel(0xFF) {
        i2c.selectSlave(mux_address);
    }
    
    bool selectChannel(uint8_t channel) {
        if(channel == current_channel) return true;
        
        // TCA9548A style multiplexer
        if(i2c.writeByte(0x00, 1 << channel) == 2) {
            current_channel = channel;
            return true;
        }
        return false;
    }
    
    bool performOnChannel(uint8_t channel, std::function<bool()> operation) {
        uint8_t old_channel = current_channel;
        if(!selectChannel(channel)) return false;
        
        bool result = operation();
        
        selectChannel(old_channel);
        return result;
    }
    
    std::vector<uint8_t> scanChannel(uint8_t channel) {
        std::vector<uint8_t> devices;
        
        performOnChannel(channel, [&]() {
            devices = i2c.scanBus();
            return true;
        });
        
        return devices;
    }
};

Device Drivers
BME280 Environmental Sensor
cpp

class BME280 {
    I2C i2c;
    uint8_t address;
    
    // Registers
    static constexpr uint8_t REG_ID = 0xD0;
    static constexpr uint8_t REG_RESET = 0xE0;
    static constexpr uint8_t REG_CTRL_HUM = 0xF2;
    static constexpr uint8_t REG_STATUS = 0xF3;
    static constexpr uint8_t REG_CTRL_MEAS = 0xF4;
    static constexpr uint8_t REG_CONFIG = 0xF5;
    static constexpr uint8_t REG_PRESS_MSB = 0xF7;
    static constexpr uint8_t REG_TEMP_MSB = 0xFA;
    
    // Calibration data
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t dig_H1, dig_H3;
    int16_t dig_H2, dig_H4, dig_H5, dig_H6;
    
    int32_t t_fine;
    
    void readCalibrationData() {
        // Read temperature calibration
        uint8_t data[24];
        i2c.readBlock(0x88, data, 24);
        dig_T1 = data[0] | (data[1] << 8);
        dig_T2 = data[2] | (data[3] << 8);
        dig_T3 = data[4] | (data[5] << 8);
        dig_P1 = data[6] | (data[7] << 8);
        dig_P2 = data[8] | (data[9] << 8);
        dig_P3 = data[10] | (data[11] << 8);
        dig_P4 = data[12] | (data[13] << 8);
        dig_P5 = data[14] | (data[15] << 8);
        dig_P6 = data[16] | (data[17] << 8);
        dig_P7 = data[18] | (data[19] << 8);
        dig_P8 = data[20] | (data[21] << 8);
        dig_P9 = data[22] | (data[23] << 8);
        
        // Read humidity calibration
        i2c.readByte(0xA1, &dig_H1);
        i2c.readBlock(0xE1, data, 7);
        dig_H2 = data[0] | (data[1] << 8);
        dig_H3 = data[2];
        dig_H4 = (data[3] << 4) | (data[4] & 0x0F);
        dig_H5 = (data[5] << 4) | ((data[4] >> 4) & 0x0F);
        dig_H6 = data[6];
    }
    
    int32_t compensateTemperature(int32_t adc_T) {
        int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * 
                        ((int32_t)dig_T2)) >> 11;
        int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
                         ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
                         ((int32_t)dig_T3)) >> 14;
        t_fine = var1 + var2;
        return (t_fine * 5 + 128) >> 8;
    }
    
    uint32_t compensatePressure(int32_t adc_P) {
        int64_t var1 = ((int64_t)t_fine) - 128000;
        int64_t var2 = var1 * var1 * (int64_t)dig_P6;
        var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
        var2 = var2 + (((int64_t)dig_P4) << 35);
        var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) +
               ((var1 * (int64_t)dig_P2) << 12);
        var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
        
        if(var1 == 0) return 0;
        
        int64_t p = 1048576 - adc_P;
        p = (((p << 31) - var2) * 3125) / var1;
        var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        var2 = (((int64_t)dig_P8) * p) >> 19;
        p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
        return (uint32_t)p;
    }
    
    uint32_t compensateHumidity(int32_t adc_H) {
        int32_t v_x1_u32r = t_fine - ((int32_t)76800);
        v_x1_u32r = ((((adc_H << 14) - (((int32_t)dig_H4) << 20) -
                      (((int32_t)dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
                     (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) *
                         (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) +
                          ((int32_t)32768))) >> 10) + ((int32_t)2097152)) *
                     ((int32_t)dig_H2) + 8192) >> 14);
        v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                     ((int32_t)dig_H1)) >> 4));
        v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
        v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
        return (v_x1_u32r >> 12);
    }
    
public:
    BME280(const std::string& dev, uint8_t addr = 0x76) 
        : i2c(dev, 400000), address(addr) {
        i2c.selectSlave(address);
        
        // Check device ID
        uint8_t id;
        i2c.readByte(REG_ID, &id);
        if(id != 0x60) {
            throw std::runtime_error("BME280 not found");
        }
        
        readCalibrationData();
        
        // Configure sensor
        i2c.writeByte(REG_CTRL_HUM, 0x01);  // Humidity oversampling x1
        i2c.writeByte(REG_CTRL_MEAS, 0x27); // Pressure/Temp x1, normal mode
        i2c.writeByte(REG_CONFIG, 0xA0);    // Standby 1000ms, filter x16
    }
    
    struct Reading {
        float temperature;  // Celsius
        float pressure;     // hPa
        float humidity;     // %RH
    };
    
    Reading readAll() {
        // Wait for measurement complete
        uint8_t status;
        do {
            i2c.readByte(REG_STATUS, &status);
            usleep(10000);
        } while(status & 0x08);
        
        // Read raw data
        uint8_t data[8];
        i2c.readBlock(REG_PRESS_MSB, data, 8);
        
        int32_t adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
        int32_t adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
        int32_t adc_H = (data[6] << 8) | data[7];
        
        Reading result;
        result.temperature = compensateTemperature(adc_T) / 100.0f;
        result.pressure = compensatePressure(adc_P) / 25600.0f;
        result.humidity = compensateHumidity(adc_H) / 1024.0f;
        
        return result;
    }
};

ADS1115 ADC
cpp

class ADS1115 {
    I2C i2c;
    uint8_t address;
    
    // Registers
    static constexpr uint8_t REG_CONVERSION = 0x00;
    static constexpr uint8_t REG_CONFIG = 0x01;
    static constexpr uint8_t REG_LO_THRESH = 0x02;
    static constexpr uint8_t REG_HI_THRESH = 0x03;
    
    enum Gain {
        GAIN_6_144V = 0x00,  // ±6.144V
        GAIN_4_096V = 0x01,  // ±4.096V
        GAIN_2_048V = 0x02,  // ±2.048V (default)
        GAIN_1_024V = 0x03,  // ±1.024V
        GAIN_0_512V = 0x04,  // ±0.512V
        GAIN_0_256V = 0x05   // ±0.256V
    };
    
    enum SampleRate {
        SPS_8 = 0x00,
        SPS_16 = 0x01,
        SPS_32 = 0x02,
        SPS_64 = 0x03,
        SPS_128 = 0x04,
        SPS_250 = 0x05,
        SPS_475 = 0x06,
        SPS_860 = 0x07
    };
    
    uint16_t readRegister(uint8_t reg) {
        uint16_t value;
        i2c.readWord(reg, &value);
        return __builtin_bswap16(value);  // Convert from big-endian
    }
    
    void writeRegister(uint8_t reg, uint16_t value) {
        value = __builtin_bswap16(value);
        i2c.writeWord(reg, value);
    }
    
public:
    ADS1115(const std::string& dev, uint8_t addr = 0x48)
        : i2c(dev, 400000), address(addr) {
        i2c.selectSlave(address);
    }
    
    void configure(Gain gain = GAIN_2_048V, SampleRate rate = SPS_128) {
        uint16_t config = 0;
        config |= (0x01 << 15);  // Operational status (start conversion)
        config |= (0x04 << 12);  // AIN0-GND (default)
        config |= (gain << 11);   // PGA gain
        config |= (0x00 << 9);    // Continuous conversion mode
        config |= (rate << 5);    // Data rate
        config |= (0x03 << 3);    // Traditional comparator, active low
        config |= (0x00 << 2);    // Non-latching comparator
        config |= (0x01 << 1);    // Disable comparator
        config |= 0x00;           // Default polarity
        
        writeRegister(REG_CONFIG, config);
    }
    
    int16_t readADC_SingleEnded(uint8_t channel) {
        if(channel > 3) return 0;
        
        uint16_t config = readRegister(REG_CONFIG);
        config &= 0x8FFF;  // Clear channel bits
        config |= ((channel + 4) << 12);  // Set channel
        writeRegister(REG_CONFIG, config);
        
        usleep(10000);  // Wait for conversion
        
        return (int16_t)readRegister(REG_CONVERSION);
    }
    
    float readVoltage(uint8_t channel, Gain gain = GAIN_2_048V) {
        int16_t raw = readADC_SingleEnded(channel);
        
        float voltage_range;
        switch(gain) {
            case GAIN_6_144V: voltage_range = 6.144f; break;
            case GAIN_4_096V: voltage_range = 4.096f; break;
            case GAIN_2_048V: voltage_range = 2.048f; break;
            case GAIN_1_024V: voltage_range = 1.024f; break;
            case GAIN_0_512V: voltage_range = 0.512f; break;
            case GAIN_0_256V: voltage_range = 0.256f; break;
            default: voltage_range = 2.048f;
        }
        
        return (raw * voltage_range) / 32768.0f;
    }
};

TCA6416A GPIO Expander
cpp

class TCA6416A {
    I2C i2c;
    uint8_t address;
    
    // Registers
    static constexpr uint8_t REG_INPUT_0 = 0x00;
    static constexpr uint8_t REG_INPUT_1 = 0x01;
    static constexpr uint8_t REG_OUTPUT_0 = 0x02;
    static constexpr uint8_t REG_OUTPUT_1 = 0x03;
    static constexpr uint8_t REG_POLARITY_0 = 0x04;
    static constexpr uint8_t REG_POLARITY_1 = 0x05;
    static constexpr uint8_t REG_CONFIG_0 = 0x06;
    static constexpr uint8_t REG_CONFIG_1 = 0x07;
    
    uint16_t readRegister(uint8_t reg) {
        uint16_t value;
        i2c.readWord(reg, &value);
        return value;
    }
    
    void writeRegister(uint8_t reg, uint16_t value) {
        i2c.writeWord(reg, value);
    }
    
public:
    TCA6416A(const std::string& dev, uint8_t addr = 0x20)
        : i2c(dev, 400000), address(addr) {
        i2c.selectSlave(address);
        
        // Default: all inputs with polarity normal
        writeRegister(REG_CONFIG_0, 0xFFFF);
        writeRegister(REG_POLARITY_0, 0x0000);
    }
    
    void setPinMode(uint8_t pin, bool output) {
        uint8_t bank = pin / 8;
        uint8_t bit = pin % 8;
        
        uint16_t config = readRegister(REG_CONFIG_0 + bank);
        if(output) {
            config &= ~(1 << bit);  // Output
        } else {
            config |= (1 << bit);   // Input
        }
        writeRegister(REG_CONFIG_0 + bank, config);
    }
    
    void digitalWrite(uint8_t pin, bool value) {
        uint8_t bank = pin / 8;
        uint8_t bit = pin % 8;
        
        uint16_t output = readRegister(REG_OUTPUT_0 + bank);
        if(value) {
            output |= (1 << bit);
        } else {
            output &= ~(1 << bit);
        }
        writeRegister(REG_OUTPUT_0 + bank, output);
    }
    
    bool digitalRead(uint8_t pin) {
        uint8_t bank = pin / 8;
        uint8_t bit = pin % 8;
        
        uint16_t input = readRegister(REG_INPUT_0 + bank);
        return (input >> bit) & 1;
    }
    
    uint16_t readAll() {
        return readRegister(REG_INPUT_0);
    }
    
    void writeAll(uint16_t value) {
        writeRegister(REG_OUTPUT_0, value);
    }
};

Performance Optimization
DMA for I2C Bulk Transfers
cpp

class I2CDMA : public I2C {
private:
    void* dma_buffer;
    size_t dma_size;
    
    bool setupDMA(size_t size) {
        dma_size = size;
        if(posix_memalign(&dma_buffer, 64, size) != 0) {
            return false;
        }
        
        // Enable DMA for I2C
        int dma_fd = open("/dev/dma", O_RDWR);
        if(dma_fd < 0) return false;
        
        // Configure DMA channel
        struct dma_config config;
        config.src = DMA_MEMORY;
        config.dst = DMA_I2C_TX_FIFO;
        config.size = size;
        config.mode = DMA_MEM_TO_DEV;
        
        ioctl(dma_fd, DMA_SET_CONFIG, &config);
        close(dma_fd);
        
        return true;
    }
    
public:
    ssize_t dmaWrite(uint8_t reg, const uint8_t* data, size_t len) {
        if(len > dma_size) setupDMA(len);
        
        // Prepare buffer with register address
        uint8_t* buffer = (uint8_t*)dma_buffer;
        buffer[0] = reg;
        memcpy(buffer + 1, data, len);
        
        // Start DMA transfer
        int dma_fd = open("/dev/dma", O_RDWR);
        ioctl(dma_fd, DMA_START, buffer);
        
        // Wait for completion
        ioctl(dma_fd, DMA_WAIT, NULL);
        close(dma_fd);
        
        return len + 1;
    }
};

Clock Stretching Handling
cpp

class I2CClockStretching {
    I2C i2c;
    static constexpr int MAX_STRETCH_US = 10000;  // 10ms max
    
public:
    bool handleStretch(uint8_t address) {
        auto start = std::chrono::high_resolution_clock::now();
        
        while(true) {
            if(i2c.selectSlave(address)) {
                return true;
            }
            
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start);
            
            if(elapsed.count() > MAX_STRETCH_US) {
                return false;  // Timeout
            }
            
            usleep(100);  // Wait before retry
        }
    }
};

Troubleshooting Guide
Common Issues

    No devices found on bus

        Check pull-up resistors (4.7kΩ typical)

        Verify voltage levels (3.3V)

        Check SDA/SCL connections

    Communication errors

        Reduce bus speed

        Check for bus capacitance > 400pF

        Verify device addressing

    Clock stretching issues

        Increase timeout values

        Check if device supports clock stretching

        Use slower bus speed

Debug Commands
bash

# Scan I2C bus
i2cdetect -y 1

# Read register from device
i2cget -y 1 0x48 0x00 w

# Write to device register
i2cset -y 1 0x48 0x01 0x1234 w

# Dump all registers
i2cdump -y 1 0x48

# Monitor I2C traffic
i2cstub -y 1 -a 0x48 -s 100000

Logic Analyzer Integration
bash

# Capture I2C traffic
sigrok-cli --driver fx2lafw --config samplerate=100m \
  --protocol-decoder i2c --channels 0=SDA,1=SCL \
  -o i2c_capture.sr

# Decode captured data
sigrok-cli -i i2c_capture.sr -P i2c:sda=0:scl=1 -A i2c=data-read

Best Practices

    Always use external pull-up resistors (2.2kΩ to 10kΩ)

    Keep bus capacitance low (< 400pF for 400kHz)

    Use level shifters for 5V devices

    Implement error recovery for stuck bus conditions

    Use repeated starts instead of stop-start

    Add bus buffers for long cables or many devices

    Monitor bus status with interrupts when possible

    Use SMBus protocols for standardized device access

Industrial Applications
I2C Bus Extender for Long Cables
cpp

class I2CExtender {
    int bus_fd;
    std::thread monitoring_thread;
    
public:
    I2CExtender(const std::string& bus) {
        bus_fd = open(bus.c_str(), O_RDWR);
        
        // Increase drive strength for longer cables
        ioctl(bus_fd, I2C_DRIVE_STRENGTH, 0x03);  // 15mA
        ioctl(bus_fd, I2C_SLEW_RATE, 0x01);       // Slower slew rate
        
        monitoring_thread = std::thread([this]() {
            monitorBus();
        });
    }
    
    void monitorBus() {
        char buffer[256];
        int bytes;
        
        while(true) {
            bytes = read(bus_fd, buffer, sizeof(buffer));
            if(bytes > 0) {
                // Check for bus errors
                if(buffer[0] & 0x80) {
                    handleBusError();
                }
            }
            usleep(10000);
        }
    }
    
    void handleBusError() {
        // Recovery procedure
        ioctl(bus_fd, I2C_RESET_BUS);
        usleep(100000);
        
        // Generate 9 clock pulses to reset slaves
        for(int i = 0; i < 9; i++) {
            ioctl(bus_fd, I2C_CLOCK_PULSE);
            usleep(10);
        }
    }
};

