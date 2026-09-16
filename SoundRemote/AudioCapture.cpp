#include "AudioCapture.h"

#include <Audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>

#include "Util.h"

using namespace boost::asio;
using namespace std::chrono_literals;
using namespace Audio;

template <typename Duration>
struct AwaitableTimer {
    AwaitableTimer(io_context& io, Duration duration)
        : timer_(io, std::chrono::steady_clock::now()), duration_(duration) {}
    bool await_ready() const { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        timer_.expires_at(timer_.expiry() + duration_);
        std::shared_ptr<bool> destroyed = destroyed_;
        timer_.async_wait([h, destroyed](boost::system::error_code ec) mutable {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted) {
                    throw std::runtime_error(Util::makeAppErrorText("Timer1", ec.what()));
                }
            } else {
                if (!*destroyed) {
                    h.resume();
                }
            }
            });
    }
    void await_resume() const noexcept {}
    ~AwaitableTimer() {
        *destroyed_ = true;
    }
private:
    steady_timer timer_;
    Duration duration_;
    std::shared_ptr<bool> destroyed_ = std::make_shared<bool>(false);
};

//Anon namespace for helpers
namespace {
    /// <summary>
    /// Allocates and returns a pointer to a WaveFormat initiated from the passed
    /// <code>Audio::Format</code>.
    /// </summary>
    /// <param name="format">Audio format to initiate from.</param>
    /// <returns>Pointer to the created WaveFormat.</returns>
    WAVEFORMATEXTENSIBLE* createWaveFormat(const Audio::Format& format) {
        auto result =
            reinterpret_cast<WAVEFORMATEXTENSIBLE*>(CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE)));
        if (NULL == result) {
            return NULL;
        }
        result->Format.nChannels = format.channelCount;
        result->Format.nSamplesPerSec = format.sampleRate;
        result->Format.wBitsPerSample = format.sampleSize;
        result->Format.cbSize = 22;
        result->Format.nBlockAlign = result->Format.nChannels * result->Format.wBitsPerSample / 8;
        result->Format.nAvgBytesPerSec = result->Format.nSamplesPerSec * result->Format.nBlockAlign;
        result->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        result->Samples.wValidBitsPerSample = result->Format.wBitsPerSample;
        switch (format.channelCount) {
        case 1:
            result->dwChannelMask = KSAUDIO_SPEAKER_MONO;
            break;
        case 2:
            result->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
            break;
        default:
            result->dwChannelMask = 0;
        }
        switch (format.sampleType) {
        case Audio::SampleType::SignedInt:
        case Audio::SampleType::UnSignedInt:
            result->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            break;
        case Audio::SampleType::Float:
            result->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;
        case Audio::SampleType::Unknown:
            result->SubFormat = KSDATAFORMAT_SUBTYPE_NONE;
            break;
        }
        return result;
    }

    class BufferReleaser {
    public:
        BufferReleaser(IAudioCaptureClient* client, UINT32 nFrames) :
            client_(client), nFrames_(nFrames) {
            assert(client != nullptr);
        }
        HRESULT release() {
            released_ = true;
            return client_->ReleaseBuffer(nFrames_);
        }
        ~BufferReleaser() {
            if (released_) return;
            HRESULT hr = release();
            // If destructor was called and buffer isn't released yet means capture coroutine is
            // being destroyed.
            if (FAILED(hr)) {
                Util::showError(
                    Audio::audioErrorText(hr, Audio::Location::CAPTURE_ACC_RELEASEBUFFER));
            }
        }
        BufferReleaser(const BufferReleaser&) = delete;
    private:
        IAudioCaptureClient* client_ = nullptr;
        UINT32 nFrames_ = 0;
        bool released_ = false;
    };
};

//------AudioCapture------>

AudioCapture::AudioCapture(const std::wstring& deviceId, Audio::Format requestedFormat,
    boost::asio::io_context& ioContext) : ioContext_(ioContext) {
    //throw Audio::Error("AudioCapture::ctor");

    constexpr int REFTIMES_PER_SEC = 10'000'000;
    constexpr int REFTIMES_PER_MILLISEC = 10'000;

    HRESULT hr;
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    throwOnError(hr, Audio::Location::CAPTURE_COINITIALIZE);
    coUninitializer_ = std::make_unique<CoUninitializer>();

    CComPtr<IMMDeviceEnumerator> enumerator;
    hr = enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    throwOnError(hr, Audio::Location::CAPTURE_COCREATEINSTANCE);

    CComPtr<IMMDevice> device;
    hr = enumerator->GetDevice(deviceId.c_str(), &device);
    throwOnError(hr, Audio::Location::CAPTURE_GETDEVICE);

    hr = device->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(&meterInfo_));
    throwOnError(hr, Audio::Location::CAPTURE_ACTIVATE_METERINFO);

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(&audioClient_));
    throwOnError(hr, Audio::Location::CAPTURE_ACTIVATE_AUDIOCLIENT);

    CComPtr<IMMEndpoint> endpoint;
    hr = device->QueryInterface(IID_PPV_ARGS(&endpoint));
    throwOnError(hr, Audio::Location::CAPTURE_QUERY_ENDPOINT);

    EDataFlow deviceFlow;
    hr = endpoint->GetDataFlow(&deviceFlow);
    throwOnError(hr, Audio::Location::CAPTURE_ENDPOINT_GETDATAFLOW);

// Figure out capture wave format
    requestedWaveFormat_ = WaveFormat(createWaveFormat(requestedFormat));
    if (nullptr == requestedWaveFormat_) {
        throwOnError(E_OUTOFMEMORY, Audio::Location::CAPTURE_MEMALLOC);
    }
    WAVEFORMATEXTENSIBLE* pSupportedFormat = nullptr;
    hr = audioClient_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
        reinterpret_cast<WAVEFORMATEX*>(requestedWaveFormat_.get()),
        reinterpret_cast<WAVEFORMATEX**>(&pSupportedFormat));
    supportedWaveFormat_.reset(pSupportedFormat);
    pSupportedFormat = nullptr;

    switch (hr) {
    // requested format is supported, supportedWaveFormat_ is NULL
    case S_OK:
        resampleRequired_ = false;
        supportedWaveFormat_.reset(
            reinterpret_cast<PWAVEFORMATEXTENSIBLE>(CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE))));
        if (nullptr == supportedWaveFormat_) {
            throwOnError(E_OUTOFMEMORY, Audio::Location::CAPTURE_MEMALLOC);
        }
        *supportedWaveFormat_ = *requestedWaveFormat_;
        break;

    // requested format is not supported, supportedWaveFormat_ was initialized
    case S_FALSE:
        resampleRequired_ = true;
        break;

    // requested format is not supported, supportedWaveFormat_ is NULL
    case AUDCLNT_E_UNSUPPORTED_FORMAT:
        resampleRequired_ = true;
        hr = audioClient_->GetMixFormat(reinterpret_cast<WAVEFORMATEX**>(&pSupportedFormat));
        throwOnError(hr, Audio::Location::CAPTURE_AC_GETMIXFORMAT);
        supportedWaveFormat_.reset(pSupportedFormat);
        pSupportedFormat = nullptr;
        break;
    
    default:
        throwOnError(hr, Audio::Location::CAPTURE_AC_ISFORMATSUPPORTED);
        break;
    }

// Initialize AudioClient
    DWORD streamFlags = (eRender == deviceFlow) ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;
    //REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    REFERENCE_TIME hnsRequestedDuration = 0;        // Requested buffer duration
    hr = audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        streamFlags,
        hnsRequestedDuration,
        0,
        reinterpret_cast<WAVEFORMATEX*>(supportedWaveFormat_.get()),
        nullptr);
    throwOnError(hr, deviceFlow == eCapture ?
        Audio::Location::CAPTURE_AC_INITIALIZE_CAPTURE :
        Audio::Location::CAPTURE_AC_INITIALIZE_RENDER);

// Get the size of the allocated buffer.
    UINT32 bufferFrameCount;
    hr = audioClient_->GetBufferSize(&bufferFrameCount);
    throwOnError(hr, Audio::Location::CAPTURE_AC_GETBUFFERSIZE);

    hr = audioClient_->GetService(IID_PPV_ARGS(&captureClient_));
    throwOnError(hr, Audio::Location::CAPTURE_AC_GETSERVICE);

// Calculate the actual duration of the allocated buffer.
    REFERENCE_TIME hnsActualDuration = static_cast<REFERENCE_TIME>(
        static_cast<double>(REFTIMES_PER_SEC) * bufferFrameCount /
        supportedWaveFormat_->Format.nSamplesPerSec);
    bufferDuration_ = BufferDuration(hnsActualDuration);
}

AudioCapture::~AudioCapture() {
}

CaptureCoroutine AudioCapture::capture() {
    //throw Audio::Error("AudioCapture::capture start");
    HRESULT hr;

    hr = audioClient_->Start();
    Audio::throwOnError(hr, Audio::Location::CAPTURE_AC_START);
    std::unique_ptr<IAudioClient, void(*)(IAudioClient*)> audioClientStop(audioClient_,
        [](IAudioClient* client_) {
          HRESULT hr = client_->Stop();
          if (FAILED(hr))     // Shouldn't throw from a dtor, so just show the error.
              Util::showError(Audio::audioErrorText(hr, Audio::Location::CAPTURE_AC_STOP));
        });

    const auto timerPeriod = bufferDuration_ / 2;
    auto uncompensatedSilenceDuration = BufferDuration::zero();
    unsigned int silenceCompensationFrames = 0;
    const std::chrono::duration<double> periodSeconds = timerPeriod;
    const unsigned int silenceBufferSize =
        std::lround(supportedWaveFormat_->Format.nSamplesPerSec * periodSeconds.count()) *
        supportedWaveFormat_->Format.nBlockAlign;
    std::vector<unsigned char> silenceBuffer( silenceBufferSize );
    AwaitableTimer timer(ioContext_, timerPeriod);
    for (;;) {
        co_await timer;

        UINT32 packetLength = 0;
        hr = captureClient_->GetNextPacketSize(&packetLength);
        Audio::throwOnError(hr, Audio::Location::CAPTURE_ACC_GETNEXTPACKETSIZE);

        // Silence compensation
        if (packetLength == 0) {
            uncompensatedSilenceDuration += timerPeriod;
            if (uncompensatedSilenceDuration >= bufferDuration_) {
                co_yield{ reinterpret_cast<char*>(silenceBuffer.data()), silenceBufferSize };
                uncompensatedSilenceDuration -= timerPeriod;
            }
        } else {
            uncompensatedSilenceDuration = BufferDuration::zero();
        }

        while (packetLength != 0) {
            BYTE* pData;
            UINT32 numFramesAvailable;
            DWORD flags;
            hr = captureClient_->GetBuffer(
                &pData,
                &numFramesAvailable,
                &flags, nullptr, nullptr);
            Audio::throwOnError(hr, Audio::Location::CAPTURE_ACC_GETBUFFER);
            BufferReleaser bufferReleaser (captureClient_, numFramesAvailable);

            //if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {}
            const auto size = numFramesAvailable * supportedWaveFormat_->Format.nBlockAlign;
            co_yield{ reinterpret_cast<char*>(pData), size };
            //throw Audio::Error("AudioCapture::capture loop");

            hr = bufferReleaser.release();
            Audio::throwOnError(hr, Audio::Location::CAPTURE_ACC_RELEASEBUFFER);

            hr = captureClient_->GetNextPacketSize(&packetLength);
            Audio::throwOnError(hr, Audio::Location::CAPTURE_ACC_GETNEXTPACKETSIZE);
        }
    }
}

bool AudioCapture::resampleRequired() const {
    return resampleRequired_;
}

WAVEFORMATEXTENSIBLE* AudioCapture::requestedWaveFormat() const {
    return requestedWaveFormat_.get();
}

WAVEFORMATEXTENSIBLE* AudioCapture::capturedWaveFormat() const {
    return supportedWaveFormat_.get();
}

float AudioCapture::getPeakValue() const {
    float result;
    const HRESULT hr = meterInfo_->GetPeakValue(&result);
    if (hr != S_OK) {
        return -1;
    }
    return result;
}
