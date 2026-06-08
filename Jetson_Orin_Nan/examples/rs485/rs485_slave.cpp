/**
 * RS485 Slave Example
 * 
 * Demonstrates RS485 slave mode responding to Modbus RTU requests
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <signal.h>
#include "rs485.hpp"

static volatile bool running = true;

void signalHandler(int signum) {
    running = false;
}

// Simulated holding registers
uint16_t holding_registers[100] = {0};

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

void processModbusRequest(const uint8_t* request, size_t len, uint8_t* response, size_t& response_len) {
    if(len < 8) return;
    
    uint8_t slave_id = request[0];
    uint8_t function_code = request[1];
    
    // Verify CRC
    uint16_t received_crc = request[len-2] | (request[len-1] << 8);
    uint16_t calculated_crc = calculateCRC(request, len - 2);
    
    if(received_crc != calculated_crc) {
        std::cout << "CRC error" << std::endl;
        return;
    }
    
    response[0] = slave_id;
    
    switch(function_code) {
        case 0x03: { // Read holding registers
            uint16_t start_addr = (request[2] << 8) | request[3];
            uint16_t num_regs = (request[4] << 8) | request[5];
            
            if(num_regs > 125) {
                response[1] = function_code | 0x80;
                response[2] = 0x02;  // Illegal data address
                response_len = 3;
                break;
            }
            
            response[1] = function_code;
            response[2] = num_regs * 2;
            
            for(int i = 0; i < num_regs && (start_addr + i) < 100; i++) {
                response[3 + 2*i] = (holding_registers[start_addr + i] >> 8) & 0xFF;
                response[4 + 2*i] = holding_registers[start_addr + i] & 0xFF;
            }
            
            response_len = 3 + num_regs * 2;
            break;
        }
        
        case 0x06: { // Write single register
            uint16_t address = (request[2] << 8) | request[3];
            uint16_t value = (request[4] << 8) | request[5];
            
            if(address < 100) {
                holding_registers[address] = value;
                memcpy(response, request, len);
                response_len = len;
                std::cout << "Wrote to register " << address << ": " << value << std::endl;
            } else {
                response[1] = function_code | 0x80;
                response[2] = 0x02;
                response_len = 3;
            }
            break;
        }
        
        default:
            response[1] = function_code | 0x80;
            response[2] = 0x01;  // Illegal function
            response_len = 3;
            break;
    }
    
    // Append CRC to response
    uint16_t crc = calculateCRC(response, response_len);
    response[response_len] = crc & 0xFF;
    response[response_len + 1] = (crc >> 8) & 0xFF;
    response_len += 2;
}

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== RS485 Slave Example ===" << std::endl;
    std::cout << "Device: /dev/ttyTHS1" << std::endl;
    std::cout << "Baud rate: 9600" << std::endl;
    std::cout << "Slave ID: 1" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    // Initialize holding registers with test values
    for(int i = 0; i < 10; i++) {
        holding_registers[i] = i * 100;
    }
    
    try {
        RS485::Config config;
        config.baudrate = 9600;
        config.auto_direction = true;
        config.turn_around_delay_us = 100;
        
        RS485 rs485("/dev/ttyTHS1", config);
        
        if(!rs485.open()) {
            std::cerr << "Failed to open RS485 device" << std::endl;
            return 1;
        }
        
        rs485.setSlaveAddress(1);
        
        std::cout << "RS485 slave running. Waiting for requests..." << std::endl;
        std::cout << std::endl;
        
        uint8_t buffer[256];
        
        while(running) {
            ssize_t bytes = rs485.read(buffer, sizeof(buffer), 100);
            
            if(bytes > 0) {
                std::cout << "Received " << bytes << " bytes" << std::endl;
                
                uint8_t response[256];
                size_t response_len = 0;
                
                processModbusRequest(buffer, bytes, response, response_len);
                
                if(response_len > 0) {
                    rs485.write(response, response_len);
                    std::cout << "Sent response" << std::endl;
                }
            }
        }
        
        rs485.close();
        std::cout << "\nRS485 closed." << std::endl;
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
