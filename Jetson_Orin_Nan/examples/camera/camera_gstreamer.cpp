/**
 * Camera GStreamer Example
 * 
 * Demonstrates CSI camera capture using GStreamer pipeline
 * Uses hardware-accelerated video encoding
 */

#include <iostream>
#include <cstring>
#include <thread>
#include <signal.h>
#include <gst/gst.h>

static volatile bool running = true;
static GMainLoop* loop = nullptr;

void signalHandler(int signum) {
    std::cout << "\nStopping camera..." << std::endl;
    running = false;
    if(loop) {
        g_main_loop_quit(loop);
    }
}

// Bus callback for pipeline messages
static gboolean bus_callback(GstBus* bus, GstMessage* msg, gpointer data) {
    switch(GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug;
            gst_message_parse_error(msg, &err, &debug);
            std::cerr << "GStreamer error: " << err->message << std::endl;
            g_error_free(err);
            g_free(debug);
            running = false;
            if(loop) g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "End of stream" << std::endl;
            running = false;
            if(loop) g_main_loop_quit(loop);
            break;
        default:
            break;
    }
    return TRUE;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== Camera GStreamer Example ===" << std::endl;
    std::cout << "CSI Camera capture with hardware encoding" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    // Initialize GStreamer
    gst_init(&argc, &argv);
    
    // Create pipeline
    // nvarguscamerasrc for CSI camera, then hardware encode to H.264
    const char* pipeline_str = 
        "nvarguscamerasrc sensor-id=0 ! "
        "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1 ! "
        "nvvidconv ! "
        "video/x-raw, width=1280, height=720 ! "
        "nvvidconv ! "
        "nvv4l2h264enc bitrate=4000000 ! "
        "h264parse ! "
        "qtmux ! "
        "filesink location=output.mp4";
    
    // For display preview, use this pipeline:
    // const char* pipeline_str = 
    //     "nvarguscamerasrc sensor-id=0 ! "
    //     "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1 ! "
    //     "nvvidconv ! "
    //     "video/x-raw, width=800, height=600 ! "
    //     "nvvidconv ! "
    //     "ximagesink";
    
    std::cout << "Pipeline: " << pipeline_str << std::endl;
    std::cout << "Recording to output.mp4..." << std::endl;
    
    GstElement* pipeline = gst_parse_launch(pipeline_str, NULL);
    if(!pipeline) {
        std::cerr << "Failed to create pipeline" << std::endl;
        return 1;
    }
    
    // Set up bus monitoring
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, bus_callback, NULL);
    gst_object_unref(bus);
    
    // Start pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    // Create main loop
    loop = g_main_loop_new(NULL, FALSE);
    
    // Run for specified time or until interrupt
    std::cout << "Recording for 30 seconds..." << std::endl;
    
    // Option 1: Run for specific duration
    // g_timeout_add_seconds(30, [](gpointer data) -> gboolean {
    //     running = false;
    //     g_main_loop_quit(loop);
    //     return FALSE;
    // }, NULL);
    
    // Option 2: Run until interrupt
    g_main_loop_run(loop);
    
    // Cleanup
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    
    std::cout << "\nRecording finished." << std::endl;
    
    return 0;
}
