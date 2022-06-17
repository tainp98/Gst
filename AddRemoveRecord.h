#ifndef ADDREMOVERECORD_H
#define ADDREMOVERECORD_H
#include <gst/gst.h>
#include <unistd.h>
#include <iostream>
#include <mutex>
class AddRemoveRecord
{
public:

    AddRemoveRecord();
    ~AddRemoveRecord();
    void init(int argc, char** argv);
    void start();
    static GstPadProbeReturn wrapBlockPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    static GstPadProbeReturn wrapDataPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    static gboolean wrapTimeoutCb(gpointer user_data);
    static void wrapNeedDataCb (GstElement *appsrc, guint unused_size, gpointer user_data);
    static gboolean wrapBusCb(GstBus *bus, GstMessage *msg, gpointer data);
    static void wrapPadAdded(GstElement *element, GstPad *pad, gpointer data);
    void stopRecording();
    void startRecording();
private:
    static gboolean printField (GQuark field, const GValue * value, gpointer pfx);
    static void printCaps (const GstCaps * caps, const gchar * pfx);
private:
    static inline AddRemoveRecord* AddRemoveRecord_ = nullptr;
    static inline std::mutex mux_;
    bool recording_ = false;
    int count_ = 0;
    guint width_ = 640, height_ = 480;
    gulong padProbeId_ = 0;
    std::string filePath_;
    std::string fileName_;
    GMainLoop *loop_;
    GstElement *pipeline_, *tee_, *videoQueue_, *fileQueue_;
    GstElement *src_, *decodebin_, *videoConvert1_, *nvh265enc_, *h265parse_, *qtdemux_, *h264parse_, *omxh264dec_, *appSrc_;
    GstElement *avdecH265_, *mpegtsmux_, *videoConvert2_, *filterCaps_, *filterCaps1_, *rtph265pay_, *udpSink_;
    GstElement *videoSink_, *fileSink_;

    GstPad *teeVideoPad_, *teeFilePad_, *mpegSinkPad_;
    GstPadProbeReturn onBlockPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    GstPadProbeReturn onDataPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    gboolean onTimeoutCb(gpointer user_data);
    void onNeedDataCb (GstElement *appsrc, guint unused_size, gpointer user_data);
    gboolean onBusCb(GstBus *bus, GstMessage *msg, gpointer data);
    void onPadAdded(GstElement *element, GstPad *pad, gpointer data);
};

#endif // ADDREMOVERECORD_H
