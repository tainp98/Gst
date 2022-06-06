#include <iostream>
#include <gst/gst.h>
using namespace std;

static void read_video_props(GstCaps *caps)
{
    gint width, height;
    const GstStructure* str;
    g_return_if_fail(gst_caps_is_fixed(caps));
    str = gst_caps_get_structure(caps, 0);
    if(!gst_structure_get_int(str, "width", &width) ||
            !gst_structure_get_int(str, "height", &height))
    {
        g_print("No width/height available\n");
        return;
    }
    g_print("The Video Size Of this Set of Capabilities is %dx%d\n", width, height);
}

static gboolean
link_element_with_filter(GstElement* element1, GstElement* element2)
{
    gboolean link_ok;
    GstCaps *caps;
    caps = gst_caps_new_full(
                gst_structure_new("video/x-raw",
                                  "width", G_TYPE_INT, 384,
                                  "height", G_TYPE_INT, 288,
                                  "framerate", GST_TYPE_FRACTION, 25, 1,
                                  NULL),
                gst_structure_new("video/x-bayer",
                                  "width", G_TYPE_INT, 384,
                                  "height", G_TYPE_INT, 288,
                                  "framerate", GST_TYPE_FRACTION, 25, 1,
                                  NULL),
                NULL);
    link_ok = gst_element_link_filtered(element1, element2, caps);
    gst_caps_unref(caps);
    if(!link_ok)
    {
        g_warning("Failed to link element1 and element2");
    }
    return link_ok;
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
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

static gboolean idle_exit_loop(gpointer data)
{
    g_main_loop_quit((GMainLoop*)data);
    return FALSE;
}

static void cb_typefound(GstElement *typefind, guint probability,
                         GstCaps *caps, gpointer data)
{
    GMainLoop *loop = (GMainLoop*)data;
    gchar *type;
    type = gst_caps_to_string(caps);
    g_print("Media type %s found, probability = %d%%\n", type, probability);
    g_free(type);
    g_idle_add(idle_exit_loop, loop);
}
int main(int argc, char** argv)
{
    GMainLoop *loop;
    GstElement *pipeline, *filesrc, *typefind, *fakesink;
    GstBus *bus;
    guint bus_watch_id;
    gst_init(&argc, &argv);

    loop = g_main_loop_new(NULL, FALSE);
    if(argc != 2)
    {
        g_printerr("Usage: %s <Ogg/Vobis filename>\n", argv[0]);
        return -1;
    }

    /* create pipeline and elements */
    pipeline = gst_pipeline_new("pipe");
    filesrc = gst_element_factory_make("filesrc", "file-source");
    typefind = gst_element_factory_make("typefind", "typefinder");
    fakesink = gst_element_factory_make("fakesink", "sink");
    if(!pipeline || !filesrc || !typefind || !fakesink)
    {
        g_printerr("One element could not be created. Exiting\n");
        return -1;
    }

    g_object_set(G_OBJECT(filesrc), "location", argv[1], NULL);
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);
    g_signal_connect(typefind, "have-type", G_CALLBACK(cb_typefound), loop);

    gst_bin_add_many(GST_BIN(pipeline), filesrc, typefind, fakesink, NULL);
    gst_element_link_many(filesrc, typefind, fakesink, NULL);

    g_print("Now playing: %s\n", argv[1]);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_print("Running...\n");
    g_main_loop_run(loop);

    g_print("Returned, stopping playback\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_print("Deleting pipeline\n");
    gst_object_unref(GST_OBJECT(pipeline));
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);
    return 0;
}
