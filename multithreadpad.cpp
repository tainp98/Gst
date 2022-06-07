#include <gst/gst.h>

static gchar *opt_effects = NULL;
#define DEFAULT_EFFECT "identity, exclusion, navigationtest,"\
    "agingtv, videoflip, vertigotv, gaussianblur, shagadelictv, edgetv"

static GstPad *blockpad;
static GstElement *conv_before;
static GstElement *conv_after;
static GstElement *cur_effect;
static GstElement *pipeline;

static GQueue effects = G_QUEUE_INIT;

static GstPadProbeReturn event_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    GMainLoop *loop = (GMainLoop*)user_data;
    GstElement *next;
    if(GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS)
    {
        g_print("Not Receive EOS\n");
        return GST_PAD_PROBE_PASS;
    }
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    // push current effect back into the queue
    g_queue_push_tail(&effects, gst_object_ref(cur_effect));
    // take next effect from the queue
    next = (GstElement*)g_queue_pop_head(&effects);
    if(next == NULL)
    {
        GST_DEBUG_OBJECT(pad, "no more effects");
        g_main_loop_quit(loop);
        return GST_PAD_PROBE_DROP;
    }

    g_print("Switching from '%s' to '%s'..\n", GST_OBJECT_NAME(cur_effect),
            GST_OBJECT_NAME(next));
    gst_element_set_state(cur_effect, GST_STATE_NULL);

    // remove unlinks automatically
    GST_DEBUG_OBJECT(pipeline, "removing %" GST_PTR_FORMAT, cur_effect);
    gst_bin_remove(GST_BIN(pipeline), cur_effect);

    GST_DEBUG_OBJECT(pipeline, "adding  %s" GST_PTR_FORMAT, next);
    gst_bin_add(GST_BIN(pipeline), next);

    GST_DEBUG_OBJECT(pipeline, "linking..");
    gst_element_link_many(conv_before, next, conv_after, NULL);

    gst_element_set_state(next, GST_STATE_PLAYING);
    cur_effect = next;
    GST_DEBUG_OBJECT(pipeline, "done");
    return GST_PAD_PROBE_DROP;
}

static GstPadProbeReturn
pad_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    GstPad *srcpad, *sinkpad;
    GST_DEBUG_OBJECT(pad, "pad is block now");
    // remove the probe first
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    // install new probe for EOS
    srcpad = gst_element_get_static_pad(cur_effect, "src");
    gst_pad_add_probe(srcpad, GstPadProbeType(GST_PAD_PROBE_TYPE_BLOCK |
                      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), event_probe_cb,
                      user_data, NULL);
    gst_object_unref(srcpad);

    // push EOS into the element, the probe will be fired when the
    // EOS leaves the effect and it has thus drained all of its data
    sinkpad = gst_element_get_static_pad(cur_effect, "sink");
    gst_pad_send_event(sinkpad, gst_event_new_eos());
    gst_object_unref(sinkpad);
    return GST_PAD_PROBE_OK;
}
static gboolean timeout_cb(gpointer user_data)
{
    gst_pad_add_probe(blockpad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                      pad_probe_cb, user_data, NULL);
    return TRUE;
}
static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop*)data;
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

int main(int argc, char** argv)
{
    GstElement *pipeline, *audio_src, *tee, *audio_queue, *audio_convert, *audio_resample, *audio_sink;
    GstElement *video_queue, *video_visual, *video_convert, *video_sink;
    GstElement *audio_bin, *video_bin;
    GstBus *bus;
    GstMessage *msg;
    GstPad *tee_audio_pad, *tee_video_pad;
    GstPad *queue_audio_pad, *queue_video_pad;

    gst_init(&argc, &argv);

    // Create Elements
    audio_src = gst_element_factory_make("audiotestsrc", "audio_source");
    tee = gst_element_factory_make("tee", "tee");
    audio_queue = gst_element_factory_make("queue", "audio_queue");
    audio_convert = gst_element_factory_make("audioconvert", "audio_convert");
    audio_resample = gst_element_factory_make("audioresample", "audio_resample");
    audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");

    video_queue = gst_element_factory_make("queue", "video_queue");
    video_visual = gst_element_factory_make("wavescope", "visual");
    video_convert = gst_element_factory_make("videoconvert", "video_convert");
    video_sink = gst_element_factory_make("autovideosink", "video_sink");

    // Create the empty pipeline
    audio_bin = gst_bin_new("audio_bin");
    video_bin = gst_bin_new("video_bin");
    pipeline = gst_pipeline_new("test-pipeline");

    // Configure elements
    g_object_set(audio_src, "freq", 215.0, NULL);
    g_object_set(video_visual, "shader", 0, "style", 1, NULL);

    // Link all elements that can be automatically linked because they
    // have always pads
    gst_bin_add_many(GST_BIN(audio_bin), audio_queue, audio_convert, audio_resample, audio_sink, NULL);
    gst_element_link_many(audio_queue, audio_convert, audio_resample, audio_sink, NULL);
    gst_bin_add_many(GST_BIN(video_bin), video_queue, video_visual, video_convert, video_sink, NULL);
    gst_element_link_many(video_queue, video_visual, video_convert, video_sink, NULL);
    GstPad *pad = gst_element_get_static_pad(audio_queue, "sink");
    gst_element_add_pad(audio_bin, gst_ghost_pad_new("sink", pad));
    pad = gst_element_get_static_pad(video_queue, "sink");
    gst_element_add_pad(video_bin, gst_ghost_pad_new("sink", pad));
    gst_object_unref(pad);
    gst_bin_add_many(GST_BIN(pipeline), audio_src, tee, audio_bin, video_bin, NULL);
    if(gst_element_link_many(audio_src, tee, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked\n");
        return -1;
    }

    // Manually link the Tee, which has Request pads
    tee_audio_pad = gst_element_get_request_pad(tee, "src_%u");
    g_print("Obtain request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));
    queue_audio_pad = gst_element_get_static_pad(audio_bin, "sink");
    tee_video_pad = gst_element_get_request_pad(tee, "src_%u");
    g_print("Obtain request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));
    queue_video_pad = gst_element_get_static_pad(video_bin, "sink");

    gst_pad_link(tee_audio_pad, queue_audio_pad);
    gst_pad_link(tee_video_pad, queue_video_pad);
    gst_object_unref(queue_audio_pad);
    gst_object_unref(queue_video_pad);

    // Start playing pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Wait until error or eos
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                     GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    // Release the request pads from tee, and unref them
    gst_element_release_request_pad(tee, tee_audio_pad);
    gst_element_release_request_pad(tee, tee_video_pad);
    gst_object_unref(tee_audio_pad);
    gst_object_unref(tee_video_pad);

    // Free resouces
    if(msg != NULL)
    {
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}

