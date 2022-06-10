#include "RecordTee.h"

static GstPadProbeReturn
pad_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    RecordTee *recordTee = (RecordTee*)user_data;
    GstPad *srcpad, *sinkpad;
    g_print("blockpad is block now\n");
    // remove the probe first
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    // install new probe for EOS
//    srcpad = gst_element_get_static_pad(video_queue, "src");
//    gst_pad_add_probe(srcpad, GstPadProbeType(GST_PAD_PROBE_TYPE_BLOCK |
//                              GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), event_probe_cb,
//                      user_data, NULL);
//    gst_object_unref(srcpad);

    // push EOS into the element, the probe will be fired when the
    // EOS leaves the effect and it has thus drained all of its data
    sinkpad = gst_element_get_static_pad(recordTee->fileQueue_, "sink");
    gst_pad_unlink(recordTee->teeFilePad_, sinkpad);
    gst_element_release_request_pad(recordTee->tee_, recordTee->teeFilePad_);
    gst_object_unref(recordTee->teeFilePad_);
    gst_element_send_event(recordTee->mpegtsmux_, gst_event_new_eos());
    gst_object_unref(sinkpad);
    usleep(1000);
    gst_element_set_state(recordTee->fileQueue_, GST_STATE_NULL);
    gst_element_set_state(recordTee->mpegtsmux_, GST_STATE_NULL);
    gst_element_set_state(recordTee->fileSink_, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->fileQueue_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->mpegtsmux_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->fileSink_);
//    g_object_unref(video_queue);
//    g_object_unref(video_visual);
//    g_object_unref(video_convert);
//    g_object_unref(video_sink);
//    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    return GST_PAD_PROBE_REMOVE;
}

static gboolean timeout_cb(gpointer user_data)
{
    RecordTee *recordTee = (RecordTee*)user_data;
    if(!recordTee->recording_)
    {
        gst_pad_add_probe(recordTee->teeFilePad_, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                          pad_probe_cb, user_data, NULL);
        std::cout << "TimeoutCallBack - Turn on record\n";
    }
    else
    {
        std::cout << "TimeoutCallBack - Turn off record\n";

        recordTee->fileQueue_ = gst_element_factory_make("queue", NULL);
        recordTee->mpegtsmux_ = gst_element_factory_make("mpegtsmux", NULL);
        recordTee->fileSink_ = gst_element_factory_make("filesink", NULL);
        std::string fileName = recordTee->filePath_;
        fileName  = fileName + "_" + std::to_string(++recordTee->count_) + ".mp4";
        g_object_set(recordTee->fileSink_, "location", fileName.c_str(), NULL);
        gst_bin_add_many(GST_BIN(recordTee->pipeline_), recordTee->fileQueue_,
                         recordTee->mpegtsmux_, recordTee->fileSink_, NULL);
        gst_element_link_many(recordTee->fileQueue_,
                              recordTee->mpegtsmux_, recordTee->fileSink_, NULL);
        recordTee->teeFilePad_ = gst_element_get_request_pad(recordTee->tee_, "src_%u");
        GstPad *queue_video_pad = gst_element_get_static_pad(recordTee->fileQueue_, "sink");

        gst_pad_link(recordTee->teeFilePad_, queue_video_pad);
        gst_object_unref(queue_video_pad);

        gst_element_set_state(recordTee->fileQueue_, GST_STATE_PLAYING);
        gst_element_set_state(recordTee->mpegtsmux_, GST_STATE_PLAYING);
        gst_element_set_state(recordTee->fileSink_, GST_STATE_PLAYING);
    }
    recordTee->recording_ = !recordTee->recording_;
    return TRUE;
}

static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
    RecordTee *recordTee = (RecordTee*)data;
    GMainLoop *loop = recordTee->loop_;
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
    {
        g_print("End of stream\n");

        g_main_loop_quit(loop);
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
        g_main_loop_quit(loop);
        break;
    }
    default:
    {
        break;
    }
    }
    return TRUE;
}

RecordTee::RecordTee()
{

}

RecordTee::~RecordTee()
{

}

void RecordTee::init()
{
    gst_init(NULL, NULL);
    filePath_ = "file";
    src_ = gst_element_factory_make("filesrc", "src");
    g_object_set(src_, "location", "/home/vietph/Videos/video/ir.m4e", NULL);
    decodebin_ = gst_element_factory_make("decodebin", "decode");
    videoConvert1_ = gst_element_factory_make("videoconvert", "video-convert1");
    nvh265enc_ = gst_element_factory_make("nvh265enc", "nvh265enc");
    h265parse_ = gst_element_factory_make("h265parse", "h265parse");
    tee_ = gst_element_factory_make("tee", "tee");
    videoQueue_ = gst_element_factory_make("queue", NULL);
    fileQueue_ = gst_element_factory_make("queue", NULL);
    avdecH265_ = gst_element_factory_make("avdec_h265", "avdech265");
    videoConvert2_ = gst_element_factory_make("videoconvert", "video-convert2");
    mpegtsmux_ = gst_element_factory_make("mpegtsmux", NULL);
    videoSink_ = gst_element_factory_make("xvimagesink", "video-sink");
    fileSink_ = gst_element_factory_make("filesink", NULL);
    std::string fileName = filePath_;
    fileName = fileName + "_" + std::to_string(++count_) + ".mp4";
    g_object_set(fileSink_, "location", fileName.c_str(), NULL);

    // create pipeline
    pipeline_ = gst_pipeline_new("pipe");
    gst_bin_add_many(GST_BIN(pipeline_), src_, decodebin_, videoConvert1_, nvh265enc_,
                     h265parse_, tee_, videoQueue_, fileQueue_, avdecH265_,
                     videoConvert2_, mpegtsmux_, videoSink_, fileSink_, NULL);
    if(gst_element_link_many(src_, tee_, NULL) != TRUE
            )
    {
        g_printerr("Elements could not be linked1\n");
        return;
    }
    if(gst_element_link_many(videoQueue_, avdecH265_, videoConvert2_, videoSink_, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked2\n");
        return;
    }
    if(gst_element_link_many(fileQueue_, mpegtsmux_, fileSink_, NULL) != TRUE)
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
}

void RecordTee::start()
{
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    loop_ = g_main_loop_new(NULL, FALSE);
    gst_bus_add_watch(GST_ELEMENT_BUS(pipeline_), bus_cb, this);
    g_timeout_add_seconds(10, timeout_cb, this);
    g_main_loop_run(loop_);
}
