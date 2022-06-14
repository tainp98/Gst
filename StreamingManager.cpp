#include "StreamingManager.h"
#include <VCacheManager.h>
#include <ConfigManager.h>
#include <QDTApplication.h>
#include <DrawTool.h>
#include <Monitoring.h>
namespace vaplatform
{

StreamingManager::StreamingManager(QDTApplication *app, QDTGeneralManager *generalManager)
    : QDTManager(app, generalManager)
{
    gst_init(0, nullptr);
    loop_ = g_main_loop_new(NULL, FALSE);
}

StreamingManager::~StreamingManager()
{
}

void StreamingManager::setContext(QDTGeneralManager *generalManager)
{
    QDTManager::setContext(generalManager);
    streamConfig_ = std::dynamic_pointer_cast<StreamConfig>(
                generalManager_->configManager()->getConfig(ConfigType::Stream));
    QDTLog::debug("StreamConfig: width = {}, height = {}, framerate = {}, bitrate = {}, ip = {}, port = {}",
                  streamConfig_->getProperty(StreamProperty::WIDTH).toInt(),
                  streamConfig_->getProperty(StreamProperty::HEIGHT).toInt(),
                  streamConfig_->getProperty(StreamProperty::FRAMERATE).toInt(),
                  streamConfig_->getProperty(StreamProperty::BITRATE).toInt(),
                  streamConfig_->getProperty(StreamProperty::HOSTIP).toString(),
                  streamConfig_->getProperty(StreamProperty::HOSTPORT).toInt());
    streamingBuffer_ = generalManager->vCacheManager()->getStreamingDataBuffer();
    streamingBuffer_->registerReceiver("Stream");
    context_ = app_->context();
}

void StreamingManager::reconfigure(std::shared_ptr<StreamConfig> streamConfig)
{
    streamConfig_ = streamConfig;
}

void StreamingManager::vStart()
{
    if (isRunning_)
        return;
    stopRequired_ = false;
    tStream_ = std::thread(&StreamingManager::run, this);
    isRunning_ = true;
}

void StreamingManager::vStop()
{
    if (!isRunning_)
        return;
    if (this->setPipelineState(GstState::GST_STATE_NULL, GST_CLOCK_TIME_NONE))
    {
        g_main_loop_quit(loop_);
        while (isRunning_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (tStream_.joinable())
            tStream_.join();
        this->freePipeline();
        return;
    }
    else
        return;
}

bool StreamingManager::setPipelineState(GstState _state, GstClockTime _timeout)
{
    if (nullptr == pipeline_)
        return false;

    switch (gst_element_set_state(GST_ELEMENT(pipeline_), _state))
    {
    case GstStateChangeReturn::GST_STATE_CHANGE_FAILURE:
    {
        return false;
    }
    case GstStateChangeReturn::GST_STATE_CHANGE_SUCCESS:
    {
        return true;
    }
    case GstStateChangeReturn::GST_STATE_CHANGE_NO_PREROLL:
    {
        return false;
    }
    case GstStateChangeReturn::GST_STATE_CHANGE_ASYNC:
    {
        GstState currState, pendingState;
        gst_element_get_state(GST_ELEMENT(pipeline_), &currState, &pendingState, _timeout);
        if (currState != _state)
            return false;
        else
            return true;
    }
    }
    return false;
}

void StreamingManager::vRestart()
{
    if (isRunning_)
    {
        vStop();
    }
    tStream_ = std::thread(&StreamingManager::run, this);
    isRunning_ = true;
}

void StreamingManager::wrapperOnEnoughData(GstAppSrc *_appSrc, gpointer _uData)
{
    StreamingManager *itself = (StreamingManager *)_uData;
    return itself->onEnoughData(_appSrc, _uData);
}

void StreamingManager::wrapperOnNeedData(GstAppSrc *_appSrc, guint _size, gpointer _uData)
{
    StreamingManager *itself = (StreamingManager *)_uData;
    return itself->onNeedData(_appSrc, _size, _uData);
}

gboolean StreamingManager::wrapperOnSeekData(GstAppSrc *_appSrc, guint64 _offset, gpointer _uData)
{
    StreamingManager *itself = (StreamingManager *)_uData;
    return itself->onSeekData(_appSrc, _offset, _uData);
}

GstPadProbeReturn StreamingManager::wrapTeeSavePadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer _uData)
{
    StreamingManager *itself = (StreamingManager *)_uData;
    return itself->onTeeSavePadProbeCb(pad, info, _uData);
}

bool StreamingManager::isInitialized()
{
    return ready_;
}

void StreamingManager::setDrawHL(bool drawHL)
{
    drawHL_ = drawHL;
}

void StreamingManager::setDrawShipDetect(bool drawShipDetect)
{
    drawShipDetect_ = drawShipDetect;
}

void StreamingManager::setDrawTrackObject(bool drawTrackObject)
{
    drawTrackObject_ = drawTrackObject;
}

void StreamingManager::setRecord(bool recordMode)
{
    if(recordMode == true)
    {
        if(recording_)
        {
            QDTLog::debug("Pipeline Is Recording");
            return;
        }
        this->startRecord();
    }
    else
    {
        if(!recording_)
        {
            QDTLog::debug("Pipeline Already Stop Record");
            return;
        }
        this->stopRecord();
    }
}

void StreamingManager::stopRecord()
{
    gst_pad_add_probe(teeSavePad_, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                      wrapTeeSavePadProbeCb, this, NULL);
    std::cout << "TimeoutCallBack - Turn off record\n";
    recording_ = false;
}

void StreamingManager::startRecord()
{
    std::cout << "=============================== TimeoutCallBack - Turn on record\n";

    saveQueue_ = gst_element_factory_make("queue", NULL);
    mpegtsmux_ = gst_element_factory_make("mpegtsmux", NULL);
    fileSink_ = gst_element_factory_make("filesink", NULL);
    std::string fileName = this->getFileNameByTime();
    fileName  = fileName + ".mp4";
    g_object_set(fileSink_, "location", fileName.c_str(), NULL);
    gst_bin_add_many(GST_BIN(pipeline_), saveQueue_, mpegtsmux_, fileSink_, NULL);
    gst_element_link_many(saveQueue_, mpegtsmux_, fileSink_, NULL);
    teeSavePad_ = gst_element_get_request_pad(tee_, "src_%u");
    GstPad *queueSavePad = gst_element_get_static_pad(saveQueue_, "sink");

    gst_pad_link(teeSavePad_, queueSavePad);
    gst_object_unref(queueSavePad);

    gst_element_set_state(saveQueue_, GST_STATE_PLAYING);
    gst_element_set_state(mpegtsmux_, GST_STATE_PLAYING);
    gst_element_set_state(fileSink_, GST_STATE_PLAYING);
    recording_ = true;
}

void StreamingManager::onEnoughData(GstAppSrc *_appSrc, gpointer _uData)
{
}

void StreamingManager::onNeedData(GstAppSrc *_appSrc, guint _size, gpointer _uData)
{
    // doing stream data drawing by config from GM
    std::shared_ptr<VImage> streamImage = streamingBuffer_->last("Stream");
    while (streamImage == nullptr)
    {
        streamImage = streamingBuffer_->last("Stream");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    GstBuffer *gstImage = gst_buffer_copy(streamImage->getGstBuffer());
    gst_app_src_push_buffer(_appSrc, gstImage);
    generalManager_->monitoring()->tickStreamFPS();
}

gboolean StreamingManager::onSeekData(GstAppSrc *_appSrc, guint64 _offset, gpointer _uData)
{
    return TRUE;
}

GstPadProbeReturn StreamingManager::onTeeSavePadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    GstPad *srcpad, *sinkpad;
    g_print("blockpad is block now\n");
    // remove the probe first
//    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

    // push EOS into the element, the probe will be fired when the
    // EOS leaves the effect and it has thus drained all of its data
    sinkpad = gst_element_get_static_pad(saveQueue_, "sink");
    gst_pad_unlink(teeSavePad_, sinkpad);
    gst_element_release_request_pad(tee_, teeSavePad_);
    gst_object_unref(teeSavePad_);

    gst_element_send_event(mpegtsmux_, gst_event_new_eos());
    gst_object_unref(sinkpad);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    gst_element_set_state(saveQueue_, GST_STATE_NULL);
    gst_element_set_state(mpegtsmux_, GST_STATE_NULL);
    gst_element_set_state(fileSink_, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(pipeline_), saveQueue_);
    gst_bin_remove(GST_BIN(pipeline_), mpegtsmux_);
    gst_bin_remove(GST_BIN(pipeline_), fileSink_);
    return GST_PAD_PROBE_REMOVE;
}

int StreamingManager::initPipeline()
{
    //  pipeline__str = "appsrc name=mysrc stream-type=0 is-live=true format=3 ! videoconvert ! omxh265enc control-rate=2 target-bitrate="
    //                   + std::to_string(streamConfig_->getBitRate()) +
    //                   " ! video/x-h265, stream-format=(string)byte-stream ! rtph265pay mtu=4096 config-interval=1 ! udpsink auto-multicast=true host="
    //                   + streamConfig_->getHostIP() + " port=" + std::to_string(streamConfig_->getHostPort()) + " async=true sync=true";

    //    pipeline__str = "appsrc name=mysrc stream-type=0 is-live=true format=3 ! videoconvert ! omxh265enc control-rate=2 target-bitrate="
    //                     + std::to_string(streamConfig_->getBitRate()) +
    //                     " ! video/x-h265, stream-format=(string)byte-stream ! h265parse ! tee name=t t. ! queue ! rtph265pay mtu=4096 config-interval=1 "
    //                     " ! udpsink auto-multicast=true host=" + streamConfig_->getHostIP() + " port=" + std::to_string(streamConfig_->getHostPort()) +
    //                     " async=true sync=true t. ! queue ! mpegtsmux ! filesink location=video.mp4";

    // stream
    appSrc_ = gst_element_factory_make("appsrc", "mysrc");
    videoconvert_ = gst_element_factory_make("videoconvert", "videoconvert");
    omxh265enc_ = gst_element_factory_make("omxh265enc", "omxh265enc");
    h265parse_ = gst_element_factory_make("h265parse", "h265parse");
    tee_ = gst_element_factory_make("tee", "tee");
    streamQueue_ = gst_element_factory_make("queue", "streamqueue");
    rtph265pay_ = gst_element_factory_make("rtph265pay", "rtph265pay");
    udpSink_ = gst_element_factory_make("udpsink", "udpsink");
    // create element for save video
    saveQueue_ = gst_element_factory_make("queue", NULL);
    mpegtsmux_ = gst_element_factory_make("mpegtsmux", NULL);
    fileSink_ = gst_element_factory_make("filesink", NULL);

    // set properties for element
    // set appsrc
//    g_object_set(appSrc_, "stream-type", 0, "is-live", TRUE, "format", 3, NULL);
    g_object_set(G_OBJECT(appSrc_),
                 "is-live", FALSE,
                 "num-buffers", -1,
                 "emit-signals", FALSE,
                 "block", TRUE,
                 "size", static_cast<guint64>(streamConfig_->getProperty(StreamProperty::WIDTH).toInt() * streamConfig_->getProperty(StreamProperty::HEIGHT).toInt() * 3 / 2),
                 "max-bytes", static_cast<guint64>(streamConfig_->getProperty(StreamProperty::WIDTH).toInt() * streamConfig_->getProperty(StreamProperty::HEIGHT).toInt() * 3 / 2),
                 "format", GST_FORMAT_TIME,
                 "stream-type", GST_APP_STREAM_TYPE_STREAM,
                 "do-timestamp", TRUE,
                 nullptr);
    std::string capStr = "video/x-raw,width=" + std::to_string(streamConfig_->getProperty(StreamProperty::WIDTH).toInt()) +
            ",height=" + std::to_string(streamConfig_->getProperty(StreamProperty::HEIGHT).toInt()) + ",format=I420,framerate=" +
            std::to_string(streamConfig_->getProperty(StreamProperty::FRAMERATE).toInt()) +
            "/1,interlace-mode=(string)progressive,pixel-aspect-ratio=(fraction)1/1,colormetry=(string)bt601";
    QDTLog::debug("Streaming Caps: {}", capStr);
    GstCaps *caps = gst_caps_from_string((const gchar *)capStr.c_str());
    gst_app_src_set_caps(GST_APP_SRC(appSrc_), caps);
    gst_caps_unref(caps);

    // set omxh265
    g_object_set(omxh265enc_, "control-rate", 2,
                 "target-bitrate", streamConfig_->getProperty(StreamProperty::BITRATE).toInt(),
                 NULL);

    // set rtph265pay
    g_object_set(rtph265pay_, "mtu", 4096,
                 "config-interval", 1, NULL);

    // set udpsink
    g_object_set(udpSink_, "auto-multicast", TRUE,
                 "host", streamConfig_->getProperty(StreamProperty::HOSTIP).toString().c_str(),
                 "port", streamConfig_->getProperty(StreamProperty::HOSTPORT).toInt(),
                 "async", TRUE, "sync", TRUE, NULL);

    // create pipeline and link manually
    pipeline_ = gst_pipeline_new("mypipeline");
    gst_bin_add_many(GST_BIN(pipeline_), appSrc_, videoconvert_, omxh265enc_,
                     h265parse_, tee_, streamQueue_, rtph265pay_, udpSink_, NULL);
    if(gst_element_link_many(appSrc_, videoconvert_, omxh265enc_, h265parse_, tee_, NULL) != TRUE)
    {
        g_print("Link fail: appsrc, videoconvert, omxh265enc, h265parse, tee\n");
        return -1;
    }

    if(gst_element_link_many(streamQueue_, rtph265pay_, udpSink_, NULL) != TRUE)
    {
        g_print("Link fail: streamQueue, rtph265pay, udpSink,\n");
        return -1;
    }

    teeStreamPad_ = gst_element_get_request_pad(tee_, "src_%u");
    GstPad *queueStreamPad = gst_element_get_static_pad(streamQueue_, "sink");
    gst_pad_link(teeStreamPad_, queueStreamPad);
    gst_object_unref(queueStreamPad);

//    pipelineStr_ = "appsrc name=mysrc stream-type=0 is-live=true format=3 ! videoconvert ! omxh265enc control-rate=2 target-bitrate=" + std::to_string(streamConfig_->getProperty(StreamProperty::BITRATE).toInt()) +
//            " ! video/x-h265, stream-format=(string)byte-stream ! h265parse ! rtph265pay mtu=4096 config-interval=1 "
//            " ! udpsink auto-multicast=true host=" +
//            streamConfig_->getProperty(StreamProperty::HOSTIP).toString() +
//            " port=" + std::to_string(streamConfig_->getProperty(StreamProperty::HOSTPORT).toInt()) +
//            " async=true sync=true";

    //    // save video
    //    pipeline__str = "appsrc name=mysrc stream-type=0 is-live=true format=3 ! videoconvert ! omxh265enc control-rate=2 target-bitrate="
    //                     + std::to_string(streamConfig_->getBitRate()) +
    //                     " ! video/x-h265, stream-format=(string)byte-stream ! h265parse ! tee name=t t. ! queue ! mpegtsmux ! filesink location=video.mp4";



    GstAppSrcCallbacks cbs;
    cbs.need_data = wrapperOnNeedData;
    cbs.enough_data = wrapperOnEnoughData;
    cbs.seek_data = wrapperOnSeekData;
    gst_app_src_set_callbacks(GST_APP_SRC_CAST(appSrc_), &cbs, this, NULL);
    return 0;
}

void StreamingManager::run()
{
    if (initPipeline() < 0)
    {
        QDTLog::error("Cannot init Pipeline");
        return;
    }
    QDTLog::debug("Starting Streaming Video");
    GstStateChangeReturn result = gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_PLAYING);
    if (result == GST_STATE_CHANGE_FAILURE)
    {
        QDTLog::error("Could NOT set PLAYING state for pipeline");
    }
    ready_ = true;
    g_main_loop_run(loop_);

    // gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_NULL);
    // gst_object_unref(pipeline_);

    isRunning_ = false;
    QDTLog::debug("End streaming...");
    return;
}

void StreamingManager::freePipeline()
{
    gst_bin_remove(GST_BIN(pipeline_), gst_bin_get_by_name(GST_BIN(pipeline_), "mysrc"));
    gst_bin_remove(GST_BIN(pipeline_), gst_bin_get_by_name(GST_BIN(pipeline_), "videoconvert"));
    gst_bin_remove(GST_BIN(pipeline_), gst_bin_get_by_name(GST_BIN(pipeline_), "encoder"));
    gst_bin_remove(GST_BIN(pipeline_), gst_bin_get_by_name(GST_BIN(pipeline_), "parse"));
    gst_bin_remove(GST_BIN(pipeline_), gst_bin_get_by_name(GST_BIN(pipeline_), "rtph265pay"));
    gst_bin_remove(GST_BIN(pipeline_), gst_bin_get_by_name(GST_BIN(pipeline_), "sink"));
    gst_element_set_state(gst_bin_get_by_name(GST_BIN(pipeline_), "mysrc"), GST_STATE_NULL);
    gst_element_set_state(gst_bin_get_by_name(GST_BIN(pipeline_), "videoconvert"), GST_STATE_NULL);
    gst_element_set_state(gst_bin_get_by_name(GST_BIN(pipeline_), "encoder"), GST_STATE_NULL);
    gst_element_set_state(gst_bin_get_by_name(GST_BIN(pipeline_), "parse"), GST_STATE_NULL);
    gst_element_set_state(gst_bin_get_by_name(GST_BIN(pipeline_), "rtph265pay"), GST_STATE_NULL);
    gst_element_set_state(gst_bin_get_by_name(GST_BIN(pipeline_), "sink"), GST_STATE_NULL);
    gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_NULL);
    gst_object_unref(gst_bin_get_by_name(GST_BIN(pipeline_), "mysrc"));
    gst_object_unref(gst_bin_get_by_name(GST_BIN(pipeline_), "videoconvert"));
    gst_object_unref(gst_bin_get_by_name(GST_BIN(pipeline_), "encoder"));
    gst_object_unref(gst_bin_get_by_name(GST_BIN(pipeline_), "parse"));
    gst_object_unref(gst_bin_get_by_name(GST_BIN(pipeline_), "rtph265pay"));
    gst_object_unref(gst_bin_get_by_name(GST_BIN(pipeline_), "sink"));
    gst_object_unref(pipeline_);
}
std::string StreamingManager::getFileNameByTime() {
    std::string fileName = "";
    std::time_t t = std::time(nullptr);
    std::tm *now = std::localtime(&t);
    fileName += std::to_string(now->tm_year + 1900);
    correctTimeLessThanTen(fileName, now->tm_mon + 1);
    correctTimeLessThanTen(fileName, now->tm_mday);
    correctTimeLessThanTen(fileName, now->tm_hour);
    correctTimeLessThanTen(fileName, now->tm_min);
    correctTimeLessThanTen(fileName, now->tm_sec);
    return fileName;
}

void StreamingManager::correctTimeLessThanTen(std::string &_inputStr, int _time) {
    _inputStr += "_";
    if (_time < 10) {
        _inputStr += "0";
        _inputStr += std::to_string(_time);
    } else {
        _inputStr += std::to_string(_time);
    }
}
}
