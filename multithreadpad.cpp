#include <multithreadpad.h>

static gchar *opt_effects = NULL;
#define DEFAULT_EFFECT "identity, exclusion, navigationtest,"\
    "agingtv, videoflip, vertigotv, gaussianblur, shagadelictv, edgetv"

static GstPad *blockpad;
static GstElement *pipeline;
static GstElement *video_bin;
static GstElement *video_queue, *video_visual, *video_convert, *video_sink;
static GstElement *tee;
static GstElement *current_bin;
static GstElement *curr1, *curr2, *curr3, *curr4;
static GstElement *next1, *next2, *next3, *next4;
GstPad *tee_audio_pad, *tee_video_pad;
static bool have_video = TRUE;

static GQueue effects = G_QUEUE_INIT;

static GstPadProbeReturn event_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    GMainLoop *loop = (GMainLoop*)user_data;
    if(GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS)
    {
        g_print("Not Receive EOS\n");
        return GST_PAD_PROBE_PASS;
    }
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

    // take next effect from the queue
    gst_element_set_state(video_bin, GST_STATE_NULL);

    // remove unlinks automatically
    GST_DEBUG_OBJECT(pipeline, "removing %" GST_PTR_FORMAT, video_bin);
    g_print("after remove video_bin\n");
    gst_bin_remove(GST_BIN(pipeline), video_bin);
    GST_DEBUG_OBJECT(pipeline, "done");
    return GST_PAD_PROBE_DROP;
}

static GstPadProbeReturn
pad_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
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
    sinkpad = gst_element_get_static_pad(video_queue, "sink");
    gst_pad_unlink(tee_video_pad, sinkpad);
    gst_element_release_request_pad(tee, tee_video_pad);
    gst_object_unref(tee_video_pad);
    gst_element_send_event(video_visual, gst_event_new_eos());
    gst_object_unref(sinkpad);
    usleep(1000);
    if(video_queue == NULL)
    {
        g_print("video_queue is nullptr\n");
    }
//    g_queue_push_tail(&effects, gst_object_ref(video_queue));
//    g_queue_push_tail(&effects, gst_object_ref(video_visual));
//    g_queue_push_tail(&effects, gst_object_ref(video_convert));
//    g_queue_push_tail(&effects, gst_object_ref(video_sink));
    gst_element_set_state(video_queue, GST_STATE_NULL);
    gst_element_set_state(video_visual, GST_STATE_NULL);
    gst_element_set_state(video_convert, GST_STATE_NULL);
    gst_element_set_state(video_sink, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(pipeline), video_queue);
    gst_bin_remove(GST_BIN(pipeline), video_visual);
    gst_bin_remove(GST_BIN(pipeline), video_convert);
    gst_bin_remove(GST_BIN(pipeline), video_sink);
//    g_object_unref(video_queue);
//    g_object_unref(video_visual);
//    g_object_unref(video_convert);
//    g_object_unref(video_sink);
//    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    return GST_PAD_PROBE_REMOVE;
}
static gboolean timeout_cb(gpointer user_data)
{
    if(have_video)
    {
        gst_pad_add_probe(tee_video_pad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                          pad_probe_cb, user_data, NULL);
        g_print("after block tee src pad\n");
    }
    else
    {
        g_print("add video_queue\n");
//        video_queue = gst_element_factory_make("queue", "video_queue");
//        video_visual = gst_element_factory_make("wavescope", "visual");
//        video_convert = gst_element_factory_make("videoconvert", "video_convert");
//        video_sink = gst_element_factory_make("autovideosink", "video_sink");
//        video_bin = gst_bin_new("videobin");
//        gst_bin_add_many(GST_BIN(video_bin), video_queue, video_visual, video_convert, video_sink, NULL);
//        gst_element_link_many(video_queue, video_visual, video_convert, video_sink, NULL);
//        GstPad *pad = gst_element_get_static_pad(video_queue, "sink");
//        gst_element_add_pad(video_bin, gst_ghost_pad_new("sink", pad));
//        next1 = (GstElement*)g_queue_pop_head(&effects);
//        next2 = (GstElement*)g_queue_pop_head(&effects);
//        next3 = (GstElement*)g_queue_pop_head(&effects);
//        next4 = (GstElement*)g_queue_pop_head(&effects);
//        if(next1 == NULL)
//        {
//            g_print("next is nullptr\n");
//        }
//        gst_bin_add_many(GST_BIN(pipeline), next1, next2, next3, next4, NULL);
//        gst_element_link_many(next1, next2, next3, next4, NULL);
        video_queue = gst_element_factory_make("queue", NULL);
        video_visual = gst_element_factory_make("wavescope", NULL);
        video_convert = gst_element_factory_make("videoconvert", NULL);
        video_sink = gst_element_factory_make("autovideosink", NULL);
        gst_bin_add_many(GST_BIN(pipeline), video_queue, video_visual, video_convert, video_sink, NULL);
        gst_element_link_many(video_queue, video_visual, video_convert, video_sink, NULL);
        if(tee_video_pad == NULL)
        {
            g_print("tee_video_pad is nullptr\n");
        }
        tee_video_pad = gst_element_get_request_pad(tee, "src_%u");
        g_print("Obtain request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));
        GstPad *queue_video_pad = gst_element_get_static_pad(video_queue, "sink");

        gst_pad_link(tee_video_pad, queue_video_pad);
        gst_object_unref(queue_video_pad);
//        blockpad = gst_element_get_request_pad(tee, "src_%u");
//        gst_element_set_state(next1, GST_STATE_PLAYING);
//        gst_element_set_state(next2, GST_STATE_PLAYING);
//        gst_element_set_state(next3, GST_STATE_PLAYING);
//        gst_element_set_state(next4, GST_STATE_PLAYING);

        gst_element_set_state(video_queue, GST_STATE_PLAYING);
        gst_element_set_state(video_visual, GST_STATE_PLAYING);
        gst_element_set_state(video_convert, GST_STATE_PLAYING);
        gst_element_set_state(video_sink, GST_STATE_PLAYING);

//        video_queue = next1;
//        video_visual = next2;
//        video_convert = next3;
//        video_sink = next4;

//        g_queue_push_tail(&effects, gst_object_ref(next1));
//        g_queue_push_tail(&effects, gst_object_ref(next2));
//        g_queue_push_tail(&effects, gst_object_ref(next3));
//        g_queue_push_tail(&effects, gst_object_ref(next4));
//        current_bin = next;
    }
    have_video = !have_video;
    return TRUE;
}
static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop*)data;
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
    {
        g_print("End of stream\n");

        // take next effect from the queue
//        gst_element_set_state(video_bin, GST_STATE_NULL);

//        // remove unlinks automatically
//        GST_DEBUG_OBJECT(pipeline, "removing %" GST_PTR_FORMAT, video_bin);

//        g_queue_push_tail(&effects, gst_object_ref(video_bin));
//        gst_bin_remove(GST_BIN(pipeline), video_bin);
//        g_print("after remove video_bin\n");
//        GST_DEBUG_OBJECT(pipeline, "done");
//        gst_element_release_request_pad(tee, blockpad);
//        gst_object_unref(blockpad);

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
    GMainLoop *loop;
    GstElement *audio_src, *audio_queue, *audio_convert, *audio_resample, *audio_sink;
//    GstElement *video_visual, *video_convert, *video_sink;
    GstElement *audio_bin;
//    GstPad *tee_audio_pad, *tee_video_pad;
    GstPad *queue_audio_pad, *queue_video_pad;

    gst_init(&argc, &argv);

    // Create Elements
    audio_src = gst_element_factory_make("audiotestsrc", "audio_source");
    tee = gst_element_factory_make("tee", "tee");
    audio_queue = gst_element_factory_make("queue", "audio_queue");
    audio_convert = gst_element_factory_make("audioconvert", "audio_convert");
    audio_resample = gst_element_factory_make("audioresample", "audio_resample");
    audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");

//    g_queue_push_tail(&effects, gst_element_factory_make("queue", NULL));
    video_queue = gst_element_factory_make("queue", NULL);
    video_visual = gst_element_factory_make("wavescope", NULL);
    video_convert = gst_element_factory_make("videoconvert", NULL);
    video_sink = gst_element_factory_make("autovideosink", NULL);

    // Create the empty pipeline
//    audio_bin = gst_bin_new(NULL);
    pipeline = gst_pipeline_new("pipeline");

    // Configure elements
    g_object_set(audio_src, "freq", 215.0, NULL);
    g_object_set(video_visual, "shader", 0, "style", 1, NULL);

    // Link all elements that can be automatically linked because they
    // have always pads
//    gst_bin_add_many(GST_BIN(audio_bin), audio_queue, audio_convert, audio_resample, audio_sink, NULL);
//    gst_element_link_many(audio_queue, audio_convert, audio_resample, audio_sink, NULL);
//    gst_bin_add_many(GST_BIN(video_bin), video_queue, video_visual, video_convert, video_sink, NULL);
//    gst_element_link_many(video_queue, video_visual, video_convert, video_sink, NULL);
//    GstPad *pad = gst_element_get_static_pad(audio_queue, "sink");
//    gst_element_add_pad(audio_bin, gst_ghost_pad_new("sink", pad));
//    pad = gst_element_get_static_pad(video_queue, "sink");
//    gst_element_add_pad(video_bin, gst_ghost_pad_new("sink", pad));
//    gst_object_unref(pad);
    gst_bin_add_many(GST_BIN(pipeline), audio_src, tee, audio_queue, audio_convert, audio_resample, audio_sink, video_queue,
                     video_visual, video_convert, video_sink, NULL);
    if(gst_element_link_many(audio_src, tee, NULL) != TRUE
            || gst_element_link_many(video_queue, video_visual, video_convert, video_sink, NULL) != TRUE
            || gst_element_link_many(audio_queue, audio_convert, audio_resample, audio_sink, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked\n");
        return -1;
    }

    // Manually link the Tee, which has Request pads
    tee_audio_pad = gst_element_get_request_pad(tee, "src_%u");
    g_print("Obtain request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));
    queue_audio_pad = gst_element_get_static_pad(audio_queue, "sink");
    tee_video_pad = gst_element_get_request_pad(tee, "src_%u");
    g_print("Obtain request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));
    queue_video_pad = gst_element_get_static_pad(video_queue, "sink");

    gst_pad_link(tee_audio_pad, queue_audio_pad);
    gst_pad_link(tee_video_pad, queue_video_pad);
    gst_object_unref(queue_audio_pad);
    gst_object_unref(queue_video_pad);
//    current_bin = video_bin;
//    g_queue_push_tail(&effects, gst_object_ref(video_queue));
//    g_queue_push_tail(&effects, gst_object_ref(video_visual));
//    g_queue_push_tail(&effects, gst_object_ref(video_convert));
//    g_queue_push_tail(&effects, gst_object_ref(video_sink));

    // Start playing pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    loop = g_main_loop_new(NULL, FALSE);
    gst_bus_add_watch(GST_ELEMENT_BUS(pipeline), bus_cb, loop);
    g_timeout_add_seconds(3, timeout_cb, loop);

    g_main_loop_run(loop);

    // Release the request pads from tee, and unref them
    gst_element_release_request_pad(tee, tee_audio_pad);
    gst_element_release_request_pad(tee, tee_video_pad);
    gst_object_unref(tee_audio_pad);
    gst_object_unref(tee_video_pad);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}

