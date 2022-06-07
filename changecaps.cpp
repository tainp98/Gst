#include <gst/gst.h>
#include <stdlib.h>

#define MAX_ROUND 100

int main(int argc, char** argv)
{
    GstElement *pipeline, *filter;
    GstCaps *caps;
    gint width, height;
    gint xdir, ydir;
    gint round;
    GstMessage *message;
    gst_init(&argc, &argv);
    /*build */
    pipeline = gst_parse_launch_full("videotestsrc ! capsfilter name = filter ! "
                                     "ximagesink", NULL, GST_PARSE_FLAG_NONE, NULL);
    g_assert(pipeline != NULL);
    filter = gst_bin_get_by_name(GST_BIN(pipeline), "filter");
    g_assert(filter);
    width = 320;
    height = 240;
    xdir = ydir = -10;
    for(round = 0; round < MAX_ROUND; round++)
    {
        gchar *capsstr;
        g_print("resize to %dx%d (%d/%d)\n", width, height, round, MAX_ROUND);
        capsstr = g_strdup_printf("video/x-raw, width=(int)%d, height=(int)%d",
                                  width, height);
        caps = gst_caps_from_string(capsstr);
        g_free(capsstr);
        g_object_set(filter, "caps", caps, NULL);
        gst_caps_unref(caps);

        if(round == 0)
        {
            gst_element_set_state(pipeline, GST_STATE_PLAYING);
        }
        width += xdir;
        if(width >= 320)
            xdir = -10;
        else if (width < 200)
            xdir = 10;
        height += ydir;
        if(height >= 240)
            ydir = -10;
        else if(height < 150)
            ydir = 10;
        message = gst_bus_poll(GST_ELEMENT_BUS(pipeline), GST_MESSAGE_ERROR, 50*GST_MSECOND);
        if(message)
        {
            g_print("got error\n");
            gst_message_unref(message);
        }
    }
    g_print("done\n");
    gst_object_unref(filter);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}
