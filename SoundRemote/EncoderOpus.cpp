#include "EncoderOpus.h"

#include <set>
#include <stdexcept>

#include <opus/opus.h>

#include "Util.h"

EncoderOpus::EncoderOpus(Audio::Compression compression, Audio::Opus::SampleRate sampleRate,
    Audio::Opus::Channels channels) {
    frameSize_ = getFrameSize(sampleRate);

    int error{};
    auto opusEncoder = opus_encoder_create(static_cast<int>(sampleRate), static_cast<int>(channels),
        OPUS_APPLICATION_AUDIO, &error);
    if (OPUS_OK != error) {
        Audio::processError(error, Audio::Location::ENCODER_CREATE);
    }
    encoder_ = Encoder(opusEncoder, EncoderDeleter());
    auto ret = opus_encoder_ctl(encoder_.get(), OPUS_SET_BITRATE(static_cast<int>(compression)));
    if (ret != OPUS_OK) {
        Audio::processError(ret, Audio::Location::ENCODER_SET_BITRATE);
    };
}

int EncoderOpus::encode(const char* pcmAudio, char* encodedPacket) {
    const opus_int32 encodeResult = opus_encode(encoder_.get(),
        reinterpret_cast<const opus_int16*>(pcmAudio), frameSize_,
        reinterpret_cast<unsigned char*>(encodedPacket), Audio::Opus::maxPacketSize);
    // If DTX is on and the return value is 2 bytes or less, then the packet does not need to be
    // transmitted.
    if (encodeResult >= 0 && encodeResult <= 2) {
        return 0;
    }
    if (encodeResult < 0 || encodeResult > Audio::Opus::maxPacketSize) {
        Audio::processError(encodeResult, Audio::Location::ENCODER_ENCODE);
    }
    return encodeResult;
}

int EncoderOpus::getFrameSize(Audio::Opus::SampleRate sampleRate) {
    return Audio::Opus::frameLength * static_cast<int>(sampleRate) / 1000;
}

int EncoderOpus::getInputSize(int frameSize, Audio::Opus::Channels channels) {
    return frameSize * static_cast<int>(channels) * 2;   // 2 is sample size in bytes
}

//---EncoderDeleter---

void EncoderOpus::EncoderDeleter::operator()(OpusEncoder* enc) const {
    opus_encoder_destroy(enc);
}
