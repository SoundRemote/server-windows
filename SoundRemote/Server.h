#pragma once

#include "keystroke.h"

class Settings;

class Server {
	using ClientsUpdateCallback = std::function<void(std::forward_list<std::string>)>;
	using KeystrokeCallback = std::function<void(const Keystroke& keystroke)>;
	using address_t = boost::asio::ip::address;
public:
	Server(boost::asio::io_context& ioContext, std::shared_ptr<Settings> settings);
	~Server();
	void sendOpusPacket(std::span<unsigned char> data);
	void setClientListCallback(ClientsUpdateCallback callback);
	void setKeystrokeCallback(KeystrokeCallback callback);
	bool hasClients() const;
private:
	class ClientList {
	public:
		ClientList(int clientTimeoutSeconds = 5);
		void keepClient(address_t clientAddress);
		void setClientListCallback(ClientsUpdateCallback callback);
		void maintain();
		std::forward_list<address_t> clients() const;
		bool empty() const;
	private:
		void onUpdate() const;

		std::unordered_map<address_t, std::chrono::steady_clock::time_point> clients_;
		ClientsUpdateCallback updateCallback_;
		const int timeoutSeconds_;
	};
private:
	boost::asio::awaitable<void> receive(boost::asio::ip::udp::socket& socket);
	/// <summary>
	/// Parses received data packet.
	/// </summary>
	/// <param name="packet">Packet contents.</param>
	/// <returns>True if data recognized and parsed successfully. False otherwise.</returns>
	bool parsePacket(const std::span<unsigned char> packet) const;
	void send(std::shared_ptr<std::vector<unsigned char>> packet);
	void sendKeepAlive();
	void startMaintenanceTimer();
	void maintain(boost::system::error_code ec);

	boost::asio::ip::udp::socket socketSend_;
	std::unique_ptr<boost::asio::ip::udp::socket> socketReceive_;
	std::unique_ptr<ClientList> clientList_;
	boost::asio::steady_timer maintainenanceTimer_;
	std::shared_ptr<Settings> settings_;
	KeystrokeCallback keystrokeCallback_;
};
