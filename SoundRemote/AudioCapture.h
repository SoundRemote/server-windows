#pragma once

#include <atlbase.h>
#include <mmreg.h>

#include <chrono>
#include <coroutine>
#include <memory>
#include <string>
#include <span>

#include <boost/asio/io_context.hpp>

#include "AudioUtil.h"

struct IAudioCaptureClient;
struct IAudioClient;
struct IAudioMeterInformation;

struct CaptureCoroutine;

class AudioCapture {
public:
	/// <summary>
	/// Creates AudioCapture.
	/// </summary>
	/// <param name="deviceId">Device id string.</param>
	/// <param name="requestedFormat">Requested audio format.</param>
	/// <param name="ioContext"><c>boost::asio::io_context</c> to use by an internal timer.</param>
	AudioCapture(const std::wstring& deviceId, Audio::Format requestedFormat,
        boost::asio::io_context& ioContext);
    ~AudioCapture();

    /// <summary>
    /// Awaitable capture coroutine.
    /// </summary>
    CaptureCoroutine capture();

    /// <summary>
    /// Is resampling of the captured audio required, i.e. requested audio format is not supported
    /// by the audio device.
    /// </summary>
    /// <returns>True if resampling required, false otherwise.</returns>
    bool resampleRequired() const;

    /// <summary>
    /// Gets the requested wave format represented as a <c>WAVEFORMATEXTENSIBLE</c> structure.
    /// </summary>
    /// <returns>A pointer to a <c>WAVEFORMATEXTENSIBLE</c> structure.
    /// Pointer remains valid until the <c>AudioCapture</c> object exists.</returns>
    WAVEFORMATEXTENSIBLE* requestedWaveFormat() const;

    /// <summary>
    /// Gets wave format that is actually supported by the audio device.
    /// Wave format represented as a <c>WAVEFORMATEXTENSIBLE</c> structure.
    /// </summary>
    /// <returns>A pointer to a <c>WAVEFORMATEXTENSIBLE</c> structure.
    /// Pointer remains valid until the <c>AudioCapture</c> object exists.</returns>
    WAVEFORMATEXTENSIBLE* capturedWaveFormat() const;

    /// <summary>
    /// Gets the peak sample value for the captured audio.
    /// </summary>
    /// <returns>Peak value as a number in range from 0.0 to 1.0. Returns -1 on fail.</returns>
    float getPeakValue() const;
private:
    using WaveFormat = std::unique_ptr<WAVEFORMATEXTENSIBLE,
        Audio::CoDeleter<WAVEFORMATEXTENSIBLE>>;
    // Hundreds nanoseconds
    using BufferDuration = std::chrono::duration<long, std::ratio_multiply<std::hecto, std::nano>>;
    
    boost::asio::io_context& ioContext_;
    bool resampleRequired_ = false;
    BufferDuration bufferDuration_ = BufferDuration::zero();
    std::unique_ptr<Audio::CoUninitializer> coUninitializer_;

    WaveFormat requestedWaveFormat_;
    WaveFormat supportedWaveFormat_;
    CComPtr<IAudioClient> audioClient_;
    CComPtr<IAudioCaptureClient> captureClient_;
    CComPtr<IAudioMeterInformation> meterInfo_;
};

struct [[nodiscard]] CaptureCoroutine {
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::span<char> pcmAudio;
        std::coroutine_handle<> awaiting_coroutine_;
        //std::exception_ptr exception_;

        CaptureCoroutine get_return_object() {
            return { Handle::from_promise(*this) };
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {
            auto exception = std::current_exception();
            std::rethrow_exception(exception);
        }
        void return_void() noexcept {}
        auto yield_value(const std::span<char>& data) {
            pcmAudio = data;

            struct transfer_awaitable {
                std::coroutine_handle<> awaiting_coroutine;

                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
                    return awaiting_coroutine ? awaiting_coroutine : std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return transfer_awaitable{ awaiting_coroutine_ };
        }
    };

    Handle h_;
    CaptureCoroutine(Handle h) :h_{ h } {}
    ~CaptureCoroutine() { h_.destroy(); }
    operator Handle() const { return h_; }

    //explicit operator bool() {
    //    return !h_.done();
    //}

    template<typename PromiseType = void>
    struct AudioAwaiter {
        Handle captureCoro;
        bool await_ready() {
            return captureCoro.promise().pcmAudio.size() > 0;
        }
        void await_suspend(std::coroutine_handle<PromiseType> awaiting) {
            captureCoro.promise().awaiting_coroutine_ = awaiting;
        }
        auto await_resume() {
            auto result = captureCoro.promise().pcmAudio;
            captureCoro.promise().pcmAudio = {};
            return result;
        }
    };

    auto operator co_await() {
        return AudioAwaiter{ h_ };
    }
};
