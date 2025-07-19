#pragma once

#include <chrono>
#include <forward_list>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "AudioUtil.h"
#include "NetDefines.h"

struct ClientInfo;

class Clients {
	using TimePoint = std::chrono::steady_clock::time_point;
	using ClientsUpdateCallback = std::function<void(std::forward_list<ClientInfo>)>;
public:
	Clients(int timeoutSeconds = 5);
	void add(const Net::Address& address, Audio::Compression compression);
	void setCompression(const Net::Address& address, Audio::Compression compression);
	void keep(const Net::Address& address);
	void remove(const Net::Address& address);
	void removeAll();
	void addClientsListener(ClientsUpdateCallback listener);
	size_t removeClientsListener(ClientsUpdateCallback listener);
	void maintain();

private:
	class Client {
	public:
		Client(Audio::Compression compression);
		void updateLastContact();
		void setCompression(Audio::Compression compression);
		TimePoint lastContact() const;
		Audio::Compression compression() const;
	private:
		Audio::Compression compression_ = Audio::Compression::none;
		TimePoint lastContact_{};
	};
	
	void updateInfos();
	void notifyListeners() const;
	void updateAndNotify();

	const int timeoutSeconds_;
	std::unordered_map<Net::Address, std::unique_ptr<Client>> clients_;
	std::forward_list<ClientInfo> clientInfos_;
	std::shared_mutex clientsMutex_;
	// listeners are not synchronized, the list is modified on program start and on device change
	std::forward_list<ClientsUpdateCallback> clientsListeners_;
};

struct ClientInfo {
	Net::Address address;
	Audio::Compression compression;
	ClientInfo(Net::Address addr, Audio::Compression br) : address(addr), compression(br) {}
	friend bool operator==(const ClientInfo& lhs, const ClientInfo& rhs);
};
