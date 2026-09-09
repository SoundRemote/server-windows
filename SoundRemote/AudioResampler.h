#pragma once

#include <atlbase.h>
#include <mmreg.h>
#include <sal.h>

#include <span>

#include <boost/asio/streambuf.hpp>

struct IMFTransform;

class AudioResampler {
public:
	AudioResampler(_In_ const WAVEFORMATEXTENSIBLE* inputFormat,
		_In_ const WAVEFORMATEXTENSIBLE* outputFormat, _In_ boost::asio::streambuf& outBuffer);
	~AudioResampler();
    void resample(_In_ const std::span<char>& pcmAudio);
private:
	CComPtr<IMFTransform> transform_;
	boost::asio::streambuf& outBuffer_;
	double outBufferSizeMultiplier_ = 0.0;
};
