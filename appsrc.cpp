#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

static void cb_need_data(GstElement *appsrc, guint unused_size, gpointer user_data)
{
    GMainLoop *loop = (GMainLoop*)user_data;
    static gboolean white = FALSE;
    static GstClockTime timestamp = 0;
    GstBuffer *buffer;
    guint size;
    guint width = 1000;
    guint height = 1000;
    GstFlowReturn ret;
    size = width*height*2;

    buffer = gst_buffer_new_allocate(NULL, size, NULL);

    // this make the image black/white
    gst_buffer_memset(buffer, 0, white? 0xff:0x00, size);
    white = !white;

    GST_BUFFER_PTS(buffer) = timestamp;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 2);

    timestamp += GST_BUFFER_DURATION(buffer);
    g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);
    if(ret != GST_FLOW_OK)
    {
        g_main_loop_quit(loop);
    }
}
int main(int argc, char** argv)
{
    GMainLoop *loop;
    GstElement *pipeline, *appsrc, *conv, *videosink;
    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);
    /*build */
    pipeline = gst_pipeline_new("my-pipeline");
    appsrc = gst_element_factory_make("appsrc", "src");
    conv = gst_element_factory_make("videoconvert", "conv");
    videosink = gst_element_factory_make("xvimagesink", "videosink");

    // setup
    g_object_set(G_OBJECT(appsrc), "caps",
                 gst_caps_new_simple("video/x-raw",
                                     "format", G_TYPE_STRING, "RGB16",
                                     "width", G_TYPE_INT, 1000,
                                     "height", G_TYPE_INT, 1000,
                                     "framerate", GST_TYPE_FRACTION, 25, 1,
                                     NULL), NULL);
    gst_bin_add_many(GST_BIN(pipeline), appsrc, conv, videosink, NULL);
    gst_element_link_many(appsrc, conv, videosink, NULL);

    // set appsrc
    g_object_set(G_OBJECT(appsrc),
                 "stream-type", 0,
                 "format", GST_FORMAT_TIME, NULL);
    g_signal_connect(appsrc, "need-data", G_CALLBACK(cb_need_data), loop);

    // play
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    // clean up
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);
    return 0;
}
