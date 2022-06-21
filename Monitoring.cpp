#include "Monitoring.h"
#include <QDTApplication.h>
#include <ErrorMessage.h>
#include <QDTLostConnectionEvent.h>
#include <QDTRecoverEvent.h>
#include <StreamingManager.h>
#include <DirTool.h>
#include <OtherTool.h>
namespace vaplatform {
Monitoring::Monitoring(QDTApplication *app, QDTGeneralManager *generalManager)
    : Period (), VStoppableProc (), QDTManager (app, generalManager)
{}

Monitoring::~Monitoring()
{

}

void Monitoring::setContext(QDTGeneralManager *generalManager)
{
    context_ = app_->context();
    vClock_ = std::make_unique<VClock>();
    fpsClock_ = std::make_unique<VClock>();
    fpsClock_->tick();
}

void Monitoring::tickVideoFPS()
{
    videoReaderFPS_.fetch_add(1);
}

void Monitoring::tickTrackerFPS()
{
    trackerFPS_.fetch_add(1);
}

void Monitoring::tickStreamFPS()
{
    streamFPS_.fetch_add(1);
}

void Monitoring::tickSaveImageFPS()
{
    saveImageFPS_.fetch_add(1);
}

void Monitoring::execRecuringTask()
{
    uint32_t counter = 0;
    while (true) {
        vClock_->tick();
        if(packetFrequency_.count(PeriodicType::FPSChecking) > 0 &&
                counter % counter_[PeriodicType::FPSChecking] == 0)
        {
            // FPS logging
            uint64_t videoReaderFPS = videoReaderFPS_.load();
            uint64_t streamFPS = streamFPS_.load();
            double waitTime = fpsClock_->tock(); // micro second
            waitTime = waitTime/1000000; // second
            QDTLog::debug("VideoReaderFPS = {}, StreamFPS = {}",
                          (videoReaderFPS - previousVideoFPS_)/waitTime,
                            (streamFPS - previousStreamFPS_)/waitTime);

            previousVideoFPS_ = videoReaderFPS;
            previousStreamFPS_ = streamFPS;
            fpsClock_->tick();
        }

        if(packetFrequency_.count(PeriodicType::SaveVideo) > 0 &&
                counter % counter_[PeriodicType::SaveVideo] == 0)
        {
            // FPS logging
            static int saveVideoFreq = 0;
            if(saveVideoFreq % 120 == 0)
            {
                std::string saveFolder = app_->generalManager()->streamingManager()->saveFolder();
                float sizeOfSaveFolder = DirTool::sizeOfFolder(saveFolder);
                if(sizeOfSaveFolder > 10000)
                {
                    // remove some oldest file when saveFolder reach out of 10Gb
                    std::string listFile = DirTool::listFileInFolder(saveFolder);
                    auto vecFile = OtherTool::splitString(listFile, ",");
                    std::map<int, std::string> mapFile;
                    for(auto it = vecFile.begin(); it != vecFile.end(); it++)
                    {
                        mapFile[std::stoi(OtherTool::splitString(*it, "-")[0])] = *it;
                    }
                    int count = 0;
                    for(auto it = mapFile.begin(); it != mapFile.end(); it++)
                    {
                        std::string fileName = saveFolder + "/" + it->second;
                        DirTool::removeFile(fileName.c_str());
                        count++;
                        if(count == 50)
                            break;
                    }
                }
            }
            saveVideoFreq++;
        }

        counter++;
        double executeTime = vClock_->tock();
        long sleepTime = (long)(1000000.0/bcnnFrequency_ - executeTime);
        std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));
    }
}
}
