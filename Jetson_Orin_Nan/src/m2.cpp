#include "m2.hpp"
#include <fstream>
#include <sstream>

M2::M2() : power_enabled_(false), monitoring_(false) {}

M2::~M2() {
    if (monitoring_) {
        monitoring_ = false;
        if (monitor_thread_.joinable()) monitor_thread_.join();
    }
}

bool M2::detectDevice(DeviceInfo& info) {
    PCIe pcie;
    PCIe::DeviceInfo nvme_dev;
    
    if (pcie.findDevice(0x144D, 0x0000, nvme_dev)) {  // Samsung NVMe
        info.key_type = KeyType::M_KEY;
        info.pcie_speed = nvme_dev.link_speed;
        info.pcie_lanes = nvme_dev.link_width;
        
        // Read model from sysfs
        std::ifstream model("/sys/class/nvme/nvme0/model");
        std::getline(model, info.model);
        
        std::ifstream serial("/sys/class/nvme/nvme0/serial");
        std::getline(serial, info.serial);
        
        return true;
    }
    
    return false;
}

bool M2::isNVMePresent() {
    DeviceInfo info;
    return detectDevice(info);
}

M2::NVMeSSD::NVMeSSD() : PCIe::NVMeDevice(PCIe::DeviceInfo()) {
    // Find NVMe device
    PCIe pcie;
    PCIe::DeviceInfo dev;
    pcie.findDevice(0x144D, 0x0000, dev);
    // Would initialize with actual device
}

bool M2::NVMeSSD::initialize() {
    return identify();
}

uint64_t M2::NVMeSSD::getCapacity() const {
    std::ifstream size("/sys/class/nvme/nvme0/ns0/size");
    uint64_t sectors;
    size >> sectors;
    return sectors * 512;
}

std::string M2::NVMeSSD::getModel() const {
    std::ifstream model("/sys/class/nvme/nvme0/model");
    std::string model_str;
    std::getline(model, model_str);
    return model_str;
}

std::string M2::NVMeSSD::getSerial() const {
    std::ifstream serial("/sys/class/nvme/nvme0/serial");
    std::string serial_str;
    std::getline(serial, serial_str);
    return serial_str;
}

int M2::NVMeSSD::getTemperature() const {
    std::ifstream temp("/sys/class/nvme/nvme0/hwmon/hwmon0/temp1_input");
    int millidegrees;
    temp >> millidegrees;
    return millidegrees / 1000;
}

bool M2::NVMeSSD::enableASPM() {
    // Write to PCIe configuration space
    std::ofstream aspm("/sys/bus/pci/devices/0000:01:00.0/power/control");
    aspm << "auto";
    return true;
}

double M2::NVMeSSD::readSpeed() {
    // Benchmark read speed
    const size_t TEST_SIZE = 256 * 1024 * 1024;  // 256MB
    void* buffer = malloc(TEST_SIZE);
    
    auto start = std::chrono::steady_clock::now();
    readSectors(0, TEST_SIZE / 512, buffer);
    auto end = std::chrono::steady_clock::now();
    
    double elapsed = std::chrono::duration<double>(end - start).count();
    free(buffer);
    
    return (TEST_SIZE / elapsed) / (1024.0 * 1024.0);  // MB/s
}

bool M2::setPowerEnable(bool enable) {
    power_enabled_ = enable;
    if (enable) {
        std::ofstream power("/sys/bus/pci/devices/0000:01:00.0/power/control");
        power << "on";
    } else {
        std::ofstream power("/sys/bus/pci/devices/0000:01:00.0/power/control");
        power << "auto";
    }
    return true;
}

double M2::getTemperature() const {
    std::ifstream temp("/sys/class/thermal/thermal_zone0/temp");
    int millidegrees;
    temp >> millidegrees;
    return millidegrees / 1000.0;
}

std::string M2::keyTypeToString(KeyType type) {
    switch (type) {
        case KeyType::M_KEY: return "M-key (PCIe x4)";
        case KeyType::B_KEY: return "B-key (USB/PCIe x2)";
        case KeyType::A_KEY: return "A-key (WiFi/PCIe x2)";
        case KeyType::E_KEY: return "E-key (WiFi/BT/PCIe x2)";
        default: return "Unknown";
    }
}

std::string M2::formFactorToString(FormFactor factor) {
    switch (factor) {
        case FormFactor::SIZE_2242: return "2242 (22x42mm)";
        case FormFactor::SIZE_2260: return "2260 (22x60mm)";
        case FormFactor::SIZE_2280: return "2280 (22x80mm)";
        case FormFactor::SIZE_22110: return "22110 (22x110mm)";
        default: return "Unknown";
    }
}
