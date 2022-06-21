#include "SplitMuxSink.h"
#include <DirTool.h>
#include <map>
SplitMuxSink::SplitMuxSink()
{

}

gboolean SplitMuxSink::printField(GQuark field, const GValue *value, gpointer pfx)
{
    gchar *str = gst_value_serialize (value);
    g_print ("%s  %15s: %s\n", (gchar *) pfx, g_quark_to_string (field), str);
    g_free (str);
    return TRUE;
}

void SplitMuxSink::printCaps(const GstCaps *caps, const gchar *pfx)
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

std::string SplitMuxSink::getFileNameByTime()
{
    std::string fileName = "";
    std::time_t t = std::time(nullptr);
    std::tm *now = std::localtime(&t);
    fileName += std::to_string(now->tm_year + 1900);
    correctTimeLessThanTen(fileName, now->tm_mon + 1);
    correctTimeLessThanTen(fileName, now->tm_mday);
    correctTimeLessThanTen(fileName, now->tm_hour);
    correctTimeLessThanTen(fileName, now->tm_min);
    correctTimeLessThanTen(fileName, now->tm_sec);
    return fileName;
}

void SplitMuxSink::correctTimeLessThanTen(std::string &_inputStr, int _time)
{
    _inputStr += "_";
    if (_time < 10) {
        _inputStr += "0";
        _inputStr += std::to_string(_time);
    } else {
        _inputStr += std::to_string(_time);
    }
}

GstPadProbeReturn SplitMuxSink::onDataPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    GstCaps *caps = gst_pad_get_current_caps (pad);
    printCaps(caps, "      ");
    gst_caps_unref (caps);
    return GST_PAD_PROBE_PASS;
}

void SplitMuxSink::onNeedDataCb(GstElement *appsrc, guint unused_size, gpointer user_data)
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

gboolean SplitMuxSink::onBusCb(GstBus *bus, GstMessage *msg, gpointer data)
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

void SplitMuxSink::onPadAdded(GstElement *element, GstPad *pad, gpointer data)
{
    GstPad *sinkpad;
    GstElement *decoder = (GstElement*)nvh265enc_;
    g_print("Dynamic pad created, linking demuxer/decoder\n");
    sinkpad = gst_element_get_static_pad(decoder, "sink");
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
}

SplitMuxSink::~SplitMuxSink()
{

}

void SplitMuxSink::init(int argc, char** argv)
{
    if(argc < 3)
    {
        g_print("Input HostIP and Port To UDPSink\n");
        exit(1);
    }
    filePath_ = "video";
    appSrc_ = gst_element_factory_make ("appsrc", "source");
    videoConvert1_ = gst_element_factory_make("videoconvert", "video-convert1");
    nvh265enc_ = gst_element_factory_make("nvh265enc", NULL);
//    nvh265enc_ = gst_element_factory_make("omxh265enc", NULL);
//    g_object_set(nvh265enc_, "control-rate", 2, "target-bitrate", 4000, NULL);
    h265parse_ = gst_element_factory_make("h265parse", "h265parse");
    tee_ = gst_element_factory_make("tee", "tee");
    videoQueue_ = gst_element_factory_make("queue", nullptr);
    fileQueue_ = gst_element_factory_make("queue", nullptr);
    videoConvert2_ = gst_element_factory_make("videoconvert", nullptr);
    filterCaps1_ = gst_element_factory_make("capsfilter", nullptr);
    rtph265pay_ = gst_element_factory_make("rtph265pay", "rtph265pay");
    udpSink_ = gst_element_factory_make("udpsink", "udpsink");
    splitMuxSink_ = gst_element_factory_make("splitmuxsink", "splitmuxsink");
    mpegtsmux_ = gst_element_factory_make("mp4mux", "mux");


    // setup elements
    GstCaps *caps;
    caps = gst_caps_new_simple ("video/x-h265",
                                "stream-format", G_TYPE_STRING, "hvc1",
                                NULL);
    g_object_set (G_OBJECT (filterCaps1_), "caps", caps, nullptr);

    g_object_set(h265parse_, "config-interval", 1, nullptr);
    g_object_set(rtph265pay_, "mtu", 4096,
                 "config-interval", 1, nullptr);
    g_object_set(udpSink_, "auto-multicast", TRUE,
                 "host", argv[1],
                 "port", atoi(argv[2]),
                 "async", TRUE, "sync", TRUE, nullptr);
    gst_caps_unref (caps);
    g_object_set (G_OBJECT (appSrc_), "caps",
                  gst_caps_new_simple (
                      "video/x-raw",
                      "format", G_TYPE_STRING, "NV12",
                      "width", G_TYPE_INT, 640,
                      "height", G_TYPE_INT, 480,
                      "framerate", GST_TYPE_FRACTION, 50, 1,
                      nullptr), nullptr);
    g_object_set (G_OBJECT (appSrc_),
                  "stream-type", 0,
                  "format", GST_FORMAT_TIME, nullptr);

    saveFolder_ = getFileNameByTime();
    DirTool::makeDir(saveFolder_);
    std::string filePattern = saveFolder_ + "/%04d-video.mp4";
    g_object_set(G_OBJECT(splitMuxSink_),
                 "muxer", mpegtsmux_,
                 "location", filePattern.c_str(),
                 "max-size-bytes", kbSize_*1024, NULL);

    // create pipeline
    pipeline_ = gst_pipeline_new("pipe");
    gst_bin_add_many(GST_BIN(pipeline_), appSrc_, videoConvert1_, nvh265enc_, h265parse_,
                     tee_, videoQueue_, rtph265pay_, udpSink_, fileQueue_, splitMuxSink_,
                     nullptr);

    if(gst_element_link_many(appSrc_, videoConvert1_, nvh265enc_, h265parse_, tee_, nullptr) != TRUE)
    {
        g_printerr("Elements could not be linked1\n");
        return;
    }

    if(gst_element_link_many(videoQueue_, rtph265pay_, udpSink_,/*avdecH265_, videoConvert2_, videoSink_,*/ nullptr) != TRUE)
    {
        g_printerr("Elements could not be linked2\n");
        return;
    }

    if(gst_element_link_many(fileQueue_, splitMuxSink_, nullptr) != TRUE)
    {
        g_printerr("Elements could not be linked3\n");
        return;
    }

    // Manually link the tee
    teeVideoPad_ = gst_element_get_request_pad(tee_, "src_%u");
    GstPad *queuePad = gst_element_get_static_pad(videoQueue_, "sink");
    gst_pad_link(teeVideoPad_, queuePad);
    teeFilePad_ = gst_element_get_request_pad(tee_, "src_%u");
    queuePad = gst_element_get_static_pad(fileQueue_, "sink");
    gst_pad_link(teeFilePad_, queuePad);
    gst_object_unref(queuePad);
    g_signal_connect (appSrc_, "need-data", G_CALLBACK (wrapNeedDataCb), this);
}

void SplitMuxSink::start()
{
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    loop_ = g_main_loop_new(nullptr, FALSE);
    gst_bus_add_watch(GST_ELEMENT_BUS(pipeline_), wrapBusCb, this);
    g_main_loop_run(loop_);
    std::string listFile = DirTool::listFileInFolder("SaveVideo");
    auto vecFile = DirTool::splitString(listFile, ",");
    std::cout << "number file = " << vecFile.size() << "\n";
    std::cout << "Size Of SaveVideo = " << DirTool::sizeOfFolder("SaveVideo") << "\n";
    std::map<int, std::string> mapFile;

    for(auto it = vecFile.begin(); it != vecFile.end(); it++)
    {
        mapFile[std::stoi(DirTool::splitString(*it, "-")[0])] = *it;
    }
    int count = 0;
    for(auto it = mapFile.begin(); it != mapFile.end(); it++)
    {
        std::string fileName = "SaveVideo/" + it->second;
        DirTool::removeFile(fileName.c_str());
//        remove(fileName.c_str());
        count++;
        if(count == 10)
            break;
    }
    listFile = DirTool::listFileInFolder("SaveVideo");
    auto vecFile2 = DirTool::splitString(listFile, ",");
    std::cout << "number file = " << vecFile2.size() << "\n";
//    std::cout << "ListFile In SaveVideo = " << DirTool::listFileInFolder("SaveVideo") << "\n";
    std::cout << "Size Of SaveVideo = " << DirTool::sizeOfFolder("SaveVideo") << "\n";

}

GstPadProbeReturn SplitMuxSink::wrapDataPadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    SplitMuxSink *splitMuxSink = static_cast<SplitMuxSink*>(user_data);
    return splitMuxSink->onDataPadProbeCb(pad, info, user_data);
}

void SplitMuxSink::wrapNeedDataCb(GstElement *appsrc, guint unused_size, gpointer user_data)
{
    SplitMuxSink *splitMuxSink = static_cast<SplitMuxSink*>(user_data);
    return splitMuxSink->onNeedDataCb(appsrc, unused_size, user_data);
}

gboolean SplitMuxSink::wrapBusCb(GstBus *bus, GstMessage *msg, gpointer data)
{
    SplitMuxSink *splitMuxSink = static_cast<SplitMuxSink*>(data);
    return splitMuxSink->onBusCb(bus, msg, data);
}

void SplitMuxSink::wrapPadAdded(GstElement *element, GstPad *pad, gpointer data)
{
    SplitMuxSink *splitMuxSink = static_cast<SplitMuxSink*>(data);
    return splitMuxSink->onPadAdded(element, pad, data);
}

std::string SplitMuxSink::saveFolder() const
{
    return saveFolder_;
}
