/**
 * I2C Sensor Read Example
 * 
 * Demonstrates reading from BME280 environmental sensor
 * Connect BME280 to I2C1 (pins 3=SDA, 5=SCL)
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include "i2c.hpp"

class BME280 {
private:
    I2C i2c;
    
    // Calibration data
    uint16_t dig_T1;
    int16_t dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t dig_H1, dig_H3;
    int16_t dig_H2, dig_H4, dig_H5, dig_H6;
    
    int32_t t_fine;
    
    void readCalibration() {
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
    BME280() : i2c("/dev/i2c-1", 0x76) {
        i2c.open();
        
        // Check device ID
        uint8_t id;
        i2c.readByte(0xD0, &id);
        if(id != 0x60) {
            throw std::runtime_error("BME280 not found");
        }
        
        readCalibration();
        
        // Configure sensor
        i2c.writeByte(0xF2, 0x01);  // Humidity oversampling x1
        i2c.writeByte(0xF4, 0x27);  // Pressure/Temp x1, normal mode
        i2c.writeByte(0xF5, 0xA0);  // Standby 1000ms, filter x16
    }
    
    ~BME280() {
        i2c.close();
    }
    
    void read(float& temp, float& pressure, float& humidity) {
        // Wait for measurement
        uint8_t status;
        do {
            i2c.readByte(0xF3, &status);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while(status & 0x08);
        
        // Read data
        uint8_t data[8];
        i2c.readBlock(0xF7, data, 8);
        
        int32_t adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
        int32_t adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
        int32_t adc_H = (data[6] << 8) | data[7];
        
        temp = compensateTemperature(adc_T) / 100.0f;
        pressure = compensatePressure(adc_P) / 25600.0f;
        humidity = compensateHumidity(adc_H) / 1024.0f;
    }
};

int main() {
    std::cout << "=== I2C Sensor Read Example ===" << std::endl;
    std::cout << "Sensor: BME280 Environmental Sensor" << std::endl;
    std::cout << "I2C Bus: 1 (pins 3=SDA, 5=SCL)" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        BME280 sensor;
        
        while(true) {
            float temp, pressure, humidity;
            sensor.read(temp, pressure, humidity);
            
            std::cout << "Temperature: " << std::fixed << std::setprecision(1) 
                      << temp << " °C" << std::endl;
            std::cout << "Pressure: " << std::setprecision(0) 
                      << pressure << " hPa" << std::endl;
            std::cout << "Humidity: " << std::setprecision(1) 
                      << humidity << " %" << std::endl;
            std::cout << "------------------------" << std::endl;
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
