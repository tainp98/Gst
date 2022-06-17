#include "RecordTee.h"
#include <gst/gstbuffer.h>
#include <gst/allocators/gstdmabuf.h>
bool recording = false;
void sigintHandler(int unused) {
        g_print("You ctrl-z!\n");
        if (recording)
                RecordTee::recordTee()->stopRecording();
        else
                RecordTee::recordTee()->startRecording();
}

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
    g_print("Receive Data\n");
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    GstCaps *caps = gst_pad_get_current_caps (pad);
    print_caps (caps, "      ");
    gst_caps_unref (caps);
    return GST_PAD_PROBE_PASS;
}

void RecordTee::cb_need_data (GstElement *appsrc, guint unused_size, gpointer user_data)
{
    RecordTee *recordTee = (RecordTee*)user_data;
  static gboolean white = FALSE;
  static GstClockTime timestamp = 0;
  GstBuffer *buffer;
  guint size;
  GstFlowReturn ret;

  size = 640 * 480*1.5;

  buffer = gst_buffer_new_allocate (NULL, size, NULL);
  /* this makes the image black/white */
  gst_buffer_memset (buffer, 0, white ? 0xff : 0x0, size);

  white = !white;

  GST_BUFFER_PTS (buffer) = timestamp;
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 2);

  timestamp += GST_BUFFER_DURATION (buffer);

//  while(true){

//  }
  g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
  gst_buffer_unref (buffer);

  if (ret != GST_FLOW_OK) {
      std::cout << "ret = " << ret << "\n";
    /* something wrong, stop pushing */
    g_main_loop_quit (recordTee->loop_);
  }
}

GstPadProbeReturn RecordTee::event_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    RecordTee *recordTee = (RecordTee*)user_data;
    GMainLoop *loop = (GMainLoop*)user_data;
    GstElement *next;
    if(GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS)
    {
        g_print("Not Receive EOS\n");
        return GST_PAD_PROBE_PASS;
    }
    g_print("blockmpegtsmuxsrcpad is block now\n");
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    // push current effect back into the queue
    GstPad *sinkpad;
    sinkpad = gst_element_get_static_pad(recordTee->fileQueue_, "sink");
    if(sinkpad == nullptr)
    {
        printf("sinkpad is nullptr\n");
    }
    gst_pad_unlink(recordTee->teeFilePad_, sinkpad);
    gst_element_release_request_pad(recordTee->tee_, recordTee->teeFilePad_);
    gst_object_unref(recordTee->teeFilePad_);
    gst_object_unref(sinkpad);
    usleep(1000);

    gst_element_set_state(recordTee->fileQueue_, GST_STATE_NULL);
    gst_element_set_state(recordTee->mpegtsmux_, GST_STATE_NULL);
    gst_element_set_state(recordTee->fileSink_, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->fileQueue_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->mpegtsmux_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->fileSink_);
    return GST_PAD_PROBE_REMOVE;
}

GstPadProbeReturn
RecordTee::pad_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    g_print("blockteesrcpad is block now\n");
    RecordTee *recordTee = (RecordTee*)user_data;
    GstPad *srcpad, *sinkpad;
    // remove the probe first
//    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
////    // install new probe for EOS
//    srcpad = gst_element_get_static_pad(recordTee->mpegtsmux_, "src");
//    gst_pad_add_probe(srcpad, GstPadProbeType(GST_PAD_PROBE_TYPE_BLOCK |
//                      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), event_probe_cb,
//                      user_data, NULL);
//    gst_object_unref(srcpad);

//    // push EOS into the element, the probe will be fired when the
//    // EOS leaves the effect and it has thus drained all of its data
////    sinkpad = gst_element_get_static_pad(recordTee->mpegtsmux_, "sink");
//    gst_pad_send_event(RecordTee::recordTee()->mpegSinkPad_, gst_event_new_eos());
////    gst_object_unref(sinkpad);
//    return GST_PAD_PROBE_OK;

    sinkpad = gst_element_get_static_pad(recordTee->fileQueue_, "sink");
    gst_pad_unlink(recordTee->teeFilePad_, sinkpad);
    gst_element_release_request_pad(recordTee->tee_, recordTee->teeFilePad_);
    gst_object_unref(recordTee->teeFilePad_);

//    usleep(10000);
    sleep(1);
    gst_element_send_event(recordTee->mpegtsmux_, gst_event_new_eos());
//    gst_pad_send_event(RecordTee::recordTee()->mpegSinkPad_, gst_event_new_eos());
    gst_object_unref(sinkpad);
    usleep(1000);

    gst_element_set_state(recordTee->fileQueue_, GST_STATE_NULL);
    gst_element_set_state(recordTee->mpegtsmux_, GST_STATE_NULL);
    gst_element_set_state(recordTee->fileSink_, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->fileQueue_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->mpegtsmux_);
    gst_bin_remove(GST_BIN(recordTee->pipeline_), recordTee->fileSink_);
    return GST_PAD_PROBE_REMOVE;
}

void RecordTee::stopRecording()
{
    RecordTee::recordTee()->pad_probe_id_ = gst_pad_add_probe(RecordTee::recordTee()->teeFilePad_,
                                                              GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                      pad_probe_cb, RecordTee::recordTee(), NULL);
    std::cout << "TimeoutCallBack - Turn off record\n";
    recording = false;
}

void RecordTee::startRecording()
{
    std::cout << "=============================== TimeoutCallBack - Turn on record\n";

    RecordTee::recordTee()->fileQueue_ = gst_element_factory_make("queue", NULL);
    RecordTee::recordTee()->mpegtsmux_ = gst_element_factory_make("mpegtsmux", NULL);
    RecordTee::recordTee()->fileSink_ = gst_element_factory_make("filesink", NULL);
//    RecordTee::recordTee()->filerCaps_ = gst_element_factory_make("capsfilter", NULL);
//    GstCaps *caps;
//    caps = gst_caps_new_simple ("video/x-h265",
//                   "stream-format", G_TYPE_STRING, "hvc1",
//                   NULL);
//    g_object_set (G_OBJECT (RecordTee::recordTee()->filerCaps_), "caps", caps, NULL);
//     gst_caps_unref (caps);
    std::string fileName = RecordTee::recordTee()->filePath_;
    fileName  = fileName + "_" + std::to_string(++RecordTee::recordTee()->count_) + ".mp4";
    g_object_set(RecordTee::recordTee()->fileSink_, "location", fileName.c_str(), NULL);
    gst_bin_add_many(GST_BIN(RecordTee::recordTee()->pipeline_), RecordTee::recordTee()->fileQueue_,
                     RecordTee::recordTee()->mpegtsmux_, RecordTee::recordTee()->fileSink_, NULL);
//    gst_element_link_many(RecordTee::recordTee()->fileQueue_,
//                          RecordTee::recordTee()->filerCaps_, NULL);
    gst_element_link_many(RecordTee::recordTee()->mpegtsmux_,
                          RecordTee::recordTee()->fileSink_, NULL);
    RecordTee::recordTee()->teeFilePad_ = gst_element_get_request_pad(RecordTee::recordTee()->tee_, "src_%u");
    GstPad *queue_video_pad;
    GstPadLinkReturn ret;

    queue_video_pad = gst_element_get_static_pad(RecordTee::recordTee()->fileQueue_, "sink");
    ret = gst_pad_link(RecordTee::recordTee()->teeFilePad_, queue_video_pad);
    std::cout << "GstPadLinkReturn srcTee - sinkQueue = " << ret << "\n";

    RecordTee::recordTee()->mpegSinkPad_ = gst_element_get_request_pad(RecordTee::recordTee()->mpegtsmux_, "sink_%d");
    queue_video_pad = gst_element_get_static_pad(RecordTee::recordTee()->fileQueue_, "src");
    ret = gst_pad_link(queue_video_pad, RecordTee::recordTee()->mpegSinkPad_);
    std::cout << "GstPadLinkReturn srcQueue - sinkMux = " << ret << "\n";

//    std::cout << gst_element_link(RecordTee::recordTee()->fileQueue_, RecordTee::recordTee()->filerCaps_) << "\n";



    GstPad *muxsinkpad = gst_element_get_static_pad(RecordTee::recordTee()->videoQueue_, "src");
    gst_pad_add_probe(muxsinkpad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)data_probe_cb, NULL, NULL);
    gst_object_unref(queue_video_pad);
    gst_object_unref(muxsinkpad);
//    gst_object_unref(RecordTee::recordTee()->mpegSinkPad_);

    gst_element_set_state(RecordTee::recordTee()->fileQueue_, GST_STATE_PLAYING);
    gst_element_set_state(RecordTee::recordTee()->mpegtsmux_, GST_STATE_PLAYING);
    gst_element_set_state(RecordTee::recordTee()->fileSink_, GST_STATE_PLAYING);
    recording = true;
}

//static gboolean timeout_cb(gpointer user_data)
//{
//    RecordTee *recordTee = (RecordTee*)user_data;
//    if(recordTee->recording_)
//    {
//        recordTee->pad_probe_id_ = gst_pad_add_probe(recordTee->teeVideoPad_, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
//                          pad_probe_cb, user_data, NULL);
//    }
//    return TRUE;
//}

static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
    RecordTee *recordTee = (RecordTee*)data;
    GMainLoop *loop = recordTee->loop_;
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
    GstElement *decoder = (GstElement*)recordTee->nvh265enc_;
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
    signal(SIGTSTP, sigintHandler);
    filePath_ = "file";
    src_ = gst_element_factory_make("filesrc", "src");
    g_object_set(src_, "location", "/home/root/tainp/videoplayback.mp4", NULL);
//    g_object_set(src_, "location", "/home/vietph/Videos/videoplayback.mp4", NULL);
    decodebin_ = gst_element_factory_make("decodebin", "decode");
//    qtdemux_ = gst_element_factory_make("qtdemux", NULL);
//    h264parse_ = gst_element_factory_make("h264parse", NULL);
//    omxh264dec_ = gst_element_factory_make("omxh264dec", NULL);
//    g_object_set(h264parse_, "config-interval", 1, NULL);
    videoConvert1_ = gst_element_factory_make("videoconvert", "video-convert1");
//    nvh265enc_ = gst_element_factory_make("nvh265enc", NULL);
    nvh265enc_ = gst_element_factory_make("omxh265enc", NULL);
    g_object_set(nvh265enc_, "control-rate", 2, "target-bitrate", 4000, NULL);
    h265parse_ = gst_element_factory_make("h265parse", "h265parse");
    g_object_set(h265parse_, "config-interval", 1, NULL);
    tee_ = gst_element_factory_make("tee", "tee");
    videoQueue_ = gst_element_factory_make("queue", NULL);
    avdecH265_ = gst_element_factory_make("avdec_h265", NULL);
    videoConvert2_ = gst_element_factory_make("videoconvert", NULL);
    videoSink_ = gst_element_factory_make("autovideosink", NULL);
    appSrc_ = gst_element_factory_make ("appsrc", "source");
    filterCaps1_ = gst_element_factory_make("capsfilter", NULL);
    filterCaps_ = gst_element_factory_make("capsfilter", NULL);
    GstCaps *caps;
    caps = gst_caps_new_simple ("video/x-h265",
                   "stream-format", G_TYPE_STRING, "byte-stream",
                   NULL);
    g_object_set (G_OBJECT (filterCaps1_), "caps", caps, NULL);

     rtph265pay_ = gst_element_factory_make("rtph265pay", "rtph265pay");
     udpSink_ = gst_element_factory_make("udpsink", "udpsink");
     g_object_set(rtph265pay_, "mtu", 4096,
                  "config-interval", 1, NULL);
     g_object_set(udpSink_, "auto-multicast", TRUE,
                  "host", "172.21.100.174",
                  "port", 18888,
                  "async", TRUE, "sync", TRUE, NULL);
//    fileQueue_ = gst_element_factory_make("queue", NULL);
//    mpegtsmux_ = gst_element_factory_make("mp4mux", NULL);
//    fileSink_ = gst_element_factory_make("filesink", NULL);
//    std::string fileName = RecordTee::recordTee()->filePath_;
//    fileName  = fileName + "_" + std::to_string(++RecordTee::recordTee()->count_) + ".mp4";
//    g_object_set(RecordTee::recordTee()->fileSink_, "location", fileName.c_str(), NULL);
     caps = gst_caps_new_simple ("video/x-raw",
                    "format", G_TYPE_STRING, "NV12",
                                 "width", G_TYPE_INT, 640,
                                 "height", G_TYPE_INT, 480,
                                 "framerate", GST_TYPE_FRACTION, 30, 1,
                    NULL);
     g_object_set (G_OBJECT (filterCaps_), "caps", caps, NULL);
     gst_caps_unref (caps);
    /* setup */
     g_object_set (G_OBJECT (appSrc_), "caps",
           gst_caps_new_simple (
                       "video/x-raw",
                        "format", G_TYPE_STRING, "NV12",
                        "width", G_TYPE_INT, 640,
                        "height", G_TYPE_INT, 480,
                        "framerate", GST_TYPE_FRACTION, 30, 1,
//                        "interlace-mode", G_TYPE_STRING, "progressive",
//                        "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
//                        "colormetry", G_TYPE_STRING, "bt601",
                        NULL), NULL);
     g_object_set (G_OBJECT (appSrc_),
           "stream-type", 0,
           "format", GST_FORMAT_TIME, NULL);

    // create pipeline
    pipeline_ = gst_pipeline_new("pipe");
    gst_bin_add_many(GST_BIN(pipeline_), appSrc_, filterCaps_, /*src_, decodebin_,*/ nvh265enc_, h265parse_,
                     tee_, videoQueue_,filterCaps1_, rtph265pay_, udpSink_,/*avdecH265_, videoConvert2_, videoSink_,*/
                     /*fileQueue_, mpegtsmux_, fileSink_,*/ NULL);
//    if(gst_element_link_many(src_, decodebin_, NULL) != TRUE)
//    {
//        g_printerr("Elements could not be linked1\n");
//        return;
//    }

    if(gst_element_link_many(appSrc_, filterCaps_, nvh265enc_, h265parse_, tee_, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked2\n");
        return;
    }
//    g_signal_connect (decodebin_, "pad-added", G_CALLBACK (on_pad_added), this);

    if(gst_element_link_many(videoQueue_, filterCaps1_, rtph265pay_, udpSink_,/*avdecH265_, videoConvert2_, videoSink_,*/ NULL) != TRUE)
    {
        g_printerr("Elements could not be linked3\n");
        return;
    }
//    if(gst_element_link_many(mpegtsmux_, fileSink_, NULL) != TRUE)
//    {
//        g_printerr("Elements could not be linked4\n");
//        return;
//    }
    // Manually link the tee
    teeVideoPad_ = gst_element_get_request_pad(tee_, "src_%u");
    GstPad *queuePad = gst_element_get_static_pad(videoQueue_, "sink");
    gst_pad_link(teeVideoPad_, queuePad);
//    teeFilePad_ = gst_element_get_request_pad(tee_, "src_%u");
//    queuePad = gst_element_get_static_pad(fileQueue_, "sink");
//    gst_pad_link(teeFilePad_, queuePad);
//    queuePad = gst_element_get_static_pad(fileQueue_, "src");
//    GstPad *muxsinkpad = gst_element_get_request_pad(mpegtsmux_, "video_%u");
//   GstPadLinkReturn ret = gst_pad_link(queuePad, muxsinkpad);
//   std::cout << "ret = " << ret << "\n";
//    gst_pad_add_probe(muxsinkpad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)data_probe_cb, NULL, NULL);
    gst_object_unref(queuePad);
    /* setup appsrc */

      g_signal_connect (appSrc_, "need-data", G_CALLBACK (cb_need_data), this);
}

void RecordTee::start()
{
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    loop_ = g_main_loop_new(NULL, FALSE);
    gst_bus_add_watch(GST_ELEMENT_BUS(pipeline_), bus_cb, this);
//    g_timeout_add_seconds(10, timeout_cb, this);
    g_main_loop_run(loop_);
}

RecordTee *RecordTee::recordTee()
{
    std::unique_lock<std::mutex> locker(RecordTee::mux_);
    if(recordTee_ == nullptr)
    {
        recordTee_ = new RecordTee;
    }
    return recordTee_;
}
