#include "pcie.hpp"
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>

PCIe::PCIe() {}
PCIe::~PCIe() {}

uint32_t PCIe::readPCIConfig(uint8_t bus, uint8_t slot, uint8_t function, 
                              uint8_t offset, uint8_t size) {
    int fd = open("/proc/bus/pci/00/00.0", O_RDWR);
    if (fd < 0) return 0;
    
    uint32_t address = (bus << 16) | (slot << 11) | (function << 8) | (offset & 0xFC);
    lseek(fd, address, SEEK_SET);
    
    uint32_t value = 0;
    read(fd, &value, size);
    close(fd);
    
    return value;
}

void PCIe::writePCIConfig(uint8_t bus, uint8_t slot, uint8_t function,
                           uint8_t offset, uint32_t value, uint8_t size) {
    int fd = open("/proc/bus/pci/00/00.0", O_RDWR);
    if (fd < 0) return;
    
    uint32_t address = (bus << 16) | (slot << 11) | (function << 8) | (offset & 0xFC);
    lseek(fd, address, SEEK_SET);
    write(fd, &value, size);
    close(fd);
}

void PCIe::scanBus(uint8_t bus) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        for (uint8_t function = 0; function < 8; function++) {
            uint16_t vendor_id = readPCIConfig(bus, slot, function, 0x00, 2);
            if (vendor_id == 0xFFFF || vendor_id == 0x0000) continue;
            
            DeviceInfo dev;
            dev.domain = 0;
            dev.bus = bus;
            dev.slot = slot;
            dev.function = function;
            dev.vendor_id = vendor_id;
            dev.device_id = readPCIConfig(bus, slot, function, 0x02, 2);
            dev.class_code = readPCIConfig(bus, slot, function, 0x0B, 1);
            dev.subclass = readPCIConfig(bus, slot, function, 0x0A, 1);
            dev.prog_if = readPCIConfig(bus, slot, function, 0x09, 1);
            dev.revision_id = readPCIConfig(bus, slot, function, 0x08, 1);
            
            for (int i = 0; i < 6; i++) {
                dev.bar[i] = readPCIConfig(bus, slot, function, 0x10 + 4*i, 4);
            }
            
            dev.irq_line = readPCIConfig(bus, slot, function, 0x3C, 1);
            dev.irq_pin = readPCIConfig(bus, slot, function, 0x3D, 1);
            
            // Get link speed and width from sysfs if available
            char path[256];
            snprintf(path, sizeof(path), "/sys/bus/pci/devices/%04x:%02x:%02x.%01x/",
                     0, bus, slot, function);
            
            std::ifstream speed_file(std::string(path) + "current_link_speed");
            if (speed_file) speed_file >> dev.link_speed;
            
            std::ifstream width_file(std::string(path) + "current_link_width");
            if (width_file) width_file >> dev.link_width;
            
            devices_.push_back(dev);
        }
    }
}

std::vector<PCIe::DeviceInfo> PCIe::scanDevices() {
    devices_.clear();
    scanBus(0);
    return devices_;
}

bool PCIe::findDevice(uint16_t vendor_id, uint16_t device_id, DeviceInfo& device) {
    scanDevices();
    for (const auto& dev : devices_) {
        if (dev.vendor_id == vendor_id && dev.device_id == device_id) {
            device = dev;
            return true;
        }
    }
    return false;
}

// NVMeDevice implementation
PCIe::NVMeDevice::NVMeDevice(const DeviceInfo& device) : device_(device) {
    dma_ = std::make_unique<DMAEngine>();
    identify_buffer_ = dma_->allocBuffer(4096);
}

PCIe::NVMeDevice::~NVMeDevice() {
    if (identify_buffer_) dma_->freeBuffer(identify_buffer_);
}

bool PCIe::NVMeDevice::identify() {
    // Map BAR0
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    void* reg_base = mmap(NULL, 8192, PROT_READ | PROT_WRITE,
                          MAP_SHARED, mem_fd, device_.bar[0] & ~0x0F);
    close(mem_fd);
    
    // Submit identify command (simplified)
    uint64_t* regs = (uint64_t*)reg_base;
    regs[0x10] = (uint64_t)identify_buffer_;  // PRP1
    
    munmap(reg_base, 8192);
    return true;
}
