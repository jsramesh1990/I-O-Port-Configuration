#include "usb.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>

USB::USB() {}

USB::~USB() {}

std::string USB::readSysfs(const std::string& path) {
    std::ifstream file(path);
    std::string content;
    if (file.is_open()) {
        std::getline(file, content);
        content.erase(content.find_last_not_of(" \n\r\t") + 1);
    }
    return content;
}

void USB::scanDevices() {
    cached_devices_.clear();
    
    DIR* dir = opendir("/sys/bus/usb/devices");
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        std::string dev_path = "/sys/bus/usb/devices/" + std::string(entry->d_name);
        
        // Check if it's a USB device (has idVendor file)
        std::string vendor_path = dev_path + "/idVendor";
        if (access(vendor_path.c_str(), R_OK) == 0) {
            DeviceInfo dev;
            dev.device_path = dev_path;
            
            std::string vendor_str = readSysfs(vendor_path);
            dev.vendor_id = std::stoul(vendor_str, nullptr, 16);
            
            std::string product_str = readSysfs(dev_path + "/idProduct");
            dev.product_id = std::stoul(product_str, nullptr, 16);
            
            dev.manufacturer = readSysfs(dev_path + "/manufacturer");
            dev.product = readSysfs(dev_path + "/product");
            dev.serial_number = readSysfs(dev_path + "/serial");
            dev.bus_number = readSysfs(dev_path + "/busnum");
            dev.device_number = readSysfs(dev_path + "/devnum");
            
            std::string speed_str = readSysfs(dev_path + "/speed");
            if (speed_str == "1.5") dev.speed = 1;
            else if (speed_str == "12") dev.speed = 2;
            else if (speed_str == "480") dev.speed = 3;
            else if (speed_str == "5000") dev.speed = 4;
            else dev.speed = 0;
            
            std::string maxpower_str = readSysfs(dev_path + "/bMaxPower");
            if (!maxpower_str.empty()) {
                dev.max_power = std::stoi(maxpower_str);
            }
            
            cached_devices_.push_back(dev);
        }
    }
    
    closedir(dir);
}

std::vector<USB::DeviceInfo> USB::getDevices() {
    scanDevices();
    return cached_devices_;
}

bool USB::findDevice(uint16_t vid, uint16_t pid, DeviceInfo& device) {
    scanDevices();
    for (const auto& dev : cached_devices_) {
        if (dev.vendor_id == vid && dev.product_id == pid) {
            device = dev;
            return true;
        }
    }
    return false;
}

bool USB::findDeviceBySerial(const std::string& serial, DeviceInfo& device) {
    scanDevices();
    for (const auto& dev : cached_devices_) {
        if (dev.serial_number == serial) {
            device = dev;
            return true;
        }
    }
    return false;
}

bool USB::resetDevice(const DeviceInfo& device) {
    std::string path = device.device_path + "/authorized";
    std::ofstream file(path);
    if (file.is_open()) {
        file << "0";
        file.close();
        usleep(100000);
        
        file.open(path);
        file << "1";
        return true;
    }
    return false;
}

bool USB::setPowerSave(const DeviceInfo& device, bool enable) {
    std::string path = device.device_path + "/power/control";
    std::ofstream file(path);
    if (file.is_open()) {
        file << (enable ? "auto" : "on");
        return true;
    }
    return false;
}

bool USB::claimInterface(const DeviceInfo& device, int interface_num) {
    // Implementation would require opening the USB device and claiming interface
    return false;
}

bool USB::releaseInterface(const DeviceInfo& device, int interface_num) {
    return false;
}

std::string USB::DeviceInfo::toString() const {
    std::stringstream ss;
    ss << "Vendor: 0x" << std::hex << vendor_id << ", Product: 0x" << product_id << std::dec << "\n";
    ss << "  Manufacturer: " << manufacturer << "\n";
    ss << "  Product: " << product << "\n";
    ss << "  Serial: " << serial_number << "\n";
    ss << "  Bus: " << bus_number << ", Device: " << device_number << "\n";
    ss << "  Speed: " << speedToString(speed);
    return ss.str();
}

std::string USB::speedToString(int speed) {
    switch (speed) {
        case 1: return "1.5 Mbps (Low Speed)";
        case 2: return "12 Mbps (Full Speed)";
        case 3: return "480 Mbps (High Speed)";
        case 4: return "5 Gbps (SuperSpeed)";
        default: return "Unknown";
    }
}

std::string USB::vidToString(uint16_t vid) {
    // Common vendor IDs
    switch (vid) {
        case 0x046D: return "Logitech";
        case 0x05AC: return "Apple";
        case 0x0781: return "SanDisk";
        case 0x090C: return "Silicon Motion";
        case 0x0BDA: return "Realtek";
        case 0x13FE: return "Kingston";
        case 0x17EF: return "Lenovo";
        case 0x1D6B: return "Linux Foundation";
        default: return "Unknown";
    }
}

std::string USB::pidToString(uint16_t vid, uint16_t pid) {
    // This would require a database of known devices
    return "Unknown";
}

// SerialPort implementation
USB::SerialPort::SerialPort(const std::string& device_path) 
    : device_path_(device_path), fd_(-1) {}

USB::SerialPort::~SerialPort() {
    close();
}

bool USB::SerialPort::open(int baudrate) {
    fd_ = ::open(device_path_.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) return false;
    
    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    
    tio.c_cflag = CS8 | CLOCAL | CREAD;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 10;
    
    cfsetospeed(&tio, B115200);
    cfsetispeed(&tio, B115200);
    
    tcflush(fd_, TCIFLUSH);
    tcsetattr(fd_, TCSANOW, &tio);
    
    return true;
}

void USB::SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

ssize_t USB::SerialPort::write(const uint8_t* data, size_t len) {
    if (fd_ < 0) return -1;
    return ::write(fd_, data, len);
}

ssize_t USB::SerialPort::read(uint8_t* buffer, size_t max_len, int timeout_ms) {
    if (fd_ < 0) return -1;
    
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(fd_ + 1, &fds, NULL, NULL, &timeout);
    if (result > 0) {
        return ::read(fd_, buffer, max_len);
    }
    return 0;
}

bool USB::SerialPort::isOpen() const {
    return fd_ >= 0;
}
