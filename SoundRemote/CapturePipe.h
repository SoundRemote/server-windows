#pragma once

#include <atomic>
#include <forward_list>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/asio/streambuf.hpp>

#include "AudioUtil.h"
#include "NetDefines.h"

class AudioCapture;
class AudioResampler;
class EncoderOpus;
class Server;
struct PipeCoroutine;
struct ClientInfo;

class CapturePipe {
public:
	CapturePipe(const std::wstring& deviceId, std::shared_ptr<Server> server,
		boost::asio::io_context& io_context, bool muted = false);
	~CapturePipe();
	void start();
	float getPeakValue() const;
	void setMuted(bool muted);
	void onClientsUpdate(std::forward_list<ClientInfo> clients);
private:
	// Capturing coroutine
	PipeCoroutine process();
	// Destroys the capturing coroutine
	void stop();
	void process(std::span<char> pcmAudio, std::shared_ptr<Server> server);
	bool haveClients() const;

	boost::asio::io_context& io_context_;
	std::unique_ptr<PipeCoroutine> pipeCoro_;
	std::unique_ptr<AudioCapture> audioCapture_;
	std::unique_ptr<AudioResampler> audioResampler_;
	std::weak_ptr<Server> server_;
	const std::wstring device_;
	boost::asio::streambuf pcmAudioBuffer_;
	std::atomic_bool muted_ = false;
	std::unordered_map<Audio::Compression, std::unique_ptr<EncoderOpus>> encoders_;
	int opusInputSize_;
	static Net::Packet::SequenceNumberType audioSequenceNumber_;
};
