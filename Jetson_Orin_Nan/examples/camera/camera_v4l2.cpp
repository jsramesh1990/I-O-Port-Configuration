/**
 * Camera V4L2 Example
 * 
 * Demonstrates CSI camera capture using V4L2 API
 * Direct access to camera device without GStreamer
 */

#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <signal.h>

static volatile bool running = true;

void signalHandler(int signum) {
    std::cout << "\nStopping camera..." << std::endl;
    running = false;
}

class V4L2Camera {
private:
    int fd;
    void* buffers[4];
    unsigned int buffer_count;
    int width, height;
    
    bool xioctl(int request, void* arg) {
        int r;
        do {
            r = ioctl(fd, request, arg);
        } while(r == -1 && errno == EINTR);
        return r >= 0;
    }
    
public:
    V4L2Camera(const char* device) : fd(-1), buffer_count(0), width(0), height(0) {
        fd = open(device, O_RDWR);
        if(fd < 0) {
            throw std::runtime_error("Failed to open camera device");
        }
    }
    
    ~V4L2Camera() {
        if(fd >= 0) {
            close();
        }
    }
    
    bool configure(int w, int h, int fps = 30) {
        width = w;
        height = h;
        
        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = width;
        fmt.fmt.pix.height = height;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        
        if(!xioctl(VIDIOC_S_FMT, &fmt)) {
            std::cerr << "Failed to set format" << std::endl;
            return false;
        }
        
        // Set framerate
        struct v4l2_streamparm parm;
        memset(&parm, 0, sizeof(parm));
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = fps;
        xioctl(VIDIOC_S_PARM, &parm);
        
        return true;
    }
    
    bool start() {
        // Request buffers
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        
        if(!xioctl(VIDIOC_REQBUFS, &req)) {
            std::cerr << "Failed to request buffers" << std::endl;
            return false;
        }
        
        buffer_count = req.count;
        
        // Map buffers
        for(unsigned int i = 0; i < buffer_count; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            
            if(!xioctl(VIDIOC_QUERYBUF, &buf)) {
                std::cerr << "Failed to query buffer" << std::endl;
                return false;
            }
            
            buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, buf.m.offset);
            if(buffers[i] == MAP_FAILED) {
                std::cerr << "Failed to mmap buffer" << std::endl;
                return false;
            }
            
            // Queue buffer
            xioctl(VIDIOC_QBUF, &buf);
        }
        
        // Start streaming
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(!xioctl(VIDIOC_STREAMON, &type)) {
            std::cerr << "Failed to start streaming" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool captureFrame(uint8_t** data, size_t* size) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        if(!xioctl(VIDIOC_DQBUF, &buf)) {
            return false;
        }
        
        *data = (uint8_t*)buffers[buf.index];
        *size = buf.bytesused;
        
        // Requeue buffer
        xioctl(VIDIOC_QBUF, &buf);
        
        return true;
    }
    
    void close() {
        if(fd >= 0) {
            // Stop streaming
            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            xioctl(VIDIOC_STREAMOFF, &type);
            
            // Unmap buffers
            for(unsigned int i = 0; i < buffer_count; i++) {
                if(buffers[i] && buffers[i] != MAP_FAILED) {
                    munmap(buffers[i], 0);
                }
            }
            
            ::close(fd);
            fd = -1;
        }
    }
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }
};

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== Camera V4L2 Example ===" << std::endl;
    std::cout << "CSI Camera capture using V4L2" << std::endl;
    std::cout << "Resolution: 1920x1080 @ 30fps" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        V4L2Camera camera("/dev/video0");
        
        if(!camera.configure(1920, 1080, 30)) {
            std::cerr << "Failed to configure camera" << std::endl;
            return 1;
        }
        
        if(!camera.start()) {
            std::cerr << "Failed to start camera" << std::endl;
            return 1;
        }
        
        std::cout << "Camera started successfully" << std::endl;
        std::cout << "Resolution: " << camera.getWidth() << "x" << camera.getHeight() << std::endl;
        std::cout << std::endl;
        
        int frame_count = 0;
        auto start_time = std::chrono::steady_clock::now();
        
        while(running) {
            uint8_t* frame_data;
            size_t frame_size;
            
            if(camera.captureFrame(&frame_data, &frame_size)) {
                frame_count++;
                
                // Print stats every second
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
                
                if(elapsed >= 1) {
                    std::cout << "Frames captured: " << frame_count 
                              << " (" << (frame_count / elapsed) << " fps)" << std::endl;
                    start_time = now;
                    frame_count = 0;
                }
                
                // Process frame data here
                // frame_data contains YUYV format data
                // frame_size = width * height * 2 (for YUYV)
            }
        }
        
        camera.close();
        std::cout << "\nCamera stopped." << std::endl;
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
