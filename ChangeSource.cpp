#include "ChangeSource.h"
ChangeSource::ChangeSource()
{

}

void ChangeSource::startChangeSource()
{
    std::cout << "=============================================================\n";
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_READY);
    std::cout << "ret = " << ret << "\n";
    if(changeSource_ % 3 == 0)
    {
        g_object_set(G_OBJECT(src_), "location", "/home/vietph/Videos/videoplayback.mp4", NULL);
    }
    else if (changeSource_ % 3 == 1)
    {
        g_object_set(G_OBJECT(src_), "location", "/home/vietph/Videos/video/ir.m4e", NULL);
    }
    else if (changeSource_ % 3 == 2)
    {
        g_object_set(G_OBJECT(src_), "location", "/home/vietph/Videos/vd1.avi", NULL);
    }
    changeSource_++;
    ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    std::cout << "ret = " << ret << "\n";
    recording_ = !recording_;
}

gboolean ChangeSource::printField(GQuark field, const GValue *value, gpointer pfx)
{
    gchar *str = gst_value_serialize (value);
    g_print ("%s  %15s: %s\n", (gchar *) pfx, g_quark_to_string (field), str);
    g_free (str);
    return TRUE;
}

void ChangeSource::printCaps(const GstCaps *caps, const gchar *pfx)
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

GstPadProbeReturn ChangeSource::onDataPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    GstCaps *caps = gst_pad_get_current_caps (pad);
    printCaps(caps, "      ");
    gst_caps_unref (caps);
    return GST_PAD_PROBE_PASS;
}

gboolean ChangeSource::onTimeoutCb(gpointer user_data)
{
    this->startChangeSource();
    return TRUE;
}

gboolean ChangeSource::onBusCb(GstBus *bus, GstMessage *msg, gpointer data)
{
    //    std::cout << "in bus_cb : message type = " << GST_MESSAGE_TYPE(msg) << ", name = "
    //              << gst_element_get_name(msg->src) << "\n";
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
    {
        g_print("End of stream\n");
        g_main_loop_quit(loop_);
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

void ChangeSource::onPadAdded(GstElement *element, GstPad *pad, gpointer data)
{
    decodeSrcPad_ = pad;
    GstPad *sinkpad;
    GstElement *decoder = (GstElement*)nvh265enc_;
    g_print("Dynamic pad created, linking decodebin/encoder\n");
    sinkpad = gst_element_get_static_pad(decoder, "sink");
    gst_pad_link(pad, sinkpad);
    std::cout << "Encoder SinkPad Caps:\n";
    gst_pad_add_probe(sinkpad, GST_PAD_PROBE_TYPE_BUFFER,
                                          wrapDataPadProbeCb, this, nullptr);
    gst_object_unref(sinkpad);
}

ChangeSource::~ChangeSource()
{

}

void ChangeSource::init(int argc, char** argv)
{
    if(argc < 3)
    {
        g_print("Input HostIP and Port To UDPSink\n");
        exit(1);
    }
    filePath_ = "video";
    src_ = gst_element_factory_make("filesrc", "src");
    g_object_set(src_, "location", "/home/vietph/Videos/vd1.avi", NULL);
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

    rtph265pay_ = gst_element_factory_make("rtph265pay", "rtph265pay");
    udpSink_ = gst_element_factory_make("udpsink", "udpsink");
    g_object_set(rtph265pay_, "mtu", 4096,
                 "config-interval", 1, nullptr);
    g_object_set(udpSink_, "auto-multicast", TRUE,
                 "host", argv[1],
            "port", atoi(argv[2]),
            "async", TRUE, "sync", TRUE, nullptr);

    // create pipeline
    pipeline_ = gst_pipeline_new("pipe");
    gst_bin_add_many(GST_BIN(pipeline_), src_, decodebin_, nvh265enc_, h265parse_,
                     rtph265pay_, udpSink_, nullptr);
    if(gst_element_link_many(src_, decodebin_, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked1\n");
        return;
    }

    if(gst_element_link_many(nvh265enc_, h265parse_, rtph265pay_, udpSink_, nullptr) != TRUE)
    {
        g_printerr("Elements could not be linked2\n");
        return;
    }
    g_signal_connect (decodebin_, "pad-added", G_CALLBACK (wrapPadAdded), this);
}

void ChangeSource::start()
{
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    loop_ = g_main_loop_new(nullptr, FALSE);
    gst_bus_add_watch(GST_ELEMENT_BUS(pipeline_), wrapBusCb, this);
    g_timeout_add_seconds(10, wrapTimeoutCb, this);
    g_main_loop_run(loop_);
}

GstPadProbeReturn ChangeSource::wrapDataPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    ChangeSource *changeSource = static_cast<ChangeSource*>(user_data);
    return changeSource->onDataPadProbeCb(pad, info, user_data);
}

gboolean ChangeSource::wrapTimeoutCb(gpointer user_data)
{
    ChangeSource *changeSource = static_cast<ChangeSource*>(user_data);
    return changeSource->onTimeoutCb(user_data);
}

gboolean ChangeSource::wrapBusCb(GstBus *bus, GstMessage *msg, gpointer data)
{
    ChangeSource *changeSource = static_cast<ChangeSource*>(data);
    return changeSource->onBusCb(bus, msg, data);
}

void ChangeSource::wrapPadAdded(GstElement *element, GstPad *pad, gpointer data)
{
    ChangeSource *changeSource = static_cast<ChangeSource*>(data);
    return changeSource->onPadAdded(element, pad, data);
}
