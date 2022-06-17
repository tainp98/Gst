#ifndef RECORDTEE_H
#define RECORDTEE_H
#include <gst/gst.h>
#include <unistd.h>
#include <iostream>
#include <mutex>
#define GST_CAPS_FEATURE_MEMORY_DMABUF "memory:DMABuf"
static GstPadProbeReturn event_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
//static GstPadProbeReturn pad_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
static GstPadProbeReturn pad_probe_cb2(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
static gboolean timeout_cb(gpointer user_data);
static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer data);
static void on_pad_added(GstElement *element, GstPad *pad, gpointer data);
class RecordTee
{
public:

    ~RecordTee();
    void init();
    void start();
    static RecordTee* recordTee();
    static GstPadProbeReturn pad_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    static GstPadProbeReturn event_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    static void cb_need_data (GstElement *appsrc, guint unused_size, gpointer user_data);
    void stopRecording();
    void startRecording();
private:
    RecordTee();
private:
    static inline RecordTee* recordTee_ = nullptr;
    static inline std::mutex mux_;
    bool recording_ = true;
    int count_ = 0;
    gulong pad_probe_id_ = 0;
    std::string filePath_;
    GMainLoop *loop_;
    GstElement *pipeline_, *tee_, *videoQueue_, *fileQueue_;
    GstElement *src_, *decodebin_, *videoConvert1_, *nvh265enc_, *h265parse_, *qtdemux_, *h264parse_, *omxh264dec_, *appSrc_;
    GstElement *avdecH265_, *mpegtsmux_, *videoConvert2_, *filterCaps_, *filterCaps1_, *rtph265pay_, *udpSink_;
    GstElement *videoSink_, *fileSink_;

    GstPad *teeVideoPad_, *teeFilePad_, *mpegSinkPad_;
    friend GstPadProbeReturn event_probe_cb(GstPad *, GstPadProbeInfo *, gpointer);
//    friend GstPadProbeReturn pad_probe_cb(GstPad *, GstPadProbeInfo *, gpointer);
    friend GstPadProbeReturn pad_probe_cb2(GstPad *, GstPadProbeInfo *, gpointer);
    friend gboolean timeout_cb(gpointer);
    friend gboolean bus_cb(GstBus *, GstMessage *, gpointer);
    friend void on_pad_added(GstElement *element, GstPad *pad, gpointer data);
};

#endif // RECORDTEE_H
