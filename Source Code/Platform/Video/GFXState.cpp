#include "Headers/GFXState.h"
#include "Headers/GFXDevice.h"

namespace Divide {

GPUState::GPUState()
{
    _loaderThread = nullptr;
    _loadingThreadAvailable = false;
    // Atomic boolean use to signal the loading thread to stop
    _closeLoadingThread = false;
    _loadQueueDataReady = false;
}

GPUState::~GPUState()
{
    stopLoaderThread();
}

bool GPUState::startLoaderThread(const DELEGATE_CBK<>& loadingFunction) {
    DIVIDE_ASSERT(_loaderThread == nullptr,
                  "GPUState::startLoaderThread error: double init detected!");

    if (Config::USE_GPU_THREADED_LOADING) {
        _loaderThread.reset(new std::thread(loadingFunction));
    }

    return true;
}

bool GPUState::stopLoaderThread() {
    if (_loaderThread.get()) {
        if (Config::USE_GPU_THREADED_LOADING) {
            DIVIDE_ASSERT(_loaderThread != nullptr,
                          "GPUState::stopLoaderThread error: stop called without "
                          "calling start first!");
            closeLoadingThread(true);
            {
                std::unique_lock<std::mutex> lk(_loadQueueMutex);
                _loadQueueDataReady = true;
                _loadQueueCV.notify_one();
            }
            while (loadingThreadAvailable()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            _loaderThread->join();
            _loaderThread.reset(nullptr);
        }
        return true;
    }

    return false;
}

void GPUState::addToLoadQueue(const DELEGATE_CBK<>& callback) {
#ifdef _DEBUG
    U32 retryCount = 0;
    static const U32 retyLimit = 3;
#endif
    while (!_loadQueue.push(callback)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
#ifdef _DEBUG
        assert(++retryCount < retyLimit);
#endif
    }
    std::unique_lock<std::mutex> lk(_loadQueueMutex);
    _loadQueueDataReady = true;
    _loadQueueCV.notify_one();
}

void GPUState::consumeOneFromQueue() {
    std::unique_lock<std::mutex> lk(_loadQueueMutex);
    _loadQueueCV.wait(lk, [this] {return _loadQueueDataReady; });
    // Try to pull a new element from the queue and process it
    while (!_closeLoadingThread && _loadQueue.consume_one(
        [](DELEGATE_CBK<>& cbk) {
            cbk();
        }))
    {
    }
    _loadQueueDataReady = false;
    lk.unlock();
    _loadQueueCV.notify_one();
}

void GPUState::registerDisplayMode(U8 displayIndex, const GPUVideoMode& mode) {
    if (displayIndex >= _supportedDislpayModes.size()) {
        _supportedDislpayModes.push_back(vectorImpl<GPUVideoMode>());
    }

    vectorImpl<GPUVideoMode>& displayModes = _supportedDislpayModes[displayIndex];

    // this is terribly slow, but should only be called a couple of times and
    // only on video hardware init
    for (GPUVideoMode& crtMode : displayModes) {
        if (crtMode._resolution == mode._resolution &&
            crtMode._bitDepth == mode._bitDepth) {
            U8 crtRefresh = mode._refreshRate.front();
            if (std::find_if(std::begin(crtMode._refreshRate),
                std::end(crtMode._refreshRate),
                [&crtRefresh](U8 refresh)
                -> bool { return refresh == crtRefresh; }) == std::end(crtMode._refreshRate)){
                crtMode._refreshRate.push_back(crtRefresh);
            }
            return;
        }
    }

    displayModes.push_back(mode);
}

};  // namespace Divide