#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <unistd.h>
#include <iostream>
bool change = true;
bool record = true;
GstElement *sink,*pipeline, *mux;
static GstPadProbeReturn
cb_have_data(GstPad *pad, GstPadProbeInfo *info,
             gpointer user_data)
{
//    std::cout << "have data\n";
    static int count = 0;
    static int count2 = 0;
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
    {
        std::cout << "null buffer\n";
        return GST_PAD_PROBE_OK;
    }
    /* Mapping a buffer can fail */
//    if(gst_buffer_map(buffer, &map, GST_MAP_WRITE))
//    {
//        ptr  = (guint16*)map.data;
//        for(y = 0; y < height; y++)
//        {
//            for(x = 0; x < width/2; x++)
//            {
//                t = ptr[width-1-x];
//                ptr[width-1-x] = ptr[x];
//                ptr[x] = t;
//            }
//            ptr += width;
//        }
//        gst_buffer_unmap(buffer, &map);
//    }
    if(count % 200 == 0)
    {
        change = !change;

    }
    if(change)
    {
//        GST_PAD_PROBE_INFO_DATA(info) = NULL;
        if(record)
        {
            gst_element_send_event(sink, gst_event_new_eos());
            usleep(1000);
            gst_element_set_state(sink, GST_STATE_NULL);
            gst_bin_remove(GST_BIN(pipeline), sink);

            record = false;

        }


    }
    else
    {
        if(!record)
        {
            sink = gst_element_factory_make("filesink", NULL);
            std::string fileName = "vd";
            fileName  = fileName + "_" + std::to_string(count) + ".mp4";
            g_object_set(sink, "location", fileName.c_str(), NULL);
            g_object_set(sink, "location", fileName.c_str(), NULL);
            gst_bin_add(GST_BIN(pipeline), sink);
            gst_element_link(mux, sink);
            gst_element_set_state(sink, GST_STATE_PLAYING);
            record = true;
        }
//        GST_PAD_PROBE_INFO_DATA(info) = buffer;
    }
    count++;
    std::cout << "count = " << count << "\n";
    GST_PAD_PROBE_INFO_DATA(info) = buffer;
    return GST_PAD_PROBE_REMOVE;
}
int main(int argc, char** argv)
{
    GMainLoop *loop;
    GstElement *src, *filter, *csp, *encode, *queue, *parser;
    GstCaps *filtercaps;
    GstPad *pad;
    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);
    /*build */
    pipeline = gst_pipeline_new("my-pipeline");
    src = gst_element_factory_make("videotestsrc", "src");
    filter = gst_element_factory_make("capsfilter", "filer");
    csp = gst_element_factory_make("videoconvert", "csp");
    encode = gst_element_factory_make("nvh265enc", "encode");
//    queue = gst_element_factory_make("queue", "queue");
    parser = gst_element_factory_make("h265parse", "parser");
//    g_object_set(parser, "config-interval", 10, NULL);b
    mux = gst_element_factory_make("mpegtsmux", "mux");
    sink = gst_element_factory_make("filesink", NULL);
    g_object_set(sink, "location", "vd.mp4", NULL);

    gst_bin_add_many(GST_BIN(pipeline), src, filter, csp, encode, parser, mux, sink, NULL);
    gst_element_link_many(src, filter, csp, encode, parser, mux, sink, NULL);
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
