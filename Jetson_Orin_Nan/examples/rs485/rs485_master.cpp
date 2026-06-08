/**
 * RS485 Master Example
 * 
 * Demonstrates RS485 master mode with Modbus RTU protocol
 * Requires external RS485 transceiver (e.g., MAX485)
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include "rs485.hpp"

// Modbus function codes
const uint8_t MODBUS_READ_HOLDING_REGISTERS = 0x03;
const uint8_t MODBUS_WRITE_SINGLE_REGISTER = 0x06;
const uint8_t MODBUS_WRITE_MULTIPLE_REGISTERS = 0x10;

uint16_t calculateCRC(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(int j = 0; j < 8; j++) {
            if(crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void printHex(const uint8_t* data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    std::cout << "=== RS485 Master Example ===" << std::endl;
    std::cout << "Device: /dev/ttyTHS1" << std::endl;
    std::cout << "Baud rate: 9600" << std::endl;
    std::cout << "Modbus RTU protocol" << std::endl;
    std::cout << std::endl;
    
    try {
        RS485::Config config;
        config.baudrate = 9600;
        config.auto_direction = true;  // Use RTS for direction control
        config.turn_around_delay_us = 100;
        config.data_bits = 8;
        config.parity = 'N';
        config.stop_bits = 1;
        
        RS485 rs485("/dev/ttyTHS1", config);
        
        if(!rs485.open()) {
            std::cerr << "Failed to open RS485 device" << std::endl;
            return 1;
        }
        
        std::cout << "RS485 opened successfully" << std::endl;
        std::cout << std::endl;
        
        uint8_t slave_id = 1;
        
        // Read holding registers from slave
        std::cout << "Reading holding registers from slave " << (int)slave_id << "..." << std::endl;
        
        uint8_t read_frame[] = {
            slave_id,
            MODBUS_READ_HOLDING_REGISTERS,
            0x00, 0x00,  // Start address
            0x00, 0x0A   // Number of registers (10)
        };
        
        // Calculate and append CRC
        uint16_t crc = calculateCRC(read_frame, sizeof(read_frame));
        uint8_t frame[sizeof(read_frame) + 2];
        memcpy(frame, read_frame, sizeof(read_frame));
        frame[sizeof(read_frame)] = crc & 0xFF;
        frame[sizeof(read_frame) + 1] = (crc >> 8) & 0xFF;
        
        std::cout << "Sending: ";
        printHex(frame, sizeof(frame));
        
        ssize_t sent = rs485.modbusWrite(frame, sizeof(frame));
        if(sent != (ssize_t)sizeof(frame)) {
            std::cerr << "Write failed" << std::endl;
            rs485.close();
            return 1;
        }
        
        // Read response
        uint8_t response[256];
        ssize_t received = rs485.modbusRead(response, sizeof(response), 500);
        
        if(received > 0) {
            std::cout << "Received: ";
            printHex(response, received);
            
            // Parse response
            if(response[0] == slave_id && response[1] == MODBUS_READ_HOLDING_REGISTERS) {
                int byte_count = response[2];
                int register_count = byte_count / 2;
                
                std::cout << "\nRegister values:" << std::endl;
                for(int i = 0; i < register_count; i++) {
                    uint16_t value = (response[3 + 2*i] << 8) | response[4 + 2*i];
                    std::cout << "  Register " << i << ": " << value << std::endl;
                }
            }
        } else {
            std::cout << "No response from slave" << std::endl;
        }
        
        // Write single register
        std::cout << "\nWriting to register 0..." << std::endl;
        
        uint8_t write_frame[] = {
            slave_id,
            MODBUS_WRITE_SINGLE_REGISTER,
            0x00, 0x00,  // Register address
            0x12, 0x34   // Value to write
        };
        
        crc = calculateCRC(write_frame, sizeof(write_frame));
        uint8_t write_cmd[sizeof(write_frame) + 2];
        memcpy(write_cmd, write_frame, sizeof(write_frame));
        write_cmd[sizeof(write_frame)] = crc & 0xFF;
        write_cmd[sizeof(write_frame) + 1] = (crc >> 8) & 0xFF;
        
        std::cout << "Sending: ";
        printHex(write_cmd, sizeof(write_cmd));
        
        sent = rs485.modbusWrite(write_cmd, sizeof(write_cmd));
        if(sent == (ssize_t)sizeof(write_cmd)) {
            received = rs485.modbusRead(response, sizeof(response), 500);
            if(received > 0) {
                std::cout << "Write confirmed" << std::endl;
            }
        }
        
        rs485.close();
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
