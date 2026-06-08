
## docs/pcie.md

```markdown
# PCIe (Peripheral Component Interconnect Express)

## Overview

The Jetson Orin Nano features PCIe Gen3 interfaces for connecting high-speed peripherals such as NVMe SSDs, AI accelerators, and other expansion cards.

## Hardware Specifications

| Parameter | Value |
|-----------|-------|
| PCIe Version | Gen3 (8 GT/s) |
| Lanes | x4 (configurable as x1/x2/x4) |
| Ports | 1 (M.2 M-key connector) |
| Bandwidth | 3.94 GB/s (x4) |
| DMA Support | Yes |
| AER (Advanced Error Reporting) | Yes |
| SR-IOV | Supported |
| ASPM (Active State Power Management) | Yes |
| Reference Clock | 100 MHz |

## PCIe Controller

### Controller Mapping
```
PCIe Controller 0 ─── M.2 M-key slot (x4)
PCIe Controller 1 ─── Internal (reserved)
PCIe Controller 2 ─── Internal (reserved)
```

### Configuration Space (Type 0 Header)
```
Offset 0x00: Vendor ID / Device ID
Offset 0x04: Command / Status
Offset 0x08: Revision ID / Class Code
Offset 0x0C: Cache Line Size / Latency Timer / Header Type
Offset 0x10: BAR0 (Base Address Register 0)
Offset 0x14: BAR1
Offset 0x18: BAR2
Offset 0x1C: BAR3
Offset 0x20: BAR4
Offset 0x24: BAR5
Offset 0x28: CardBus CIS Pointer
Offset 0x2C: Subsystem Vendor ID / Subsystem ID
Offset 0x30: Expansion ROM Base Address
Offset 0x34: Capabilities Pointer
Offset 0x38: Reserved
Offset 0x3C: Interrupt Line / Interrupt Pin
Offset 0x40: Min Grant / Max Latency
```

## Implementation

### PCIe Device Scanner
```cpp
class PCIeScanner {
private:
    struct PCIDevice {
        uint16_t domain;
        uint8_t bus;
        uint8_t slot;
        uint8_t function;
        uint16_t vendor_id;
        uint16_t device_id;
        uint8_t class_code;
        uint8_t subclass;
        uint8_t prog_if;
        uint8_t revision_id;
        uint32_t bar[6];
        uint8_t irq_line;
        uint8_t irq_pin;
    };
    
    std::vector<PCIDevice> devices;
    
    uint32_t readConfig(uint8_t bus, uint8_t slot, uint8_t function, 
                        uint8_t offset, uint8_t size) {
        int fd = open("/proc/bus/pci/00/00.0", O_RDWR);
        if(fd < 0) return 0;
        
        uint32_t address = (bus << 16) | (slot << 11) | 
                          (function << 8) | (offset & 0xFC);
        
        lseek(fd, address, SEEK_SET);
        uint32_t value;
        read(fd, &value, size);
        close(fd);
        
        return value;
    }
    
    void scanBus(uint8_t bus) {
        for(uint8_t slot = 0; slot < 32; slot++) {
            for(uint8_t function = 0; function < 8; function++) {
                uint16_t vendor_id = readConfig(bus, slot, function, 0x00, 2);
                if(vendor_id == 0xFFFF || vendor_id == 0x0000) continue;
                
                PCIDevice dev;
                dev.domain = 0;
                dev.bus = bus;
                dev.slot = slot;
                dev.function = function;
                dev.vendor_id = vendor_id;
                dev.device_id = readConfig(bus, slot, function, 0x02, 2);
                dev.class_code = readConfig(bus, slot, function, 0x0B, 1);
                dev.subclass = readConfig(bus, slot, function, 0x0A, 1);
                dev.prog_if = readConfig(bus, slot, function, 0x09, 1);
                dev.revision_id = readConfig(bus, slot, function, 0x08, 1);
                
                // Read BARs
                for(int i = 0; i < 6; i++) {
                    dev.bar[i] = readConfig(bus, slot, function, 0x10 + 4*i, 4);
                }
                
                dev.irq_line = readConfig(bus, slot, function, 0x3C, 1);
                dev.irq_pin = readConfig(bus, slot, function, 0x3D, 1);
                
                devices.push_back(dev);
            }
        }
    }
    
public:
    void scan() {
        devices.clear();
        
        // Scan bus 0
        scanBus(0);
        
        // Check for PCIe bridges and scan secondary buses
        for(const auto& dev : devices) {
            if(dev.class_code == 0x06 && dev.subclass == 0x04) {
                uint8_t secondary_bus = readConfig(dev.bus, dev.slot, 
                                                   dev.function, 0x19, 1);
                if(secondary_bus != 0) {
                    scanBus(secondary_bus);
                }
            }
        }
    }
    
    void printDevices() {
        printf("=== PCIe Devices ===\n");
        for(const auto& dev : devices) {
            printf("Domain: %d, Bus: %d, Slot: %d, Function: %d\n",
                   dev.domain, dev.bus, dev.slot, dev.function);
            printf("  Vendor: 0x%04X, Device: 0x%04X\n", 
                   dev.vendor_id, dev.device_id);
            printf("  Class: 0x%02X, Subclass: 0x%02X\n", 
                   dev.class_code, dev.subclass);
            printf("  IRQ: %d (pin %d)\n", dev.irq_line, dev.irq_pin);
            printf("  BARs:");
            for(int i = 0; i < 6 && dev.bar[i] != 0; i++) {
                printf(" 0x%08X", dev.bar[i]);
            }
            printf("\n---\n");
        }
    }
    
    bool findDevice(uint16_t vendor_id, uint16_t device_id, PCIDevice& device) {
        for(const auto& dev : devices) {
            if(dev.vendor_id == vendor_id && dev.device_id == device_id) {
                device = dev;
                return true;
            }
        }
        return false;
    }
};
```

### NVMe SSD Interface
```cpp
class NVMeController {
private:
    int fd;
    void* reg_base;
    size_t reg_size;
    uint32_t doorbell_stride;
    
    struct NVMeRegisters {
        uint64_t cap;       // Controller capabilities
        uint32_t vs;        // Version
        uint32_t intms;     // Interrupt mask
        uint32_t intmc;     // Interrupt mask clear
        uint32_t cc;        // Controller configuration
        uint32_t reserved0;
        uint32_t csts;      // Controller status
        uint32_t nssr;      // NVM subsystem reset
        uint32_t aqa;       // Admin queue attributes
        uint64_t asq;       // Admin submission queue
        uint64_t acq;       // Admin completion queue
    } __attribute__((packed));
    
    struct NVMeCommand {
        uint32_t cdw0;      // Command opcode
        uint32_t nsid;      // Namespace ID
        uint64_t reserved0;
        uint64_t mptr;      // Metadata pointer
        uint64_t prp1;      // PRP entry 1
        uint64_t prp2;      // PRP entry 2
        uint32_t cdw10;     // Command-specific
        uint32_t cdw11;
        uint32_t cdw12;
        uint32_t cdw13;
        uint32_t cdw14;
        uint32_t cdw15;
    } __attribute__((packed));
    
    struct NVMeCompletion {
        uint32_t result;
        uint32_t reserved0;
        uint16_t sq_head;
        uint16_t sq_id;
        uint16_t command_id;
        uint16_t status;
    } __attribute__((packed));
    
public:
    NVMeController(const PCIDevice& device) {
        // Map BAR0 (registers)
        int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
        reg_base = mmap(NULL, 8192, PROT_READ | PROT_WRITE,
                       MAP_SHARED, mem_fd, device.bar[0] & ~0x0F);
        close(mem_fd);
        
        // Check capabilities
        NVMeRegisters* regs = (NVMeRegisters*)reg_base;
        printf("NVMe Version: %d.%d\n", 
               (regs->vs >> 16) & 0xFFFF, regs->vs & 0xFFFF);
        printf("Max Queue Entries: %d\n", (regs->cap >> 16) & 0xFFFF);
        
        doorbell_stride = (regs->cap >> 32) & 0xFFF;
    }
    
    ~NVMeController() {
        if(reg_base) munmap(reg_base, 8192);
    }
    
    bool identifyController() {
        NVMeRegisters* regs = (NVMeRegisters*)reg_base;
        
        // Allocate DMA buffer for identify data
        void* identify_buffer;
        posix_memalign(&identify_buffer, 4096, 4096);
        
        // Build identify command
        NVMeCommand cmd = {0};
        cmd.cdw0 = 0x06;  // Identify opcode
        cmd.nsid = 0;      // Controller identify
        cmd.prp1 = (uint64_t)identify_buffer;
        
        // Submit command
        if(submitCommand(&cmd) < 0) {
            free(identify_buffer);
            return false;
        }
        
        // Parse identify data
        uint8_t* data = (uint8_t*)identify_buffer;
        char model[41];
        char serial[21];
        char firmware[9];
        
        memcpy(model, data + 24, 40);
        memcpy(serial, data + 4, 20);
        memcpy(firmware, data + 64, 8);
        
        model[40] = 0;
        serial[20] = 0;
        firmware[8] = 0;
        
        printf("NVMe Device: %s\n", model);
        printf("  Serial: %s\n", serial);
        printf("  Firmware: %s\n", firmware);
        
        free(identify_buffer);
        return true;
    }
    
    bool readSectors(uint64_t lba, uint32_t count, void* buffer) {
        NVMeCommand cmd = {0};
        cmd.cdw0 = 0x02;  // Read opcode
        cmd.nsid = 1;      // Namespace 1
        cmd.prp1 = (uint64_t)buffer;
        cmd.cdw10 = lba & 0xFFFFFFFF;
        cmd.cdw11 = (lba >> 32) & 0xFFFFFFFF;
        cmd.cdw12 = count - 1;  // Number of blocks
        
        return submitCommand(&cmd) >= 0;
    }
    
    bool writeSectors(uint64_t lba, uint32_t count, void* buffer) {
        NVMeCommand cmd = {0};
        cmd.cdw0 = 0x01;  // Write opcode
        cmd.nsid = 1;
        cmd.prp1 = (uint64_t)buffer;
        cmd.cdw10 = lba & 0xFFFFFFFF;
        cmd.cdw11 = (lba >> 32) & 0xFFFFFFFF;
        cmd.cdw12 = count - 1;
        
        return submitCommand(&cmd) >= 0;
    }
    
private:
    int submitCommand(NVMeCommand* cmd) {
        NVMeRegisters* regs = (NVMeRegisters*)reg_base;
        
        // Get submission queue tail doorbell
        volatile uint32_t* sq_tail = (uint32_t*)((uint8_t*)reg_base + 0x1000);
        
        // Submit command
        memcpy((void*)regs->asq, cmd, sizeof(NVMeCommand));
        *sq_tail = 1;
        
        // Wait for completion
        NVMeCompletion* cq = (NVMeCompletion*)regs->acq;
        auto start = std::chrono::steady_clock::now();
        
        while(true) {
            if(cq->status & 0x01) {  // Phase tag
                break;
            }
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if(elapsed.count() > 5000) {
                return -1;  // Timeout
            }
            
            usleep(100);
        }
        
        // Update completion queue head
        volatile uint32_t* cq_head = (uint32_t*)((uint8_t*)reg_base + 0x1004);
        *cq_head = 1;
        
        return (cq->status >> 1) & 0x7FFF;  // Return status code
    }
};
```

### PCIe DMA Engine
```cpp
class PCIeDMA {
private:
    int dma_fd;
    void* dma_buffer;
    size_t buffer_size;
    uint64_t pcie_address;
    
    struct DMAChannel {
        uint32_t control;
        uint32_t status;
        uint64_t src_addr;
        uint64_t dst_addr;
        uint64_t transfer_size;
        uint32_t stride;
        uint32_t next;
    } __attribute__((packed));
    
    DMAChannel* channels[8];
    
public:
    PCIeDMA(size_t size = 64 * 1024 * 1024) : buffer_size(size) {
        // Allocate DMA buffer
        dma_fd = open("/dev/dma_heap/linux,cma", O_RDWR);
        if(dma_fd >= 0) {
            struct dma_heap_allocation_data alloc = {
                .len = size,
                .fd_flags = O_RDWR | O_CLOEXEC,
                .heap_flags = 0,
            };
            ioctl(dma_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
            close(dma_fd);
            
            dma_fd = alloc.fd;
            dma_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, dma_fd, 0);
        } else {
            posix_memalign(&dma_buffer, 4096, size);
        }
        
        // Map PCIe DMA channels
        int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
        void* pcie_regs = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE,
                               MAP_SHARED, mem_fd, 0x13E0000);
        
        for(int i = 0; i < 8; i++) {
            channels[i] = (DMAChannel*)((uint8_t*)pcie_regs + 0x10000 + i * 0x20);
        }
        close(mem_fd);
    }
    
    ~PCIeDMA() {
        if(dma_buffer) {
            munmap(dma_buffer, buffer_size);
            if(dma_fd >= 0) close(dma_fd);
        }
    }
    
    void* getBuffer() {
        return dma_buffer;
    }
    
    bool transferToDevice(uint64_t device_addr, size_t size, int channel = 0) {
        if(size > buffer_size) return false;
        
        // Configure DMA channel
        channels[channel]->control = 0;
        channels[channel]->src_addr = (uint64_t)dma_buffer;
        channels[channel]->dst_addr = device_addr;
        channels[channel]->transfer_size = size;
        channels[channel]->stride = 0;
        channels[channel]->next = 0;
        
        // Start transfer
        channels[channel]->control = 0x80000000;  // Enable + start
        
        // Wait for completion
        auto start = std::chrono::steady_clock::now();
        while(channels[channel]->status & 0x80000000) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if(elapsed.count() > 5000) {
                return false;  // Timeout
            }
        }
        
        return true;
    }
    
    bool transferFromDevice(uint64_t device_addr, size_t size, int channel = 0) {
        if(size > buffer_size) return false;
        
        channels[channel]->control = 0;
        channels[channel]->src_addr = device_addr;
        channels[channel]->dst_addr = (uint64_t)dma_buffer;
        channels[channel]->transfer_size = size;
        channels[channel]->stride = 0;
        channels[channel]->next = 0;
        
        channels[channel]->control = 0x80000000;
        
        auto start = std::chrono::steady_clock::now();
        while(channels[channel]->status & 0x80000000) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if(elapsed.count() > 5000) {
                return false;
            }
        }
        
        return true;
    }
    
    void enableScatterGather(const std::vector<uint64_t>& sg_list) {
        // Build scatter-gather list
        uint64_t* sg_buffer = (uint64_t*)dma_buffer;
        for(size_t i = 0; i < sg_list.size(); i++) {
            sg_buffer[i] = sg_list[i];
        }
        
        // Configure for scatter-gather
        channels[0]->stride = 8;  // 64-bit entries
        channels[0]->next = (uint64_t)sg_buffer;
    }
};
```

### PCIe Hotplug Support
```cpp
class PCIeHotplug {
private:
    std::string slot_path;
    std::thread monitor_thread;
    std::atomic<bool> monitoring;
    std::function<void(bool)> callback;
    
    bool getSlotStatus() {
        std::ifstream file(slot_path + "/power_status");
        std::string status;
        std::getline(file, status);
        return status == "on";
    }
    
public:
    PCIeHotplug(const std::string& slot = "/sys/bus/pci/slots/0") 
        : slot_path(slot), monitoring(false) {}
    
    void onHotplug(std::function<void(bool connected)> cb) {
        callback = cb;
    }
    
    void startMonitoring() {
        monitoring = true;
        monitor_thread = std::thread([this]() {
            bool last_status = getSlotStatus();
            
            while(monitoring) {
                bool current_status = getSlotStatus();
                
                if(current_status != last_status) {
                    if(callback) {
                        callback(current_status);
                    }
                    last_status = current_status;
                    
                    if(current_status) {
                        // Rescan PCIe bus
                        std::ofstream rescan("/sys/bus/pci/rescan");
                        rescan << "1";
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });
    }
    
    void stopMonitoring() {
        monitoring = false;
        if(monitor_thread.joinable()) {
            monitor_thread.join();
        }
    }
    
    bool powerOff() {
        std::ofstream power(slot_path + "/power");
        if(power.is_open()) {
            power << "0";
            return true;
        }
        return false;
    }
    
    bool powerOn() {
        std::ofstream power(slot_path + "/power");
        if(power.is_open()) {
            power << "1";
            return true;
        }
        return false;
    }
};
```

### AI Accelerator Interface (Example: Google Coral Edge TPU)
```cpp
class CoralTPU {
private:
    int fd;
    void* reg_base;
    PCIeDMA dma;
    
    struct TPURegisters {
        uint32_t status;
        uint32_t control;
        uint32_t interrupt;
        uint32_t model_address;
        uint32_t input_address;
        uint32_t output_address;
        uint32_t input_size;
        uint32_t output_size;
        uint32_t inference_time;
    } __attribute__((packed));
    
public:
    CoralTPU(const PCIDevice& device) : dma(64 * 1024 * 1024) {
        // Map device registers
        int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
        reg_base = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                       MAP_SHARED, mem_fd, device.bar[0] & ~0x0F);
        close(mem_fd);
        
        // Check device status
        TPURegisters* regs = (TPURegisters*)reg_base;
        if(regs->status != 0x545055) {  // "TPU" magic number
            throw std::runtime_error("Coral TPU not detected");
        }
    }
    
    ~CoralTPU() {
        if(reg_base) munmap(reg_base, 4096);
    }
    
    bool loadModel(const uint8_t* model_data, size_t model_size) {
        // Copy model to DMA buffer
        memcpy(dma.getBuffer(), model_data, model_size);
        
        // Transfer to TPU
        TPURegisters* regs = (TPURegisters*)reg_base;
        regs->model_address = (uint32_t)(uint64_t)dma.getBuffer();
        
        // Wait for model load
        auto start = std::chrono::steady_clock::now();
        while(!(regs->status & 0x02)) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if(elapsed.count() > 10000) {
                return false;  // Timeout
            }
            usleep(1000);
        }
        
        return true;
    }
    
    bool runInference(const uint8_t* input, size_t input_size,
                     uint8_t* output, size_t output_size) {
        TPURegisters* regs = (TPURegisters*)reg_base;
        
        // Copy input to DMA buffer
        memcpy(dma.getBuffer(), input, input_size);
        
        // Configure inference
        regs->input_address = (uint32_t)(uint64_t)dma.getBuffer();
        regs->output_address = (uint32_t)(uint64_t)dma.getBuffer() + input_size;
        regs->input_size = input_size;
        regs->output_size = output_size;
        
        // Start inference
        regs->control = 0x01;
        
        // Wait for completion
        auto start = std::chrono::steady_clock::now();
        while(!(regs->status & 0x01)) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if(elapsed.count() > 5000) {
                return false;
            }
            usleep(100);
        }
        
        // Copy output from DMA buffer
        memcpy(output, (uint8_t*)dma.getBuffer() + input_size, output_size);
        
        uint32_t inference_time = regs->inference_time;
        printf("Inference time: %d us\n", inference_time);
        
        return true;
    }
};
```

## Performance Optimization

### PCIe Bandwidth Test
```cpp
class PCIeBandwidthTest {
private:
    PCIeDMA dma;
    std::chrono::steady_clock::time_point start_time;
    
public:
    PCIeBandwidthTest() : dma(256 * 1024 * 1024) {}  // 256MB buffer
    
    double testWriteBandwidth(uint64_t device_addr, size_t size) {
        // Fill buffer with pattern
        uint64_t* buffer = (uint64_t*)dma.getBuffer();
        for(size_t i = 0; i < size / sizeof(uint64_t); i++) {
            buffer[i] = i;
        }
        
        start_time = std::chrono::steady_clock::now();
        
        if(!dma.transferToDevice(device_addr, size)) {
            return 0.0;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        
        return (size / elapsed) / (1024.0 * 1024.0 * 1024.0);  // GB/s
    }
    
    double testReadBandwidth(uint64_t device_addr, size_t size) {
        start_time = std::chrono::steady_clock::now();
        
        if(!dma.transferFromDevice(device_addr, size)) {
            return 0.0;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        
        return (size / elapsed) / (1024.0 * 1024.0 * 1024.0);
    }
};
```

## Troubleshooting Guide

### Common Issues

1. **Device not detected**
   - Check PCIe link status: `lspci -vvv`
   - Verify power supply to M.2 slot
   - Check device tree configuration

2. **Poor performance**
   - Verify link speed: `lspci -vvv | grep LnkSta`
   - Check for ASPM being enabled
   - Monitor thermal throttling

3. **DMA errors**
   - Check IOMMU configuration
   - Verify memory allocation alignment
   - Check for address translation errors

### Debug Commands
```bash
# List PCIe devices
lspci
lspci -vvv
lspci -t

# Check PCIe link status
lspci -s 00:00.0 -vvv | grep Lnk

# Monitor PCIe errors
watch -n 1 cat /sys/bus/pci/devices/0000:00:00.0/aer_stats/*

# Reset PCIe device
echo 1 > /sys/bus/pci/devices/0000:01:00.0/remove
echo 1 > /sys/bus/pci/rescan
```

## Best Practices

1. **Use DMA for large transfers** (> 4KB)
2. **Align buffers to 4KB pages** for DMA
3. **Enable IOMMU** for device isolation
4. **Monitor PCIe errors** in production
5. **Use MSI-X interrupts** for better performance
6. **Implement proper power management** for mobile applications
7. **Use scatter-gather lists** for non-contiguous buffers
8. **Test with maximum throughput** to ensure thermal stability

## Industrial Applications

### NVMe RAID Configuration
```cpp
class NVMeRAID0 {
private:
    std::vector<NVMeController> drives;
    size_t stripe_size;
    
public:
    NVMeRAID0(const std::vector<PCIDevice>& devices, size_t stripe = 128 * 1024) 
        : stripe_size(stripe) {
        for(const auto& dev : devices) {
            drives.emplace_back(dev);
        }
    }
    
    bool readSectors(uint64_t lba, uint32_t count, void* buffer) {
        uint32_t drive_count = drives.size();
        uint32_t per_drive = count / drive_count;
        uint32_t remainder = count % drive_count;
        
        std::vector<std::thread> threads;
        uint8_t* buf = (uint8_t*)buffer;
        
        for(uint32_t i = 0; i < drive_count; i++) {
            uint32_t start_lba = lba + (i * per_drive);
            uint32_t read_count = per_drive + (i < remainder ? 1 : 0);
            
            threads.emplace_back([this, i, start_lba, read_count, buf]() {
                drives[i].readSectors(start_lba, read_count, 
                                      buf + i * per_drive * 512);
            });
        }
        
        for(auto& thread : threads) {
            thread.join();
        }
        
        return true;
    }
    
    double getTotalBandwidth() {
        double total = 0;
        for(auto& drive : drives) {
            total += 3.5;  // 3.5 GB/s per NVMe drive
        }
        return total;
    }
};
