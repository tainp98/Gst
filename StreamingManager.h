#ifndef STREAMINGMANAGER_H
#define STREAMINGMANAGER_H
#include <iostream>
#include <string.h>
#include <thread>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <QDTGeneralManager.h>
#include <RollingFixedBuffer.h>
#include <RollingFixedGenericBuffer.h>
#include <StreamConfig.h>
#include <QDTApplication.h>
#include <QDTContext.h>

namespace vaplatform {

class StreamingManager : public QDTManager {
public:
    explicit StreamingManager(QDTApplication* app, QDTGeneralManager* generalManager);

    ~StreamingManager() override;

    void setContext(QDTGeneralManager* generalManager) override;
    void vStart() override;
    void vStop() override;
    void vRestart() override;
    void reconfigure(std::shared_ptr<StreamConfig> streamConfig);

    static bool wrapper_run(void* pointer);
    static void wrapperOnNeedData(GstAppSrc* _appSrc, guint _size, gpointer _uData);
    static void wrapperOnEnoughData(GstAppSrc* _appSrc, gpointer _uData);
    static gboolean wrapperOnSeekData(GstAppSrc* _appSrc, guint64 _offset, gpointer _uData);
    static GstPadProbeReturn wrapTeeSavePadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

    bool isInitialized();

    void setDrawHL(bool drawHL);
    void setDrawShipDetect(bool drawShipDetect);
    void setDrawTrackObject(bool drawTrackObject);
    void setRecord(bool recordMode);

private:
    void stopRecord();
    void startRecord();
    void run();

    int initPipeline();
    void freePipeline();
    bool setPipelineState(GstState _state, GstClockTime _timeout);
    
    void onNeedData(GstAppSrc* _appSrc, guint _size, gpointer _uData);
    void onEnoughData(GstAppSrc* _appSrc, gpointer _uData);
    gboolean onSeekData(GstAppSrc* _appSrc, guint64 _offset, gpointer _uData);
    GstPadProbeReturn onTeeSavePadProbeCb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    std::string getFileNameByTime();
    void correctTimeLessThanTen(std::string &_inputStr, int _time);

private:
    bool recording_ = false;
    GMainLoop* loop_;
    GstElement* pipeline_;
    std::string pipelineStr_;
    GstBus* bus_;
    GError* gErr_ = nullptr;
    guint busWatchId_;
    GstElement* appSrc_;
    GstElement *videoconvert_, *omxh265enc_, *h265parse_, *tee_, *streamQueue_,
    *rtph265pay_, *saveQueue_, *mpegtsmux_, *fileSink_, *udpSink_;
    GstPad *teeStreamPad_, *teeSavePad_;
    QDTContext* context_ = nullptr;
    std::shared_ptr<StreamConfig> streamConfig_ = nullptr;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> stopRequired_{false};
    std::thread tStream_;
    bool ready_ = false;
    bool changeMode_;
    bool drawHL_{true};
    bool drawShipDetect_{true};
    bool drawTrackObject_{true};
    int width_, height_;
    std::shared_ptr<RollingFixedVImageBuffer> streamingBuffer_;

};
}
#endif // STREAMINGMANAGER_H
