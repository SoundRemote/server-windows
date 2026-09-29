#include "AudioResampler.h"

#include <initguid.h>   // MUST INCLUDE BEFORE <mfapi.h>
#include <mfapi.h>
#include <Mferror.h>
#include <mftransform.h>
#include <wmcodecdsp.h>

#include "AudioUtil.h"

AudioResampler::AudioResampler(_In_ const WAVEFORMATEXTENSIBLE* inputFormat,
    _In_ const WAVEFORMATEXTENSIBLE* outputFormat, _In_ boost::asio::streambuf& outBuffer)
    : outBuffer_(outBuffer) {
    HRESULT hr;
    CComPtr<IUnknown> transformUnk;
    hr = transformUnk.CoCreateInstance(__uuidof(CResamplerMediaObject));
    throwOnError(hr, Audio::Location::RESAMPLER_COCREATEINSTANCE);
// Get resampler MFT
    hr = transformUnk->QueryInterface(IID_PPV_ARGS(&transform_));
    throwOnError(hr, Audio::Location::RESAMPLER_QUERY_TRANSFORM);
// Set the best conversion quality (1-60)
    CComPtr<IWMResamplerProps> resamplerProps;
    hr = transformUnk->QueryInterface(IID_PPV_ARGS(&resamplerProps));
    throwOnError(hr, Audio::Location::RESAMPLER_QUERY_PROPS);
    hr = resamplerProps->SetHalfFilterLength(60);
    throwOnError(hr, Audio::Location::RESAMPLER_PROPS_SETHALFFILTERLENGTH);
// Create and set input media type
    CComPtr<IMFMediaType> inMediaType;
    hr = MFCreateMediaType(&inMediaType);
    throwOnError(hr, Audio::Location::RESAMPLER_CREATEMEDIATYPE_INPUT);
    hr = MFInitMediaTypeFromWaveFormatEx(inMediaType,
        reinterpret_cast<const WAVEFORMATEX*>(inputFormat), sizeof(WAVEFORMATEXTENSIBLE));
    throwOnError(hr, Audio::Location::RESAMPLER_INITMEDIATYPE_INPUT);
    hr = transform_->SetInputType(0, inMediaType, 0);
    throwOnError(hr, Audio::Location::RESAMPLER_TRANSFORM_SETINPUTTYPE);
// Create and set output media type
    CComPtr<IMFMediaType> outMediaType;
    hr = MFCreateMediaType(&outMediaType);
    throwOnError(hr, Audio::Location::RESAMPLER_CREATEMEDIATYPE_OUTPUT);
    hr = MFInitMediaTypeFromWaveFormatEx(outMediaType,
        reinterpret_cast<const WAVEFORMATEX*>(outputFormat), sizeof(WAVEFORMATEXTENSIBLE));
    throwOnError(hr, Audio::Location::RESAMPLER_INITMEDIATYPE_OUTPUT);
    transform_->SetOutputType(0, outMediaType, 0);
    throwOnError(hr, Audio::Location::RESAMPLER_TRANSFORM_SETOUTPUTTYPE);
// Start transform stream
    hr = transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    throwOnError(hr, Audio::Location::RESAMPLER_TRANSFORM_MESSAGE_BEGIN);
    hr = transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    throwOnError(hr, Audio::Location::RESAMPLER_TRANSFORM_MESSAGE_START);
    outBufferSizeMultiplier_ = 2 * static_cast<double>(outputFormat->Format.nAvgBytesPerSec) /
        static_cast<double>(inputFormat->Format.nAvgBytesPerSec);
}

AudioResampler::~AudioResampler() {
    if (transform_) {
        transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, NULL);
        transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL);
    }
}

void AudioResampler::resample(_In_ const std::span<char>& pcmAudio) {
    HRESULT hr;
// Create MediaBuffer with incoming data
    CComPtr<IMFMediaBuffer> inSampleBuffer;
    hr = MFCreateMemoryBuffer(static_cast<DWORD>(pcmAudio.size_bytes()), &inSampleBuffer);
    throwOnError(hr, Audio::Location::RESAMPLER_CREATE_INPUT_BUFFER);
    BYTE* inBytes = nullptr;
    hr = inSampleBuffer->Lock(&inBytes, nullptr, nullptr);
    throwOnError(hr, Audio::Location::RESAMPLER_INPUT_BUFFER_LOCK);
    memcpy(inBytes, pcmAudio.data(), pcmAudio.size_bytes());
    hr = inSampleBuffer->Unlock();
    throwOnError(hr, Audio::Location::RESAMPLER_INPUT_BUFFER_UNLOCK);
    inBytes = nullptr;
    hr = inSampleBuffer->SetCurrentLength(static_cast<DWORD>(pcmAudio.size_bytes()));
    throwOnError(hr, Audio::Location::RESAMPLER_INPUT_BUFFER_SETLENGTH);
// Create Sample from that MediaBuffer
    CComPtr<IMFSample> inSample;
    hr = MFCreateSample(&inSample);
    throwOnError(hr, Audio::Location::RESAMPLER_CREATE_INPUT_SAMPLE);
    hr = inSample->AddBuffer(inSampleBuffer);
    throwOnError(hr, Audio::Location::RESAMPLER_INPUT_SAMPLE_ADDBUFFER);
// Process the Sample
    hr = transform_->ProcessInput(0, inSample, 0);
    throwOnError(hr, Audio::Location::RESAMPLER_TRANSFORM_PROCESSINPUT);
// Create output buffer
    MFT_OUTPUT_STREAM_INFO outStreamInfo;
    hr = transform_->GetOutputStreamInfo(0, &outStreamInfo);
    throwOnError(hr, Audio::Location::RESAMPLER_TRANSFORM_GETOUTPUTSTREAMINFO);
    MFT_OUTPUT_DATA_BUFFER outputDataBuffer { 0, nullptr, 0, nullptr };
    CComPtr<IMFMediaBuffer> outSampleBuffer;
    CComPtr<IMFSample> outSample;
    // If the MFT does not set one of this flags, the client must allocate the samples for the
    // output stream.
    const bool outSampleProvided = 0 != (outStreamInfo.dwFlags &
        (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES));
    if (!outSampleProvided) {
        const DWORD outSampleBufferSize =
            static_cast<DWORD>(outBufferSizeMultiplier_ * pcmAudio.size_bytes());
        hr = MFCreateMemoryBuffer(outSampleBufferSize, &outSampleBuffer);
        throwOnError(hr, Audio::Location::RESAMPLER_CREATE_OUTPUT_BUFFER);
        hr = MFCreateSample(&outSample);
        throwOnError(hr, Audio::Location::RESAMPLER_CREATE_OUTPUT_SAMPLE);
        hr = outSample->AddBuffer(outSampleBuffer);
        throwOnError(hr, Audio::Location::RESAMPLER_OUTPUT_SAMPLE_ADDBUFFER);
        outputDataBuffer.pSample = outSample;
    }
    DWORD totalBytesResampled = 0;
    for (;;) {
        DWORD dwStatus = 0;
        hr = transform_->ProcessOutput(0, 1, &outputDataBuffer, &dwStatus);
        // If the sample was allocated by the MFT, attach it to a CComPtr so it gets released
        // automatically.
        if (outSampleProvided) {
            outSample.Attach(outputDataBuffer.pSample);
        }
        // Release the events that the MFT might allocate.
        SAFE_RELEASE(outputDataBuffer.pEvents);
        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            break;
        }
        throwOnError(hr, Audio::Location::RESAMPLER_TRANSFORM_PROCESSOUTPUT);
        CComPtr<IMFMediaBuffer> processedDataBuffer;
        // The caller must release the interface.
        hr = outSample->ConvertToContiguousBuffer(&processedDataBuffer);
        throwOnError(hr, Audio::Location::RESAMPLER_OUTPUT_SAMPLE_CONVERT);
        DWORD cbBytes = 0;
        hr = processedDataBuffer->GetCurrentLength(&cbBytes);
        throwOnError(hr, Audio::Location::RESAMPLER_PROCESSED_BUFFER_GETCURRENTLENGTH);
        BYTE* outBytes = nullptr;
        hr = processedDataBuffer->Lock(&outBytes, nullptr, nullptr);
        throwOnError(hr, Audio::Location::RESAMPLER_PROCESSED_BUFFER_LOCK);
        outBuffer_.sputn(reinterpret_cast<char*>(outBytes), cbBytes);
        totalBytesResampled += cbBytes;
        hr = processedDataBuffer->Unlock();
        throwOnError(hr, Audio::Location::RESAMPLER_PROCESSED_BUFFER_UNLOCK);
    };
}
