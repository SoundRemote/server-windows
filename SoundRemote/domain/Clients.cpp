#include "Clients.h"

Clients::Clients(int timeoutSeconds) : timeoutSeconds_(timeoutSeconds) {}

void Clients::add(const Net::Address& address, Audio::Compression compression) {
	const std::unique_lock lock(clientsMutex_);
	if (clients_.contains(address)) {
		clients_[address]->updateLastContact();
		if (clients_[address]->compression() == compression) {
			return;
		}
		clients_[address]->setCompression(compression);
		updateAndNotify();
	} else {
		clients_[address] = std::make_unique<Client>(compression);		
		updateAndNotify();
	}
}

void Clients::setCompression(const Net::Address& address, Audio::Compression compression) {
	const std::unique_lock lock(clientsMutex_);
	if (!clients_.contains(address) || clients_[address]->compression() == compression) {
		return;
	}
	clients_[address]->setCompression(compression);
	updateAndNotify();
}

void Clients::keep(const Net::Address& address) {
	const std::unique_lock lock(clientsMutex_);
	if (!clients_.contains(address)) {
		return;
	}
	clients_[address]->updateLastContact();
}

void Clients::remove(const Net::Address& address) {
	const std::unique_lock lock(clientsMutex_);
	if (clients_.erase(address)) {
		updateAndNotify();
	}
}

void Clients::removeAll() {
	const std::unique_lock lock(clientsMutex_);
	if (clients_.empty()) {
		return;
	}
	clients_.clear();
	updateAndNotify();
}

void Clients::addClientsListener(ClientsUpdateCallback listener) {
	clientsListeners_.push_front(listener);
	listener(clientInfos_);
}

size_t Clients::removeClientsListener(ClientsUpdateCallback listener) {
	return clientsListeners_.remove_if([&](ClientsUpdateCallback f) {
		return f.target_type().name() == listener.target_type().name();
	});
}

void Clients::maintain() {
	const std::unique_lock lock(clientsMutex_);
	bool clientRemoved = false;
	const auto now = std::chrono::steady_clock::now();
	for (auto&& it = clients_.begin(); it != clients_.end();) {
		std::chrono::duration<float> elapsedSeconds = now - it->second->lastContact();
		if (elapsedSeconds.count() > timeoutSeconds_) {
			it = clients_.erase(it);
			clientRemoved = true;
		} else {
			++it;
		}
	}
	if (clientRemoved) {
		updateAndNotify();
	}
}

void Clients::updateInfos() {
	clientInfos_.clear();
	for (auto&& client : clients_) {
		clientInfos_.push_front({ client.first, client.second->compression() });
	}
}

void Clients::notifyListeners() const {
	for (auto&& listener : clientsListeners_) {
		listener(clientInfos_);
	}
}

void Clients::updateAndNotify() {
	updateInfos();
	notifyListeners();
}

// Client

Clients::Client::Client(Audio::Compression compression): compression_(compression) {
	updateLastContact();
}

void Clients::Client::updateLastContact() {
	lastContact_ = std::chrono::steady_clock::now();
}

void Clients::Client::setCompression(Audio::Compression compression) {
	compression_ = compression;
}

Clients::TimePoint Clients::Client::lastContact() const {
	return lastContact_;
}

Audio::Compression Clients::Client::compression() const {
	return compression_;
}

bool operator==(const ClientInfo& lhs, const ClientInfo& rhs) {
	return lhs.address == rhs.address &&
		lhs.compression == rhs.compression;
}