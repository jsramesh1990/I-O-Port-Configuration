#include "camera.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

Camera::Camera(const std::string& device) : device_(device), fd_(-1), streaming_(false) {}

Camera::~Camera() {
    close();
}

bool Camera::open(const Config& config) {
    fd_ = ::open(device_.c_str(), O_RDWR);
    if (fd_ < 0) return false;
    
    config_ = config;
    
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = config.resolution.width;
    fmt.fmt.pix.height = config.resolution.height;
    fmt.fmt.pix.pixelformat = config.format ? config.format : V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (xioctl(VIDIOC_S_FMT, &fmt) < 0) {
        close();
        return false;
    }
    
    // Set framerate
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = config.fps;
    xioctl(VIDIOC_S_PARM, &parm);
    
    initMMAP();
    return true;
}

void Camera::close() {
    stopStreaming();
    
    if (fd_ >= 0) {
        // Stop streaming
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(VIDIOC_STREAMOFF, &type);
        
        for (int i = 0; i < 4; i++) {
            if (buffers_[i]) {
                munmap(buffers_[i], 0);
            }
        }
        
        ::close(fd_);
        fd_ = -1;
    }
}

void Camera::initMMAP() {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    xioctl(VIDIOC_REQBUFS, &req);
    
    for (int i = 0; i < 4; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        xioctl(VIDIOC_QUERYBUF, &buf);
        
        buffers_[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd_, buf.m.offset);
        
        xioctl(VIDIOC_QBUF, &buf);
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(VIDIOC_STREAMON, &type);
}

bool Camera::captureFrame(Frame& frame, int timeout_ms) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(VIDIOC_DQBUF, &buf) < 0) {
        return false;
    }
    
    frame.data = (uint8_t*)buffers_[buf.index];
    frame.size = buf.bytesused;
    frame.width = config_.resolution.width;
    frame.height = config_.resolution.height;
    frame.format = config_.format;
    
    struct timeval tv = buf.timestamp;
    frame.timestamp = tv.tv_sec * 1000000 + tv.tv_usec;
    
    xioctl(VIDIOC_QBUF, &buf);
    return true;
}
