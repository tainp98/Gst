#ifndef RECORDTEE_H
#define RECORDTEE_H
#include <gst/gst.h>
#include <unistd.h>
#include <iostream>
static GstPadProbeReturn event_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
static GstPadProbeReturn pad_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
static GstPadProbeReturn pad_probe_cb2(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
static gboolean timeout_cb(gpointer user_data);
static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer data);
static void on_pad_added(GstElement *element, GstPad *pad, gpointer data);
class RecordTee
{
public:
    RecordTee();
    ~RecordTee();
    void init();
    void start();
private:
    bool recording_ = true;
    int count_ = 0;
    gulong pad_probe_id_ = 0;
    std::string filePath_;
    GMainLoop *loop_;
    GstElement *pipeline_, *tee_, *videoQueue_, *fileQueue_;
    GstElement *src_, *decodebin_, *videoConvert1_, *nvh265enc_, *h265parse_;
    GstElement *avdecH265_, *mpegtsmux_, *videoConvert2_;
    GstElement *videoSink_, *fileSink_;

    GstPad *teeVideoPad_, *teeFilePad_;
    friend GstPadProbeReturn event_probe_cb(GstPad *, GstPadProbeInfo *, gpointer);
    friend GstPadProbeReturn pad_probe_cb(GstPad *, GstPadProbeInfo *, gpointer);
    friend GstPadProbeReturn pad_probe_cb2(GstPad *, GstPadProbeInfo *, gpointer);
    friend gboolean timeout_cb(gpointer);
    friend gboolean bus_cb(GstBus *, GstMessage *, gpointer);
    friend void on_pad_added(GstElement *element, GstPad *pad, gpointer data);
};

#endif // RECORDTEE_H
