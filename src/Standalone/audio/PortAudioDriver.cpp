#include "PortAudioDriver.h"
#include <QDebug>
#include <QtGlobal>

#include "portaudio.h"
#include "audio/core/SamplesBuffer.h"
#include "persistence/Settings.h"
#include "MainController.h"
#include "log/Logging.h"

#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdio>

#ifdef Q_OS_WIN
    #include <windows.h>
#endif

/*
 * This file contain the platform independent PortAudio code. The platform specific
 * code are in WindowsPortAudioDriver.cpp, MacPortAudioDriver.cpp and in future LinuxPortAudioDriver.cpp.
*/

namespace audio
{

#ifdef Q_OS_WIN
namespace {

void appendBootstrapLog(const char *stage)
{
    char tempPath[MAX_PATH] = {0};
    DWORD tempPathLength = GetTempPathA(MAX_PATH, tempPath);
    if (tempPathLength == 0 || tempPathLength >= MAX_PATH)
        return;

    char logPath[MAX_PATH] = {0};
    int written = snprintf(logPath, MAX_PATH, "%sjamtaba-bootstrap.log", tempPath);
    if (written <= 0 || written >= MAX_PATH)
        return;

    HANDLE logFile = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logFile == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME systemTime;
    GetLocalTime(&systemTime);

    char buffer[256] = {0};
    written = snprintf(buffer, sizeof(buffer),
                       "%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu %s\r\n",
                       systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                       systemTime.wHour, systemTime.wMinute, systemTime.wSecond,
                       systemTime.wMilliseconds, GetCurrentProcessId(), stage);
    if (written > 0) {
        DWORD bytesWritten = 0;
        WriteFile(logFile, buffer, (DWORD)written, &bytesWritten, nullptr);
    }

    CloseHandle(logFile);
}

}
#endif

PortAudioDriver::PortAudioDriver(controller::MainController* mainController, QString audioInputDevice, QString audioOutputDevice, int firstInputIndex, int lastInputIndex, int firstOutputIndex, int lastOutputIndex, int sampleRate, int bufferSize ) :
    AudioDriver(mainController),
    useSystemDefaultDevices(false),
    useNonInterleavedPortAudio(true)
{
#ifdef Q_OS_WIN
    appendBootstrapLog("PortAudioDriver ctor: entered");
#endif
    qCDebug(jtAudio) << QString("initializing portaudio (%1)...").arg(Pa_GetVersionText());
#ifdef Q_OS_WIN
    appendBootstrapLog("PortAudioDriver ctor: before Pa_Initialize");
#endif
    auto error = Pa_Initialize();
#ifdef Q_OS_WIN
    appendBootstrapLog("PortAudioDriver ctor: after Pa_Initialize");
#endif
    if (error != paNoError) {
        qCritical() << "ERROR initializing portaudio:" << Pa_GetErrorText(error);
        return;
    }

#ifdef Q_OS_WIN
    appendBootstrapLog("PortAudioDriver ctor: before getDeviceNames");
#endif
    auto devicesNames = getDeviceNames();
#ifdef Q_OS_WIN
    appendBootstrapLog("PortAudioDriver ctor: after getDeviceNames");
#endif

    qCDebug(jtAudio) << "Device names: " << devicesNames;

    auto devicesFound = devicesNames.contains(audioInputDevice) && devicesNames.contains(audioOutputDevice);
#ifdef Q_OS_WIN
    appendBootstrapLog(devicesFound ? "PortAudioDriver ctor: devicesFound" : "PortAudioDriver ctor: devicesMissing");
#endif

    if (devicesFound) {
        audioInputDeviceIndex = devicesNames.indexOf(audioInputDevice);
        audioOutputDeviceIndex = UseSingleAudioIODevice ? audioInputDeviceIndex : devicesNames.indexOf(audioOutputDevice);
        globalInputRange = ChannelRange(firstInputIndex, (lastInputIndex - firstInputIndex) + 1);
        globalOutputRange = ChannelRange(firstOutputIndex, (lastOutputIndex - firstOutputIndex) + 1);
    }
    else {
        audioInputDeviceIndex = audioOutputDeviceIndex = paNoDevice; // forcing system default device
    }

#ifdef Q_OS_WIN
    appendBootstrapLog("PortAudioDriver ctor: before initPortAudio");
#endif
    auto portAudioInitialized = initPortAudio(sampleRate, bufferSize);
#ifdef Q_OS_WIN
    appendBootstrapLog("PortAudioDriver ctor: after initPortAudio");
#endif

    if (portAudioInitialized) {
        if (!devicesFound) {

            // if the previous device not found store the default system device as 'last used device'

            auto firstIn = globalInputRange.getFirstChannel();
            auto firstOut = globalOutputRange.getFirstChannel();
            auto lastIn = globalInputRange.getLastChannel();
            auto lastOut = globalOutputRange.getLastChannel();
            auto inputDevice = (audioInputDeviceIndex >= 0 && audioInputDeviceIndex < devicesNames.size()) ? devicesNames[audioInputDeviceIndex] : "";
            auto outputDevice = (audioOutputDeviceIndex >= 0 && audioOutputDeviceIndex < devicesNames.size()) ? devicesNames[audioOutputDeviceIndex] : "";

            mainController->storeIOSettings(
                        firstIn, lastIn,            // input channels
                        firstOut, lastOut,          // output channels
                        inputDevice, outputDevice   // I/O device names
            );
        }
    }
    else {
        audioInputDeviceIndex = audioOutputDeviceIndex = paNoDevice;
    }

#ifdef Q_OS_WIN
    appendBootstrapLog("PortAudioDriver ctor: exit");
#endif
}

QStringList PortAudioDriver::getDeviceNames() const
{
    QStringList devicesNames;

    auto deviceCount = Pa_GetDeviceCount();
    for (int i = 0; i < deviceCount; ++i) {
        auto deviceInfo = Pa_GetDeviceInfo(i);
        devicesNames << QString(deviceInfo->name);
    }

    return devicesNames;
}

bool PortAudioDriver::canBeStarted() const
{
    if (useSystemDefaultDevices) {
        return     Pa_GetDefaultInputDevice()  != paNoDevice
                && Pa_GetDefaultOutputDevice() != paNoDevice; // we need output
    }

    return     audioInputDeviceIndex  != paNoDevice
            && audioOutputDeviceIndex != paNoDevice;
}

int PortAudioDriver::getAudioInputDeviceIndex() const
{
    return useSystemDefaultDevices ? Pa_GetDefaultInputDevice() :audioInputDeviceIndex;
}

int PortAudioDriver::getAudioOutputDeviceIndex() const
{
    return useSystemDefaultDevices ? Pa_GetDefaultOutputDevice() :audioOutputDeviceIndex;
}


bool PortAudioDriver::initPortAudio(int sampleRate, int bufferSize)
{

    paStream = nullptr;// inputBuffer = outputBuffer = NULL;

    // check for invalid audio device index
    if (!useSystemDefaultDevices) {
        if (audioInputDeviceIndex < 0 ||  audioInputDeviceIndex  >= Pa_GetDeviceCount() ) {
            qCDebug(jtAudio) << "Trying to use default audio device to input";
            audioInputDeviceIndex = Pa_GetDefaultInputDevice();
            if (audioInputDeviceIndex == paNoDevice) {
                audioInputDeviceIndex = Pa_GetDefaultInputDevice();
            }
        }

        if (audioOutputDeviceIndex < 0 ||  audioOutputDeviceIndex  >= Pa_GetDeviceCount() ) {
            qCDebug(jtAudio) << "Trying to use default audio device to output";
            audioOutputDeviceIndex = Pa_GetDefaultOutputDevice();
            if (audioOutputDeviceIndex == paNoDevice) {
                audioOutputDeviceIndex = Pa_GetDefaultOutputDevice();
            }
        }
    }

    ensureInputRangeIsValid();
    ensureOutputRangeIsValid();

    // set sample rate
    this->sampleRate = (sampleRate >= 44100 && sampleRate <= 192000) ? sampleRate : 44100;
    PaDeviceIndex device = useSystemDefaultDevices ? Pa_GetDefaultOutputDevice() : audioOutputDeviceIndex;

    if (device != paNoDevice) { // avoid query sample rates in a invalid device
        QList<int> validSampleRates = getValidSampleRates(device);
        if (!validSampleRates.isEmpty()) {
            if (this->sampleRate > validSampleRates.last()) {
                this->sampleRate = validSampleRates.last(); // use the max supported sample rate
            }
            if (!validSampleRates.contains(this->sampleRate)) {
                this->sampleRate = validSampleRates.first();
            }
        }
    }

    this->bufferSize = bufferSize;
    if (device != paNoDevice) {
        QList<int> validBufferSizes = getValidBufferSizes(device);
        if (!validBufferSizes.isEmpty()) {
            if (this->bufferSize < validBufferSizes.first()) {
                this->bufferSize = validBufferSizes.first(); // use the minimum supported buffer size
            }
            if (this->bufferSize > validBufferSizes.last()) {
                this->bufferSize = validBufferSizes.last(); // use the max supported buffer size
            }
            if (!validBufferSizes.contains(this->bufferSize)) {
                this->bufferSize = validBufferSizes.first();
            }
        }
    }
    return true;
}

void PortAudioDriver::ensureOutputRangeIsValid()
{
    // check if outputs are valid
    PaDeviceIndex device = useSystemDefaultDevices ? Pa_GetDefaultOutputDevice() : audioOutputDeviceIndex;
    if (device != paNoDevice) {
        int outputsCount = globalOutputRange.getChannels();
        int maxOutputs = getMaxOutputs();
        if (outputsCount > maxOutputs || globalOutputRange.getFirstChannel() >= maxOutputs || outputsCount <= 0) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(device);
            if (info)
                globalOutputRange = ChannelRange(info->defaultLowOutputLatency, std::min(2, info->maxOutputChannels));
        }
    }
}

void PortAudioDriver::ensureInputRangeIsValid()
{
    // check if inputs are valid for selected device
    PaDeviceIndex device = useSystemDefaultDevices ? Pa_GetDefaultInputDevice() : audioInputDeviceIndex;
    if (device != paNoDevice) {
        int inputsCount = globalInputRange.getChannels();
        int maxInputs = getMaxInputs();
        if (inputsCount > maxInputs || globalInputRange.getFirstChannel() >= maxInputs || inputsCount <= 0 ) {
            // const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(inputDeviceIndex);
            globalInputRange = ChannelRange( 0, std::min(maxInputs, 1));
        }
    }
}

PortAudioDriver::~PortAudioDriver()
{
    qCDebug(jtAudio) << "PortAudioDriver destructor";
}

// this method just convert portaudio void* inputBuffer to a float[][] buffer, and do the same for outputs
void PortAudioDriver::translatePortAudioCallBack(const void *in, void *out, unsigned long framesPerBuffer)
{
    const uint bytesToProcess = framesPerBuffer * sizeof(float);

    // prepare buffers and expose then to application process
    inputBuffer.setFrameLenght(framesPerBuffer);
    outputBuffer.setFrameLenght(framesPerBuffer);
    if (!globalInputRange.isEmpty()) {
        int inputChannels = globalInputRange.getChannels();
        if (useNonInterleavedPortAudio) {
            float **inputs = (float**)in;
            for (int c = 0; c < inputChannels; c++) {
                std::memcpy(inputBuffer.getSamplesArray(c), inputs[c], bytesToProcess);
            }
        }
        else {
            const float *inputs = static_cast<const float *>(in);
            for (int c = 0; c < inputChannels; ++c) {
                float *channelData = inputBuffer.getSamplesArray(c);
                for (unsigned long s = 0; s < framesPerBuffer; ++s)
                    channelData[s] = inputs[s * inputChannels + c];
            }
        }
    }
    else {
        inputBuffer.zero();
    }

    outputBuffer.zero();


    // all application audio processing is computed here
    if (mainController) {
        mainController->process(inputBuffer, outputBuffer, sampleRate);
    }

    // convert application output buffers to portaudio format
    int outputChannels = globalOutputRange.getChannels();
    if (useNonInterleavedPortAudio) {
        float **outputs = static_cast<float**>(out);
        for (int c = 0; c < outputChannels; c++){
            std::memcpy(outputs[c], outputBuffer.getSamplesArray(c), bytesToProcess);
        }
    }
    else {
        float *outputs = static_cast<float *>(out);
        for (unsigned long s = 0; s < framesPerBuffer; ++s) {
            for (int c = 0; c < outputChannels; ++c)
                outputs[s * outputChannels + c] = outputBuffer.getSamplesArray(c)[s];
        }
    }
}

// friend function, receive the pointer to PortAudioDriver instance in userData param
int portaudioCallBack(const void *inputBuffer, void *outputBuffer,
                      unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* /*timeInfo*/,
                      PaStreamCallbackFlags /*statusFlags*/, void *userData)
{
    //qDebug() << "portAudioCallBack  Thread ID: " << QThread::currentThreadId();
    PortAudioDriver* instance = static_cast<PortAudioDriver*>(userData);
    instance->translatePortAudioCallBack(inputBuffer, outputBuffer, framesPerBuffer);
    return paContinue;
}


bool PortAudioDriver::start()
{
    PaDeviceIndex device = useSystemDefaultDevices ? Pa_GetDefaultOutputDevice() : audioOutputDeviceIndex;
    if (device == paNoDevice) {
        qCWarning(jtAudio) << "No audio output devices found";
        return false;
    }
    device = useSystemDefaultDevices ? Pa_GetDefaultInputDevice() : audioInputDeviceIndex;
    if (device == paNoDevice) {
        qCWarning(jtAudio) << "No audio input devices found";
        return false;
    }

    stop();

    if (!useSystemDefaultDevices) {
        qCInfo(jtAudio) << "Starting Input  portaudio driver using" << getAudioInputDeviceName(audioInputDeviceIndex) << " as device.";
        qCInfo(jtAudio) << "Starting Output portaudio driver using" << getAudioInputDeviceName(audioOutputDeviceIndex) << " as device.";
    }
    else {
        qCDebug(jtAudio) << "Starting portaudio using" << getAudioInputDeviceName() << " as input device.";
        qCDebug(jtAudio) << "Starting portaudio using" << getAudioOutputDeviceName() << " as output device.";
    }

    ensureInputRangeIsValid();
    ensureOutputRangeIsValid();

    recreateBuffers(); //adjust the input and output buffers channels

    unsigned long framesPerBuffer = bufferSize; // paFramesPerBufferUnspecified;
    qCDebug(jtAudio) << "Starting portaudio using" << framesPerBuffer << " as buffer size.";
    PaSampleFormat sampleFormat = paFloat32 | paNonInterleaved;
    useNonInterleavedPortAudio = true;

    PaStreamParameters inputParams;
    inputParams.channelCount = globalInputRange.getChannels();
    inputParams.device = useSystemDefaultDevices ? Pa_GetDefaultInputDevice() : audioInputDeviceIndex;
    inputParams.sampleFormat = sampleFormat;
    const PaDeviceInfo *inputDeviceInfo = Pa_GetDeviceInfo(inputParams.device);
    inputParams.suggestedLatency = inputDeviceInfo ? inputDeviceInfo->defaultLowInputLatency : 0;
    inputParams.hostApiSpecificStreamInfo = NULL;

    configureHostSpecificInputParameters(inputParams); // this can be different in different operational systems

    //+++++++++ OUTPUT
    PaStreamParameters outputParams;
    outputParams.channelCount = globalOutputRange.getChannels();// */outputChannels;
    outputParams.device = useSystemDefaultDevices ? Pa_GetDefaultOutputDevice() : audioOutputDeviceIndex;
    outputParams.sampleFormat = sampleFormat;
    const PaDeviceInfo *outputDeviceInfo = Pa_GetDeviceInfo(outputParams.device);
    outputParams.suggestedLatency = outputDeviceInfo ? outputDeviceInfo->defaultLowOutputLatency : 0;
    outputParams.hostApiSpecificStreamInfo = NULL;

    configureHostSpecificOutputParameters(outputParams);

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    bool willUseInputParams = !globalInputRange.isEmpty();
    if (willUseInputParams) {
        qCDebug(jtAudio) << "Trying initialize portaudio using inputParams and samplerate=" << sampleRate;
    }
    else {
        qCDebug(jtAudio) << "Trying initialize portaudio WITHOUT inputParams because globalInputRange is empty!";
        qCDebug(jtAudio) << "Detected inputs for " << getAudioInputDeviceName(audioInputDeviceIndex) << ":" << getMaxInputs();
    }


    if (globalOutputRange.isEmpty()) {
        qCInfo(jtAudio) << "No output devices channels found";
        releaseHostSpecificParameters(inputParams, outputParams);
        return false;
    }

    auto retryWithDefaultSampleRate = [&](PaError error, const PaDeviceInfo *deviceInfo, const PaStreamParameters *input, const PaStreamParameters *output) -> PaError {
        if (error == paNoError || !deviceInfo || deviceInfo->defaultSampleRate <= 0)
            return error;

        int fallbackSampleRate = qRound(deviceInfo->defaultSampleRate);
        if (fallbackSampleRate == sampleRate)
            return error;

        PaError fallbackError = Pa_IsFormatSupported(input, output, fallbackSampleRate);
        if (fallbackError == paNoError) {
            qWarning() << "Retrying PortAudio with device default sample rate" << fallbackSampleRate
                       << "instead of" << sampleRate;
            this->sampleRate = fallbackSampleRate;
            return paNoError;
        }

        return error;
    };

    auto retryWithInterleavedFormat = [&](PaError error) -> PaError {
        if (error == paNoError || sampleFormat == paFloat32)
            return error;

        sampleFormat = paFloat32;
        useNonInterleavedPortAudio = false;
        inputParams.sampleFormat = sampleFormat;
        outputParams.sampleFormat = sampleFormat;

        PaError formatError = Pa_IsFormatSupported(nullptr, &outputParams, this->sampleRate);
        formatError = retryWithDefaultSampleRate(formatError, outputDeviceInfo, nullptr, &outputParams);
        if (formatError == paNoError)
            return paNoError;

        useNonInterleavedPortAudio = true;
        sampleFormat = paFloat32 | paNonInterleaved;
        inputParams.sampleFormat = sampleFormat;
        outputParams.sampleFormat = sampleFormat;
        return error;
    };

    // Older PortAudio/CoreAudio combinations may reject the format probe but still
    // open successfully, so treat this as diagnostics and let Pa_OpenStream decide.
    PaError error =  Pa_IsFormatSupported(nullptr, &outputParams, sampleRate);
    error = retryWithDefaultSampleRate(error, outputDeviceInfo, nullptr, &outputParams);
    error = retryWithInterleavedFormat(error);
    if (error != paNoError) {
        qWarning() << "Output format probe failed, trying stream open anyway:" <<
                      Pa_GetErrorText(error) <<
                      "sampleRate:" << this->sampleRate <<
                      "channels:" << outputParams.channelCount;
    }


    // test if input format is supported
    if (!globalInputRange.isEmpty()) {
        error =  Pa_IsFormatSupported(&inputParams, nullptr, this->sampleRate);
        error = retryWithDefaultSampleRate(error, inputDeviceInfo, &inputParams, nullptr);
        if (error != paNoError && !useNonInterleavedPortAudio) {
            error = Pa_IsFormatSupported(&inputParams, nullptr, this->sampleRate);
        }
        if (error != paNoError) {
            qWarning() << "Input format probe failed, trying stream open anyway:" <<
                          Pa_GetErrorText(error) <<
                          "sampleRate:" << this->sampleRate <<
                          "channels:" << inputParams.channelCount;
        }
    }

    auto tryOpenStream = [&](int streamSampleRate, PaSampleFormat streamFormat, bool nonInterleaved) -> PaError {
        useNonInterleavedPortAudio = nonInterleaved;
        inputParams.sampleFormat = streamFormat;
        outputParams.sampleFormat = streamFormat;
        paStream = NULL;
        return Pa_OpenStream(&paStream,
                             (!globalInputRange.isEmpty()) ? (&inputParams) : NULL,
                             &outputParams,
                             streamSampleRate,
                             framesPerBuffer,
                             paNoFlag,
                             portaudioCallBack,
                             (void*)this);
    };

    error = tryOpenStream(this->sampleRate, sampleFormat, useNonInterleavedPortAudio);
    if (error != paNoError && useNonInterleavedPortAudio) {
        qWarning() << "Retrying PortAudio stream open with interleaved float32";
        error = tryOpenStream(this->sampleRate, paFloat32, false);
    }

    int outputDefaultSampleRate = outputDeviceInfo ? qRound(outputDeviceInfo->defaultSampleRate) : 0;
    if (error != paNoError && outputDefaultSampleRate > 0 && outputDefaultSampleRate != this->sampleRate) {
        qWarning() << "Retrying PortAudio stream open with output default sample rate"
                   << outputDefaultSampleRate;
        this->sampleRate = outputDefaultSampleRate;
        error = tryOpenStream(this->sampleRate, paFloat32, false);
    }

    if (error != paNoError) {
        qCritical() << "Error opening portaudio stream:"
                    << Pa_GetErrorText(error)
                    << "sampleRate:" << this->sampleRate
                    << "channels:" << outputParams.channelCount;
        releaseHostSpecificParameters(inputParams, outputParams);
        return false;
    }
    if (paStream != NULL) {
        preInitializePortAudioStream(paStream);
        error = Pa_StartStream(paStream);
        if (error != paNoError) {
            releaseHostSpecificParameters(inputParams, outputParams);
            return false;
        }
    }
    qCDebug(jtAudio) << "Portaudio driver started ok!";
    emit started();

    releaseHostSpecificParameters(inputParams, outputParams);

    return true;
}

QList<int> PortAudioDriver::getValidSampleRates(int deviceIndex) const
{
    QList<int> validSRs;
    const PaDeviceInfo *dev = Pa_GetDeviceInfo(deviceIndex);
    if (!dev || dev->maxOutputChannels <= 0)
        return validSRs;

    PaStreamParameters outputParams;
    outputParams.channelCount = std::min<int>(2, dev->maxOutputChannels);
    outputParams.device = deviceIndex;
    outputParams.sampleFormat = paFloat32;
    outputParams.suggestedLatency = dev ? dev->defaultLowOutputLatency : 0;
    outputParams.hostApiSpecificStreamInfo = NULL;
    configureHostSpecificOutputParameters(outputParams);

    QList<int> candidateSampleRates{44100, 48000, 96000, 192000};
    int defaultSampleRate = qRound(dev->defaultSampleRate);
    if (defaultSampleRate > 0 && !candidateSampleRates.contains(defaultSampleRate))
        candidateSampleRates.prepend(defaultSampleRate);

    for (int sampleRate : qAsConst(candidateSampleRates)) {
        PaError error = Pa_IsFormatSupported(nullptr, &outputParams, sampleRate);
        if (error == paNoError && !validSRs.contains(sampleRate))
            validSRs.append(sampleRate);
    }

    std::sort(validSRs.begin(), validSRs.end());
    releaseHostSpecificParameters(outputParams, outputParams);
    return validSRs;
}

void PortAudioDriver::stop(bool refreshDevicesList)
{

    if (paStream != NULL) {
        if (!Pa_IsStreamStopped(paStream)) {
            qCDebug(jtAudio) << "Stopping portaudio driver ...";
            PaError error = Pa_CloseStream(paStream);
            if (error != paNoError) {
                qCritical() << "   Error closing portaudio stream: " << Pa_GetErrorText(error);
            }
            emit stopped();
            qCDebug(jtAudio) << "Portaudio driver stoped!";
        }
    }
    if (refreshDevicesList) {
        qCDebug(jtAudio) << "   Refreshing portaudio devices list";
        Pa_Terminate(); // terminate and reinitialize to refresh portaudio internal devices list
        Pa_Initialize();
    }


}

void PortAudioDriver::release()
{
    qCDebug(jtAudio) << "releasing portaudio resources...";
    stop();
    Pa_Terminate();
    qCDebug(jtAudio) << "portaudio terminated!";
}

int PortAudioDriver::getMaxInputs() const
{
    PaDeviceIndex device = useSystemDefaultDevices ? Pa_GetDefaultInputDevice() : audioInputDeviceIndex;

    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
    if (deviceInfo)
        return deviceInfo->maxInputChannels;

    return 0;
}

int PortAudioDriver::getMaxOutputs() const
{
    PaDeviceIndex device = useSystemDefaultDevices ? Pa_GetDefaultOutputDevice() : audioOutputDeviceIndex;

    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
    if (deviceInfo)
        return deviceInfo->maxOutputChannels;

    return 0;
}

void PortAudioDriver::setAudioInputDeviceIndex(PaDeviceIndex index)
{
    stop();
    audioInputDeviceIndex = !useSystemDefaultDevices ? index: paNoDevice;
}

void PortAudioDriver::setAudioOutputDeviceIndex(PaDeviceIndex index)
{
    stop();
    audioOutputDeviceIndex = !useSystemDefaultDevices ? index: paNoDevice;
}

QString PortAudioDriver::getAudioInputDeviceName(int index) const
{
    index = useSystemDefaultDevices ? Pa_GetDefaultInputDevice() : index == CurrentAudioDeviceSelection ? audioInputDeviceIndex : index;
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(index);
    return deviceInfo? deviceInfo->name : "Error!";
}

QString PortAudioDriver::getAudioOutputDeviceName(int index) const
{
    index = useSystemDefaultDevices ? Pa_GetDefaultOutputDevice() : index == CurrentAudioDeviceSelection ? audioOutputDeviceIndex : index;
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(index);
    return deviceInfo? deviceInfo->name : "Error!";
}

QString PortAudioDriver::getAudioDeviceInfo(int index, unsigned& nIn, unsigned& nOut) const
{
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(index);
    nIn =  deviceInfo ? (unsigned) deviceInfo->maxInputChannels : 0;
    nOut = deviceInfo ? (unsigned) deviceInfo->maxOutputChannels : 0;
    return deviceInfo ? deviceInfo->name : "Error!";
}

int PortAudioDriver::getDevicesCount() const
{
    return Pa_GetDeviceCount();
}

} // namespace
