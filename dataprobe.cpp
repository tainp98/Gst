#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
static GstPadProbeReturn
cb_have_data(GstPad *pad, GstPadProbeInfo *info,
             gpointer user_data)
{
    gint x, y;
    GstMapInfo map;
    guint16 *ptr, t;
    GstBuffer *buffer;
    gint width = 1000;
    gint height = 1000;
    GST_APP_STREAM_TYPE_SEEKABLE;

    buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    buffer = gst_buffer_make_writable(buffer);

    if(buffer == NULL)
        return GST_PAD_PROBE_OK;
    /* Mapping a buffer can fail */
    if(gst_buffer_map(buffer, &map, GST_MAP_WRITE))
    {
        ptr  = (guint16*)map.data;
        for(y = 0; y < height; y++)
        {
            for(x = 0; x < width/2; x++)
            {
                t = ptr[width-1-x];
                ptr[width-1-x] = ptr[x];
                ptr[x] = t;
            }
            ptr += width;
        }
        gst_buffer_unmap(buffer, &map);
    }
    GST_PAD_PROBE_INFO_DATA(info) = buffer;
    return GST_PAD_PROBE_OK;
}
int main(int argc, char** argv)
{
    GMainLoop *loop;
    GstElement *pipeline, *src, *sink, *filter, *csp;
    GstCaps *filtercaps;
    GstPad *pad;
    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);
    /*build */
    pipeline = gst_pipeline_new("my-pipeline");
    src = gst_element_factory_make("videotestsrc", "src");
    filter = gst_element_factory_make("capsfilter", "filer");
    csp = gst_element_factory_make("videoconvert", "csp");
    sink = gst_element_factory_make("xvimagesink", "sink");

    gst_bin_add_many(GST_BIN(pipeline), src, filter, csp, sink, NULL);
    gst_element_link_many(src, filter, csp, sink, NULL);
    filtercaps = gst_caps_new_simple("video/x-raw",
                                     "format", G_TYPE_STRING, "RGB16",
                                     "width", G_TYPE_INT, 1000,
                                     "height", G_TYPE_INT, 1000,
                                     "framerate", GST_TYPE_FRACTION, 25, 1,
                                     NULL);
    g_object_set(G_OBJECT(filter), "caps", filtercaps, NULL);
    gst_object_unref(filtercaps);

    pad = gst_element_get_static_pad(src, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)cb_have_data, NULL, NULL);
    gst_object_unref(pad);

    // run
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if(gst_element_get_state(pipeline, NULL, NULL, -1) == GST_STATE_CHANGE_FAILURE)
    {
        g_error("Failed to go into Playing state");
    }
    g_print("Running...\n");
    g_main_loop_run(loop);

    // exit
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    return 0;
}
