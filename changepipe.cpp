#include <gst/gst.h>

static gchar *opt_effects = NULL;
#define DEFAULT_EFFECT "identity"

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

static void on_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
    GstPad *sinkpad;
    GstElement *decoder = (GstElement*)data;
    g_print("Dynamic pad created, linking demuxer/decoder\n");
    sinkpad = gst_element_get_static_pad(decoder, "sink");
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
}

int main(int argc, char** argv)
{
    GOptionEntry options[] = {
        {"effects", 'e', 0, G_OPTION_ARG_STRING, &opt_effects,
        "Effects to use (comma-separated list of element names)", NULL},
        {NULL}
    };
    GOptionContext *ctx;
    GError *err = NULL;
    GMainLoop *loop;
    GstElement *src, *q1, *q2, *effect, *filter1, *filter2, *sink, *decodebin, *videoconvert;
    gchar **effect_names, **e;

    ctx = g_option_context_new("");
    g_option_context_add_main_entries(ctx, options, NULL);
    g_option_context_add_group(ctx, gst_init_get_option_group());
    if(!g_option_context_parse(ctx, &argc, &argv, &err))
    {
        g_print("Error initializing: %s\n", err->message);
        g_clear_error(&err);
        g_option_context_free(ctx);
        return 1;
    }
    g_option_context_free(ctx);

    if(opt_effects != NULL)
    {
        effect_names = g_strsplit(opt_effects, ",", -1);
    }
    else
        effect_names = g_strsplit(DEFAULT_EFFECT, ",", -1);

    for(e = effect_names; e != NULL && *e != NULL; ++e)
    {
        GstElement *el;
        el = gst_element_factory_make(*e, *e);
        if(el)
        {
            g_print("Adding effect '%s'\n", *e);
            g_queue_push_tail(&effects, el);
        }
    }

    pipeline = gst_pipeline_new("pipeline");
    src = gst_element_factory_make("filesrc", "video-src");
    g_object_set(src, "location", "/home/tainp/Videos/video.mp4", NULL);
    decodebin = gst_element_factory_make("decodebin", "decode");
    videoconvert = gst_element_factory_make("videoconvert", "video-convert");
    filter1 = gst_element_factory_make("capsfilter", NULL);
    gst_util_set_object_arg(G_OBJECT(filter1), "caps",
                            "video/x-raw, width=320, height=240, "
                            "format={ I420, YV12, YUY2, UYVY, AYUV, Y41B, Y42B, "
                            "YVYU, Y444, v210, v216, NV12, NV21, UYVP, A420,"
                            "YUV9, YVU9, IYU1 }");
    q1 = gst_element_factory_make("queue", NULL);
    blockpad = gst_element_get_static_pad(q1, "src");
    conv_before = gst_element_factory_make("videoconvert", "conv_before");
    cur_effect = (GstElement*)g_queue_pop_head(&effects);
//    cur_effect = effect;
    conv_after = gst_element_factory_make("videoconvert", NULL);
    q2 = gst_element_factory_make("queue", NULL);
    filter2 = gst_element_factory_make("capsfilter", NULL);
    gst_util_set_object_arg(G_OBJECT(filter2), "caps",
                            "video/x-raw, width=320, height=240, "
                            "format={ RGBx, BGRx, xRGB, xBGR, RGBA,"
                            "BGRA, ARGB, ABGR, RGB, BGR}");
    sink = gst_element_factory_make("ximagesink", NULL);

    gst_bin_add_many(GST_BIN(pipeline), src, decodebin, videoconvert, filter1, q1, conv_before, cur_effect, conv_after,
                     q2, sink, NULL);
    gst_element_link_many(src, decodebin, NULL);
    gst_element_link_many(filter1, q1, conv_before, cur_effect, conv_after,
                          q2, sink, NULL);

    g_signal_connect (decodebin, "pad-added", G_CALLBACK (on_pad_added), filter1);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    loop = g_main_loop_new(NULL, FALSE);
    gst_bus_add_watch(GST_ELEMENT_BUS(pipeline), bus_cb, loop);

//    g_timeout_add_seconds(1, timeout_cb, loop);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}

