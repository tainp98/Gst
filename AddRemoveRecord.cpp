#include "AddRemoveRecord.h"
AddRemoveRecord::AddRemoveRecord()
{

}

void AddRemoveRecord::stopRecording()
{
    padProbeId_ = gst_pad_add_probe(teeFilePad_,GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                                      wrapBlockPadProbeCb, this, nullptr);
    std::cout << "Stop Record " << fileName_ << "\n";
    recording_ = false;
}

void AddRemoveRecord::startRecording()
{

    fileQueue_ = gst_element_factory_make("queue", nullptr);
    mpegtsmux_ = gst_element_factory_make("mp4mux", nullptr);
    fileSink_ = gst_element_factory_make("filesink", nullptr);

    fileName_ = filePath_;
    fileName_  = fileName_ + std::to_string(++count_) + ".mp4";
    g_object_set(fileSink_, "location", fileName_.c_str(), nullptr);
    gst_bin_add_many(GST_BIN(pipeline_), fileQueue_,
                     mpegtsmux_, fileSink_, nullptr);
    gst_element_link_many(mpegtsmux_,
                          fileSink_, nullptr);
    teeFilePad_ = gst_element_get_request_pad(tee_, "src_%u");
    GstPad *queue_video_pad;
    GstPadLinkReturn ret;

    queue_video_pad = gst_element_get_static_pad(fileQueue_, "sink");
    ret = gst_pad_link(teeFilePad_, queue_video_pad);
    std::cout << "GstPadLinkReturn srcTee - sinkQueue = " << ret << "\n";

    mpegSinkPad_ = gst_element_get_request_pad(mpegtsmux_, "video_%u");
    queue_video_pad = gst_element_get_static_pad(fileQueue_, "src");
    ret = gst_pad_link(queue_video_pad, mpegSinkPad_);
    std::cout << "GstPadLinkReturn srcQueue - sinkMux = " << ret << "\n";

    GstPad *muxsinkpad = gst_element_get_static_pad(fileQueue_, "src");
    gst_pad_add_probe(muxsinkpad, GST_PAD_PROBE_TYPE_BUFFER, static_cast<GstPadProbeCallback>(wrapDataPadProbeCb), nullptr, nullptr);
    gst_object_unref(queue_video_pad);
    gst_object_unref(muxsinkpad);

    gst_element_set_state(fileQueue_, GST_STATE_PLAYING);
    gst_element_set_state(mpegtsmux_, GST_STATE_PLAYING);
    gst_element_set_state(fileSink_, GST_STATE_PLAYING);
    std::cout << "Start Record " << fileName_ << "\n";
    std::cout << "FileQueue Src: " << "\n";
    recording_ = true;
}

gboolean AddRemoveRecord::printField(GQuark field, const GValue *value, gpointer pfx)
{
    gchar *str = gst_value_serialize (value);
    g_print ("%s  %15s: %s\n", (gchar *) pfx, g_quark_to_string (field), str);
    g_free (str);
    return TRUE;
}

void AddRemoveRecord::printCaps(const GstCaps *caps, const gchar *pfx)
{
    guint i;

    g_return_if_fail (caps != nullptr);

    if (gst_caps_is_any (caps)) {
        g_print ("%sANY\n", pfx);
        return;
    }
    if (gst_caps_is_empty (caps)) {
        g_print ("%sEMPTY\n", pfx);
        return;
    }

    for (i = 0; i < gst_caps_get_size (caps); i++) {
        GstStructure *structure = gst_caps_get_structure (caps, i);
        g_print ("%s%s\n", pfx, gst_structure_get_name (structure));
        gst_structure_foreach (structure, printField, (gpointer) pfx);
    }
}

GstPadProbeReturn AddRemoveRecord::onBlockPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    GstPad *sinkpad;

    sinkpad = gst_element_get_static_pad(fileQueue_, "sink");
    gst_pad_unlink(teeFilePad_, sinkpad);
    gst_element_release_request_pad(tee_, teeFilePad_);
    gst_object_unref(teeFilePad_);
    sleep(1);
    gst_element_send_event(mpegtsmux_, gst_event_new_eos());
    //    gst_pad_send_event(AddRemoveRecord::AddRemoveRecord()->mpegSinkPad_, gst_event_new_eos());
    gst_object_unref(sinkpad);
    usleep(1000);

    gst_element_set_state(fileQueue_, GST_STATE_NULL);
    gst_element_set_state(mpegtsmux_, GST_STATE_NULL);
    gst_element_set_state(fileSink_, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(pipeline_), fileQueue_);
    gst_bin_remove(GST_BIN(pipeline_), mpegtsmux_);
    gst_bin_remove(GST_BIN(pipeline_), fileSink_);
    return GST_PAD_PROBE_REMOVE;
}

GstPadProbeReturn AddRemoveRecord::onDataPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    GstCaps *caps = gst_pad_get_current_caps (pad);
    printCaps(caps, "      ");
    gst_caps_unref (caps);
    return GST_PAD_PROBE_PASS;
}

gboolean AddRemoveRecord::onTimeoutCb(gpointer user_data)
{
    if(recording_)
    {
        this->stopRecording();
    }
    else
    {
        this->startRecording();
    }
    return TRUE;
}

void AddRemoveRecord::onNeedDataCb(GstElement *appsrc, guint unused_size, gpointer user_data)
{
    static gboolean white = FALSE;
    static GstClockTime timestamp = 0;
    static uint8_t value = 0;
    static guint xRect = 2, yRect = 2, widthRect = 50, heightRect = 40;
    GstBuffer *buffer;
    guint graySize, uySize;
    GstFlowReturn ret;

    graySize = width_ * height_;
    uySize = static_cast<guint>(width_*height_*0.5);

    buffer = gst_buffer_new_allocate (nullptr, graySize+uySize, nullptr);
    /* this makes the image black/white */
//    gst_buffer_memset (buffer, 0, white ? 0xff : 0x0, size);
    gst_buffer_memset (buffer, 0, 0x8f, graySize);
    gst_buffer_memset (buffer, graySize, 0x80, uySize);

    gst_buffer_memset(buffer, yRect*width_+xRect, 0x00, widthRect);
    gst_buffer_memset(buffer, (yRect+heightRect)*width_+xRect, 0x00, widthRect);

    gst_buffer_memset(buffer, (yRect*width_+xRect)/2 + graySize, 0x00, widthRect);
    gst_buffer_memset(buffer, ((yRect+heightRect)*width_+xRect)/2 + graySize, 0x00, widthRect);
    for(guint i = 1; i < heightRect; i++)
    {
        gst_buffer_memset(buffer, (yRect+i)*width_+xRect, 0x00, 1);
        gst_buffer_memset(buffer, (yRect+i)*width_+xRect + widthRect, 0x00, 1);

        gst_buffer_memset(buffer, ((yRect+i)*width_+xRect)/2 + graySize, 0x00, 1);
        gst_buffer_memset(buffer, ((yRect+i)*width_+xRect + widthRect)/2 + graySize, 0x00, 1);
    }
    if(xRect < width_-widthRect)
    {
        xRect += 2;
    }
    else
    {
        xRect = 2;
    }
    if(yRect < height_-heightRect)
    {
        yRect += 2;
    }
    else
    {
        yRect = 2;
    }

    white = !white;

    GST_BUFFER_PTS (buffer) = timestamp;
    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 2);

    timestamp += GST_BUFFER_DURATION (buffer);
    g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref (buffer);

    if (ret != GST_FLOW_OK) {
        /* something wrong, stop pushing */
        g_main_loop_quit (loop_);
    }
}

gboolean AddRemoveRecord::onBusCb(GstBus *bus, GstMessage *msg, gpointer data)
{
    //    std::cout << "in bus_cb : message type = " << GST_MESSAGE_TYPE(msg) << ", name = "
    //              << gst_element_get_name(msg->src) << "\n";
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
    {
        g_print("End of stream\n");
        break;
    }
    case GST_MESSAGE_ERROR:
    {
        gchar *debug;
        GError *error;
        gst_message_parse_error(msg, &error, &debug);
        g_free(debug);
        g_printerr("Error: %s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(loop_);
        break;
    }
    default:
    {
        break;
    }
    }
    return TRUE;
}

void AddRemoveRecord::onPadAdded(GstElement *element, GstPad *pad, gpointer data)
{
    GstPad *sinkpad;
    GstElement *decoder = (GstElement*)nvh265enc_;
    g_print("Dynamic pad created, linking demuxer/decoder\n");
    sinkpad = gst_element_get_static_pad(decoder, "sink");
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
}

AddRemoveRecord::~AddRemoveRecord()
{

}

void AddRemoveRecord::init(int argc, char** argv)
{
    if(argc < 3)
    {
        g_print("Input HostIP and Port To UDPSink\n");
        exit(1);
    }
    filePath_ = "video";
    src_ = gst_element_factory_make("filesrc", "src");
    g_object_set(src_, "location", "/home/root/tainp/videoplayback.mp4", nullptr);
    //    g_object_set(src_, "location", "/home/vietph/Videos/videoplayback.mp4", NULL);
    decodebin_ = gst_element_factory_make("decodebin", "decode");
    videoConvert1_ = gst_element_factory_make("videoconvert", "video-convert1");
        nvh265enc_ = gst_element_factory_make("nvh265enc", NULL);
    h265parse_ = gst_element_factory_make("h265parse", "h265parse");
    g_object_set(h265parse_, "config-interval", 1, nullptr);
    tee_ = gst_element_factory_make("tee", "tee");
    videoQueue_ = gst_element_factory_make("queue", nullptr);
    avdecH265_ = gst_element_factory_make("avdec_h265", nullptr);
    videoConvert2_ = gst_element_factory_make("videoconvert", nullptr);
    videoSink_ = gst_element_factory_make("autovideosink", nullptr);
    appSrc_ = gst_element_factory_make ("appsrc", "source");
    filterCaps1_ = gst_element_factory_make("capsfilter", nullptr);
    filterCaps_ = gst_element_factory_make("capsfilter", nullptr);
    GstCaps *caps;
    caps = gst_caps_new_simple ("video/x-h265",
                                "stream-format", G_TYPE_STRING, "hvc1",
                                NULL);
    g_object_set (G_OBJECT (filterCaps1_), "caps", caps, nullptr);

    rtph265pay_ = gst_element_factory_make("rtph265pay", "rtph265pay");
    udpSink_ = gst_element_factory_make("udpsink", "udpsink");
    g_object_set(rtph265pay_, "mtu", 4096,
                 "config-interval", 1, nullptr);
    g_object_set(udpSink_, "auto-multicast", TRUE,
                 "host", argv[1],
                 "port", atoi(argv[2]),
                 "async", TRUE, "sync", TRUE, nullptr);
    gst_caps_unref (caps);
    /* setup */
    g_object_set (G_OBJECT (appSrc_), "caps",
                  gst_caps_new_simple (
                      "video/x-raw",
                      "format", G_TYPE_STRING, "I420",
                      "width", G_TYPE_INT, 640,
                      "height", G_TYPE_INT, 480,
                      "framerate", GST_TYPE_FRACTION, 50, 1,
                      nullptr), nullptr);
    g_object_set (G_OBJECT (appSrc_),
                  "stream-type", 0,
                  "format", GST_FORMAT_TIME, nullptr);

    // create pipeline
    pipeline_ = gst_pipeline_new("pipe");
    gst_bin_add_many(GST_BIN(pipeline_), appSrc_, videoConvert1_, /*src_, decodebin_,*/ nvh265enc_, h265parse_,
                     tee_, videoQueue_,filterCaps1_, rtph265pay_, udpSink_/*,avdecH265_, videoConvert2_, videoSink_*/,
                     nullptr);
    //    if(gst_element_link_many(src_, decodebin_, NULL) != TRUE)
    //    {
    //        g_printerr("Elements could not be linked1\n");
    //        return;
    //    }

    if(gst_element_link_many(appSrc_, videoConvert1_, nvh265enc_, h265parse_, tee_, nullptr) != TRUE)
    {
        g_printerr("Elements could not be linked2\n");
        return;
    }
    //    g_signal_connect (decodebin_, "pad-added", G_CALLBACK (on_pad_added), this);

    if(gst_element_link_many(videoQueue_, filterCaps1_, rtph265pay_, udpSink_,/*avdecH265_, videoConvert2_, videoSink_,*/ nullptr) != TRUE)
    {
        g_printerr("Elements could not be linked3\n");
        return;
    }

    // Manually link the tee
    teeVideoPad_ = gst_element_get_request_pad(tee_, "src_%u");
    GstPad *queuePad = gst_element_get_static_pad(videoQueue_, "sink");
    gst_pad_link(teeVideoPad_, queuePad);
    gst_object_unref(queuePad);
    g_signal_connect (appSrc_, "need-data", G_CALLBACK (wrapNeedDataCb), this);
}

void AddRemoveRecord::start()
{
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    loop_ = g_main_loop_new(nullptr, FALSE);
    gst_bus_add_watch(GST_ELEMENT_BUS(pipeline_), wrapBusCb, this);
    g_timeout_add_seconds(10, wrapTimeoutCb, this);
    g_main_loop_run(loop_);
}

GstPadProbeReturn AddRemoveRecord::wrapBlockPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    AddRemoveRecord *addRemoveRecord = static_cast<AddRemoveRecord*>(user_data);
    return addRemoveRecord->onBlockPadProbeCb(pad, info, user_data);
}

GstPadProbeReturn AddRemoveRecord::wrapDataPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    AddRemoveRecord *addRemoveRecord = static_cast<AddRemoveRecord*>(user_data);
    return addRemoveRecord->onDataPadProbeCb(pad, info, user_data);
}

gboolean AddRemoveRecord::wrapTimeoutCb(gpointer user_data)
{
    AddRemoveRecord *addRemoveRecord = static_cast<AddRemoveRecord*>(user_data);
    return addRemoveRecord->onTimeoutCb(user_data);
}

void AddRemoveRecord::wrapNeedDataCb(GstElement *appsrc, guint unused_size, gpointer user_data)
{
    AddRemoveRecord *addRemoveRecord = static_cast<AddRemoveRecord*>(user_data);
    return addRemoveRecord->onNeedDataCb(appsrc, unused_size, user_data);
}

gboolean AddRemoveRecord::wrapBusCb(GstBus *bus, GstMessage *msg, gpointer data)
{
    AddRemoveRecord *addRemoveRecord = static_cast<AddRemoveRecord*>(data);
    return addRemoveRecord->onBusCb(bus, msg, data);
}

void AddRemoveRecord::wrapPadAdded(GstElement *element, GstPad *pad, gpointer data)
{
    AddRemoveRecord *addRemoveRecord = static_cast<AddRemoveRecord*>(data);
    return addRemoveRecord->onPadAdded(element, pad, data);
}

