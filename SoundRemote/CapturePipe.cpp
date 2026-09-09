#include "CapturePipe.h"

#include <coroutine>
#include <unordered_set>

#include "AudioCapture.h"
#include "AudioResampler.h"
#include "AudioUtil.h"
#include "Clients.h"
#include "EncoderOpus.h"
#include "Server.h"
#include "Util.h"

struct [[nodiscard]] PipeCoroutine {
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;

    struct promise_type {
        PipeCoroutine get_return_object() {
            return Handle::from_promise(*this);
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {
            auto exception = std::current_exception();
            std::rethrow_exception(exception);
        }
    };

    Handle h_;
    PipeCoroutine(Handle h) :h_{ h } {}
    ~PipeCoroutine() { /*h_.destroy();*/ }
    operator Handle() const { return h_; }
    //PipeCoroutine(const PipeCoroutine&) = delete;
};

Net::Packet::SequenceNumberType CapturePipe::audioSequenceNumber_ = 1u;

CapturePipe::CapturePipe(const std::wstring& deviceId, std::shared_ptr<Server> server,
    boost::asio::io_context& ioContext, bool muted)
    : device_(deviceId), server_(server), io_context_(ioContext), muted_(muted) {
    //throw std::runtime_error("CapturePipe::ctr");
    Audio::Format requestedFormat;
    audioCapture_ = std::make_unique<AudioCapture>(deviceId, requestedFormat, ioContext);
    if (audioCapture_->resampleRequired()) {
        auto capturedWaveFormat = audioCapture_->capturedWaveFormat();
        auto requestedWaveFormat = audioCapture_->requestedWaveFormat();
        audioResampler_ = std::make_unique<AudioResampler>(capturedWaveFormat, requestedWaveFormat,
            pcmAudioBuffer_);
    }
    opusInputSize_ = EncoderOpus::getInputSize(
        EncoderOpus::getFrameSize(Audio::Opus::SampleRate::khz_48), Audio::Opus::Channels::stereo);
}

CapturePipe::~CapturePipe() {
    stop();
}

void CapturePipe::start() {
    pipeCoro_ = std::make_unique<PipeCoroutine>(process());
}

float CapturePipe::getPeakValue() const {
    return audioCapture_->getPeakValue();
}

void CapturePipe::setMuted(bool muted) {
    muted_ = muted;
}

void CapturePipe::onClientsUpdate(std::forward_list<ClientInfo> clients) {
    std::unordered_set<Audio::Compression> newCompressions;
    std::unordered_set<Audio::Compression> existingCompressions;
    for (auto&& client : clients) {
        if (encoders_.contains(client.compression)) {
            existingCompressions.insert(client.compression);
        } else {
            newCompressions.insert(client.compression);
        }
    }

    if (newCompressions.empty() && existingCompressions.size() == encoders_.size()) {
        return;
    }

    std::erase_if(encoders_, [&](const auto& item) {
        return !existingCompressions.contains(item.first);
    });
    for (auto&& it: newCompressions) {
        if (it == Audio::Compression::none) {
            encoders_[it] = std::unique_ptr<EncoderOpus>();
        } else {
            encoders_[it] = std::make_unique<EncoderOpus>(it, Audio::Opus::SampleRate::khz_48,
                Audio::Opus::Channels::stereo);
        }
    }
}

PipeCoroutine CapturePipe::process() {
    //throw std::runtime_error("CapturePipe::process start");
    auto audioCapture = audioCapture_->capture();
    for (;;) {
        auto capturedAudio = co_await audioCapture;
        if (!muted_) {
            auto server = server_.lock();
            if (server && haveClients()) {
                process(capturedAudio, server);
            }
        }
        //throw std::runtime_error("CapturePipe::process loop");
        audioCapture.h_();
    }
}

void CapturePipe::stop() {
    if (pipeCoro_) {
        pipeCoro_->h_.destroy();
        pipeCoro_.reset();
    }
}

bool CapturePipe::haveClients() const {
    return !encoders_.empty();
}

void CapturePipe::process(std::span<char> pcmAudio, std::shared_ptr<Server> server) {
    if (audioCapture_->resampleRequired()) {
        audioResampler_->resample(pcmAudio);
    } else {
        pcmAudioBuffer_.sputn(pcmAudio.data(), pcmAudio.size());
    }
    while (pcmAudioBuffer_.data().size() >= opusInputSize_) {
        for (auto&& [compression, encoder] : encoders_) {
            if (compression == Audio::Compression::none) {
                std::vector<char> uncompressed(opusInputSize_);
                std::copy_n(
                    reinterpret_cast<const char *>(pcmAudioBuffer_.data().data()),
                    opusInputSize_,
                    uncompressed.data()
                );
                server->sendAudio(compression, audioSequenceNumber_,  uncompressed);
            } else {
                std::vector<char> encodedPacket(Audio::Opus::maxPacketSize);
                const auto packetSize = encoder->encode(
                    static_cast<const char*>(pcmAudioBuffer_.data().data()),
                    encodedPacket.data()
                );
                encodedPacket.resize(packetSize);
                if (packetSize > 0) {
                    server->sendAudio(compression, audioSequenceNumber_, encodedPacket);
                }
            }
        }
        ++audioSequenceNumber_;
        pcmAudioBuffer_.consume(opusInputSize_);
    }
}
