#pragma once

#include <memory>

#include "AudioUtil.h"

struct OpusEncoder;

class EncoderOpus {
public:
	EncoderOpus(Audio::Compression compression, Audio::Opus::SampleRate sampleRate,
		Audio::Opus::Channels channels);

	/// <summary>
	/// Encodes a frame of PCM audio.
	/// </summary>
	/// <param name="pcmAudio">
	/// - input signal in 16 bit signed int format.
	/// Use <c>EncoderOpus::getInputSize()</c> to get the required size.
	/// </param>
	/// <param name="encodedPacket">
	/// - buffer to contain the encoded packet.
	/// Use <c>Audio::Opus::maxPacketSize</c> to get the recommended buffer size.
	/// </param>
	/// <returns>
	/// Encoded packet length in bytes. If the return value is 0 encoded packet does not need to be
	/// transmitted (DTX).
	/// </returns>
	int encode(const char* pcmAudio, char* encodedPacket);
	static int getFrameSize(Audio::Opus::SampleRate sampleRate);
	static int getInputSize(int frameSize, Audio::Opus::Channels channels);
private:
	struct EncoderDeleter {
		void operator()(OpusEncoder* enc) const;
	};
	using Encoder = std::unique_ptr<OpusEncoder, EncoderDeleter>;

	Encoder encoder_;
	// Number of samples per frame
	int frameSize_;

	EncoderOpus(const EncoderOpus&) = delete;
	EncoderOpus& operator= (const EncoderOpus&) = delete;
};
