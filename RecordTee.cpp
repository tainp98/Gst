#include "RecordTee.h"
static gboolean print_field (GQuark field, const GValue * value, gpointer pfx) {
  gchar *str = gst_value_serialize (value);

  g_print ("%s  %15s: %s\n", (gchar *) pfx, g_quark_to_string (field), str);
  g_free (str);
  return TRUE;
}
static void print_caps (const GstCaps * caps, const gchar * pfx) {
  guint i;

  g_return_if_fail (caps != NULL);

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
    gst_structure_foreach (structure, print_field, (gpointer) pfx);
  }
}
static GstPadProbeReturn data_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
//    g_print("Receive Data\n");
    GstCaps *caps = gst_pad_get_current_caps (pad);
    print_caps (caps, "      ");
    gst_caps_unref (caps);
    return GST_PAD_PROBE_PASS;
}

static GstPadProbeReturn event_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    RecordTee* recordTee = (RecordTee*)user_data;
    GMainLoop *loop = (GMainLoop*)recordTee->loop_;
    GstElement *next;
    if(GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS)
    {
        g_print("Not Receive EOS\n");
        return GST_PAD_PROBE_PASS;
    }
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

    gst_element_set_state(recordTee->nvh265enc_, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->nvh265enc_);


    // remove unlinks automatically
//    GstPad *sinkpad = gst_element_get_static_pad(recordTee->videoQueue_, "sink");
//    gst_pad_unlink(recordTee->teeFilePad_, sinkpad);
//    gst_element_release_request_pad(recordTee->tee_, recordTee->teeVideoPad_);
//    gst_object_unref(recordTee->teeVideoPad_);
//    gst_object_unref(sinkpad);
////    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoQueue_);
//    gst_element_set_state(recordTee->videoQueue_, GST_STATE_NULL);
//    gst_element_set_state(recordTee->avdecH265_, GST_STATE_NULL);
//    gst_element_set_state(recordTee->videoConvert2_, GST_STATE_NULL);
//    gst_element_set_state(recordTee->videoSink_, GST_STATE_NULL);
//    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoQueue_);
//    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->avdecH265_);
//    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoConvert2_);
//    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoSink_);
    recordTee->nvh265enc_ = gst_element_factory_make("nvh265enc", NULL);
    recordTee->videoQueue_ = gst_element_factory_make("queue", NULL);
    recordTee->avdecH265_ = gst_element_factory_make("avdec_h265", NULL);
    recordTee->videoConvert2_ = gst_element_factory_make("videoconvert", NULL);
    recordTee->videoSink_ = gst_element_factory_make("xvimagesink", NULL);

    gst_bin_add_many(GST_BIN(recordTee->pipeline_), recordTee->nvh265enc_, recordTee->videoQueue_,
                     recordTee->avdecH265_, recordTee->videoConvert2_, recordTee->videoSink_, NULL);
    gst_element_link_many(recordTee->videoConvert1_, recordTee->nvh265enc_, recordTee->h265parse_, NULL);
    gst_element_link_many(recordTee->videoQueue_,
                          recordTee->avdecH265_, recordTee->videoConvert2_, recordTee->videoSink_, NULL);
    recordTee->teeVideoPad_ = gst_element_get_request_pad(recordTee->tee_, "src_%u");
    GstPad *queue_video_pad = gst_element_get_static_pad(recordTee->videoQueue_, "sink");

    gst_pad_link(recordTee->teeVideoPad_, queue_video_pad);
    queue_video_pad = gst_element_get_static_pad(recordTee->videoConvert1_, "sink");
    gst_pad_add_probe(queue_video_pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)data_probe_cb, NULL, NULL);
    gst_object_unref(queue_video_pad);


    gst_element_set_state(recordTee->nvh265enc_, GST_STATE_PLAYING);
    gst_element_set_state(recordTee->videoQueue_, GST_STATE_PLAYING);
    gst_element_set_state(recordTee->avdecH265_, GST_STATE_PLAYING);
    gst_element_set_state(recordTee->videoConvert2_, GST_STATE_PLAYING);
    gst_element_set_state(recordTee->videoSink_, GST_STATE_PLAYING);
    return GST_PAD_PROBE_DROP;
}
static GstPadProbeReturn
pad_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    RecordTee *recordTee = (RecordTee*)user_data;
    GstPad *srcpad, *sinkpad;
    g_print("blockpad is block now\n");
    // remove the probe first
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    // install new probe for EOS
//    srcpad = gst_element_get_static_pad(recordTee->avdecH265_, "src");
//    gst_pad_add_probe(recordTee->teeVideoPad_, GstPadProbeType(GST_PAD_PROBE_TYPE_BLOCK
//                                                               | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), event_probe_cb,
//                      user_data, NULL);
//    gst_object_unref(srcpad);
//    sinkpad = gst_element_get_static_pad(recordTee->avdecH265_, "sink");
//    gst_pad_send_event(sinkpad, gst_event_new_eos());
////    gst_element_send_event(recordTee->pipeline_, gst_event_new_eos());
//    gst_object_unref(sinkpad);
//    return GST_PAD_PROBE_REMOVE;

    // push EOS into the element, the probe will be fired when the
    // EOS leaves the effect and it has thus drained all of its data
    sinkpad = gst_element_get_static_pad(recordTee->fileQueue_, "sink");
    gst_pad_unlink(recordTee->teeFilePad_, sinkpad);
    gst_element_release_request_pad(recordTee->tee_, recordTee->teeFilePad_);
    gst_object_unref(recordTee->teeFilePad_);
    gst_element_send_event(recordTee->mpegtsmux_, gst_event_new_eos());
    gst_object_unref(sinkpad);
//    usleep(1000);
    sleep(1);
    gst_element_set_state(recordTee->fileQueue_, GST_STATE_NULL);
//    gst_element_set_state(recordTee->avdecH265_, GST_STATE_NULL);
    gst_element_set_state(recordTee->mpegtsmux_, GST_STATE_NULL);
    gst_element_set_state(recordTee->fileSink_, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->fileQueue_);
//    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->avdecH265_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->mpegtsmux_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->fileSink_);

//    g_object_unref(video_queue);
//    g_object_unref(video_visual);
//    g_object_unref(video_convert);
//    g_object_unref(video_sink);
//    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    return GST_PAD_PROBE_REMOVE;
}

static GstPadProbeReturn
pad_probe_cb2(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    RecordTee *recordTee = (RecordTee*)user_data;
    GstPad *srcpad, *sinkpad;
    g_print("blockpad is block now\n");
    // remove the probe first
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    // install new probe for EOS
//    srcpad = gst_element_get_static_pad(recordTee->nvh265enc_, "src");
//    gst_pad_add_probe(srcpad, GstPadProbeType(GST_PAD_PROBE_TYPE_BLOCK
//                                                               | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), event_probe_cb,
//                      user_data, NULL);
//    gst_object_unref(srcpad);
//    sinkpad = gst_element_get_static_pad(recordTee->nvh265enc_, "sink");
//    gst_pad_send_event(sinkpad, gst_event_new_eos());
    gst_element_send_event(recordTee->pipeline_, gst_event_new_eos());
//    gst_object_unref(sinkpad);
    return GST_PAD_PROBE_REMOVE;

    // push EOS into the element, the probe will be fired when the
    // EOS leaves the effect and it has thus drained all of its data
    sinkpad = gst_element_get_static_pad(recordTee->videoQueue_, "sink");
    gst_pad_unlink(recordTee->teeVideoPad_, sinkpad);
    gst_element_release_request_pad(recordTee->tee_, recordTee->teeVideoPad_);
    gst_object_unref(recordTee->teeVideoPad_);
    gst_element_send_event(recordTee->videoQueue_, gst_event_new_eos());
    gst_object_unref(sinkpad);
    usleep(1000);
    gst_element_set_state(recordTee->videoQueue_, GST_STATE_NULL);
    gst_element_set_state(recordTee->avdecH265_, GST_STATE_NULL);
    gst_element_set_state(recordTee->videoConvert2_, GST_STATE_NULL);
    gst_element_set_state(recordTee->videoSink_, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoQueue_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->avdecH265_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoConvert2_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoSink_);

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
    if(recordTee->recording_)
    {
        recordTee->pad_probe_id_ = gst_pad_add_probe(recordTee->teeVideoPad_, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                          pad_probe_cb, user_data, NULL);
        std::cout << "TimeoutCallBack - Turn off record\n";
    }
    else
    {
        std::cout << "=============================== TimeoutCallBack - Turn on record\n";
//        gst_element_send_event(recordTee->pipeline_, gst_event_new_eos());
//        GstPad *srcpad;
//        srcpad = gst_element_get_static_pad(recordTee->videoConvert1_, "src");
//        recordTee->pad_probe_id_ = gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
//                          pad_probe_cb2, user_data, NULL);
//        gst_object_unref(srcpad);

        recordTee->fileQueue_ = gst_element_factory_make("queue", NULL);
        recordTee->mpegtsmux_ = gst_element_factory_make("mpegtsmux", NULL);
//        recordTee->videoConvert2_ = gst_element_factory_make("videoconvert", NULL);
        recordTee->fileSink_ = gst_element_factory_make("filesink", NULL);
        std::string fileName = recordTee->filePath_;
        fileName  = fileName + "_" + std::to_string(++recordTee->count_) + ".mp4";
        g_object_set(recordTee->fileSink_, "location", fileName.c_str(), NULL);
        gst_bin_add_many(GST_BIN(recordTee->pipeline_), recordTee->fileQueue_,
                         recordTee->mpegtsmux_, recordTee->fileSink_, NULL);
        gst_element_link_many(recordTee->mpegtsmux_,
                              recordTee->fileSink_, NULL);
        recordTee->teeFilePad_ = gst_element_get_request_pad(recordTee->tee_, "src_%u");
        GstPad *queue_video_pad = gst_element_get_static_pad(recordTee->fileQueue_, "sink");

        gst_pad_link(recordTee->teeFilePad_, queue_video_pad);
        GstPad *muxsinkpad = gst_element_get_request_pad(recordTee->mpegtsmux_, "sink_%d");
        queue_video_pad = gst_element_get_static_pad(recordTee->fileQueue_, "src");
        gst_pad_link(queue_video_pad, muxsinkpad);
        gst_pad_add_probe(muxsinkpad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)data_probe_cb, NULL, NULL);
        gst_object_unref(queue_video_pad);
        gst_object_unref(muxsinkpad);


//        gst_element_sync_state_with_parent(recordTee->videoQueue_);
//        gst_element_sync_state_with_parent(recordTee->avdecH265_);
//        gst_element_sync_state_with_parent(recordTee->videoConvert2_);
//        gst_element_sync_state_with_parent(recordTee->videoSink_);

        gst_element_set_state(recordTee->fileQueue_, GST_STATE_PLAYING);
//        gst_element_set_state(recordTee->avdecH265_, GST_STATE_PLAYING);
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
//        GstPad *sinkpad = gst_element_get_static_pad(recordTee->videoQueue_, "sink");
//        gst_pad_unlink(recordTee->teeFilePad_, sinkpad);
//        gst_element_release_request_pad(recordTee->tee_, recordTee->teeVideoPad_);
//        gst_object_unref(recordTee->teeVideoPad_);
//        gst_object_unref(sinkpad);
//    //    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoQueue_);
//        gst_element_set_state(recordTee->videoQueue_, GST_STATE_NULL);
//        gst_element_set_state(recordTee->avdecH265_, GST_STATE_NULL);
//        gst_element_set_state(recordTee->videoConvert2_, GST_STATE_NULL);
//        gst_element_set_state(recordTee->videoSink_, GST_STATE_NULL);
//        gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoQueue_);
//        gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->avdecH265_);
//        gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoConvert2_);
//        gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoSink_);
//        GstPad* srcpad, *sinkpad;
//        srcpad = gst_element_get_static_pad(recordTee->videoQueue_, "src");
//        gst_pad_add_probe(srcpad, GstPadProbeType(GST_PAD_PROBE_TYPE_BLOCK), event_probe_cb,
//                          data, NULL);
//        gst_object_unref(srcpad);
//        sinkpad = gst_element_get_static_pad(recordTee->videoQueue_, "sink");
//        gst_pad_send_event(sinkpad, gst_event_new_eos());
//        gst_object_unref(sinkpad);

//        g_main_loop_quit(loop);
        gst_element_set_state(recordTee->nvh265enc_, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->nvh265enc_);


        // remove unlinks automatically
    //    GstPad *sinkpad = gst_element_get_static_pad(recordTee->videoQueue_, "sink");
    //    gst_pad_unlink(recordTee->teeFilePad_, sinkpad);
    //    gst_element_release_request_pad(recordTee->tee_, recordTee->teeVideoPad_);
    //    gst_object_unref(recordTee->teeVideoPad_);
    //    gst_object_unref(sinkpad);
    ////    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoQueue_);
    //    gst_element_set_state(recordTee->videoQueue_, GST_STATE_NULL);
    //    gst_element_set_state(recordTee->avdecH265_, GST_STATE_NULL);
    //    gst_element_set_state(recordTee->videoConvert2_, GST_STATE_NULL);
    //    gst_element_set_state(recordTee->videoSink_, GST_STATE_NULL);
    //    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoQueue_);
    //    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->avdecH265_);
    //    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoConvert2_);
    //    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->videoSink_);
        recordTee->nvh265enc_ = gst_element_factory_make("nvh265enc", NULL);
        recordTee->videoQueue_ = gst_element_factory_make("queue", NULL);
        recordTee->avdecH265_ = gst_element_factory_make("avdec_h265", NULL);
        recordTee->videoConvert2_ = gst_element_factory_make("videoconvert", NULL);
        recordTee->videoSink_ = gst_element_factory_make("xvimagesink", NULL);

        gst_bin_add_many(GST_BIN(recordTee->pipeline_), recordTee->nvh265enc_, recordTee->videoQueue_,
                         recordTee->avdecH265_, recordTee->videoConvert2_, recordTee->videoSink_, NULL);
        gst_element_link_many(recordTee->videoConvert1_, recordTee->nvh265enc_, recordTee->h265parse_, NULL);
        gst_element_link_many(recordTee->videoQueue_,
                              recordTee->avdecH265_, recordTee->videoConvert2_, recordTee->videoSink_, NULL);
        recordTee->teeVideoPad_ = gst_element_get_request_pad(recordTee->tee_, "src_%u");
        GstPad *queue_video_pad = gst_element_get_static_pad(recordTee->videoQueue_, "sink");

        gst_pad_link(recordTee->teeVideoPad_, queue_video_pad);
        queue_video_pad = gst_element_get_static_pad(recordTee->videoConvert1_, "sink");
        gst_pad_add_probe(queue_video_pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)data_probe_cb, NULL, NULL);
        gst_object_unref(queue_video_pad);


        gst_element_set_state(recordTee->nvh265enc_, GST_STATE_PLAYING);
        gst_element_set_state(recordTee->videoQueue_, GST_STATE_PLAYING);
        gst_element_set_state(recordTee->avdecH265_, GST_STATE_PLAYING);
        gst_element_set_state(recordTee->videoConvert2_, GST_STATE_PLAYING);
        gst_element_set_state(recordTee->videoSink_, GST_STATE_PLAYING);
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

static void on_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
    RecordTee* recordTee = (RecordTee*)data;
    GstPad *sinkpad;
    GstElement *decoder = (GstElement*)recordTee->videoConvert1_;
    g_print("Dynamic pad created, linking demuxer/decoder\n");
    sinkpad = gst_element_get_static_pad(decoder, "sink");
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
}

RecordTee::RecordTee()
{

}

RecordTee::~RecordTee()
{

}

void RecordTee::init()
{

    filePath_ = "file";
    src_ = gst_element_factory_make("filesrc", "src");
    g_object_set(src_, "location", "/home/vietph/Videos/video/ir.m4e", NULL);
    decodebin_ = gst_element_factory_make("decodebin", "decode");
    videoConvert1_ = gst_element_factory_make("videoconvert", "video-convert1");
    nvh265enc_ = gst_element_factory_make("nvh265enc", NULL);
    h265parse_ = gst_element_factory_make("h265parse", "h265parse");
//    g_object_set(h265parse_, "config-interval", -1, NULL);
    tee_ = gst_element_factory_make("tee", "tee");
    videoQueue_ = gst_element_factory_make("queue", NULL);
    fileQueue_ = gst_element_factory_make("queue", NULL);
    avdecH265_ = gst_element_factory_make("avdec_h265", NULL);
    videoConvert2_ = gst_element_factory_make("videoconvert", NULL);
    mpegtsmux_ = gst_element_factory_make("mpegtsmux", NULL);
    videoSink_ = gst_element_factory_make("xvimagesink", NULL);
    fileSink_ = gst_element_factory_make("filesink", NULL);
    std::string fileName = filePath_;
    fileName = fileName + "_" + std::to_string(++count_) + ".mp4";
    g_object_set(fileSink_, "location", fileName.c_str(), NULL);

    // create pipeline
    pipeline_ = gst_pipeline_new("pipe");
    gst_bin_add_many(GST_BIN(pipeline_), src_, decodebin_, videoConvert1_, nvh265enc_, h265parse_,
                     tee_, videoQueue_, fileQueue_, avdecH265_,
                     videoConvert2_, mpegtsmux_, videoSink_, fileSink_, NULL);
//    gst_bin_add_many(GST_BIN(pipeline_), src_, decodebin_, videoSink_, NULL);
    if(gst_element_link_many(src_, decodebin_, NULL) != TRUE
            )
    {
        g_printerr("Elements could not be linked1\n");
        return;
    }

    if(gst_element_link_many(videoConvert1_, nvh265enc_, h265parse_, tee_, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked2\n");
        return;
    }
    g_signal_connect (decodebin_, "pad-added", G_CALLBACK (on_pad_added), this);

    if(gst_element_link_many(videoQueue_, avdecH265_, videoConvert2_, videoSink_, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked3\n");
        return;
    }
    if(gst_element_link_many(mpegtsmux_, fileSink_, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked4\n");
        return;
    }
    // Manually link the tee
    teeVideoPad_ = gst_element_get_request_pad(tee_, "src_%u");
    GstPad *queuePad = gst_element_get_static_pad(videoQueue_, "sink");
    gst_pad_link(teeVideoPad_, queuePad);
    teeFilePad_ = gst_element_get_request_pad(tee_, "src_%u");
    queuePad = gst_element_get_static_pad(fileQueue_, "sink");
    gst_pad_link(teeFilePad_, queuePad);
    queuePad = gst_element_get_static_pad(fileQueue_, "src");
    GstPad *muxsinkpad = gst_element_get_request_pad(mpegtsmux_, "sink_%d");
    gst_pad_link(queuePad, muxsinkpad);
    gst_pad_add_probe(muxsinkpad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)data_probe_cb, NULL, NULL);
    gst_object_unref(queuePad);
    gst_object_unref(muxsinkpad);
}

void RecordTee::start()
{
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    loop_ = g_main_loop_new(NULL, FALSE);
    gst_bus_add_watch(GST_ELEMENT_BUS(pipeline_), bus_cb, this);
    g_timeout_add_seconds(5, timeout_cb, this);
    g_main_loop_run(loop_);
}
