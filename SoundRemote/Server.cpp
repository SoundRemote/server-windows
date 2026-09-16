#include "Server.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "Clients.h"
#include "NetUtil.h"
#include "Util.h"

using boost::asio::ip::udp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using namespace std::chrono_literals;
using namespace std::placeholders;

Server::Server(int clientPort, int serverPort, boost::asio::io_context& ioContext,
    std::shared_ptr<Clients> clients) :
    clientPort_(clientPort),
    clients_(clients),
    socketSend_(ioContext, udp::v4()),
    socketReceive_(ioContext, udp::endpoint(udp::v4(), serverPort)),
    maintenanceTimer_(ioContext) {
    co_spawn(ioContext, receive(socketReceive_), detached);
    startMaintenanceTimer();
}

Server::~Server() {
    socketReceive_.shutdown(udp::socket::shutdown_receive);
    socketReceive_.close();
    socketSend_.shutdown(udp::socket::shutdown_send);
    socketSend_.close();
}

void Server::onClientsUpdate(std::forward_list<ClientInfo> clients) {
    std::unordered_map<Audio::Compression, std::forward_list<Net::Address>> newClients;
    for (auto&& client: clients) {
        if (!newClients.contains(client.compression)) {
            newClients[client.compression] = std::forward_list<Net::Address>();
        }
        newClients[client.compression].push_front(client.address);
    }
    clientsCache_ = std::move(newClients);
}

void Server::sendAudio(
    Audio::Compression compression,
    Net::Packet::SequenceNumberType sequenceNumber,
    std::vector<char> data
) {
    auto category = compression == Audio::Compression::none ?
        Net::Packet::Category::AudioDataUncompressed :
        Net::Packet::Category::AudioDataOpus;
    auto packet = std::make_shared<std::vector<char>>(
        Net::createAudioPacket(category, sequenceNumber, { data.data(), data.size() })
    );
    for (auto&& address : clientsCache_[compression]) {
        send(address, packet);
    }
}

void Server::sendDisconnectBlocking() {
    if (clientsCache_.empty()) { return; }
    auto destination = udp::endpoint(udp::v4(), clientPort_);
    auto packet = std::make_shared<std::vector<char>>(Net::createDisconnectPacket());
    for (auto&& [compression, addresses] : clientsCache_) {
        for (auto&& address : addresses) {
            destination.address(address);
            socketSend_.send_to(boost::asio::buffer(packet->data(), packet->size()), destination);
        }
    }
}

void Server::setKeystrokeCallback(KeystrokeCallback callback) {
    keystrokeCallback_ = callback;
}

awaitable<void> Server::receive(udp::socket& socket) {
    //throw std::exception("Server::receive");
    std::array<char, Net::inputPacketSize> datagram{};
    udp::endpoint sender;
    //Have to handle errors here or write custom completion handler for co_spawn()
    try {
        for (;;) {
            auto nBytes = co_await socket.async_receive_from(boost::asio::buffer(datagram), sender,
                use_awaitable);
            std::span receivedData = { datagram.data(), nBytes };
            auto category = Net::getPacketCategory(receivedData);
            switch (category) {
            case Net::Packet::Category::Connect:
                processConnect(sender.address(), receivedData);
                break;
            case Net::Packet::Category::Disconnect:
                processDisconnect(sender.address());
                break;
            case Net::Packet::Category::SetFormat:
                processSetFormat(sender.address(), receivedData);
                break;
            case Net::Packet::Category::Keystroke:
                processKeystroke(receivedData);
                break;
            case Net::Packet::Category::ClientKeepAlive:
                processKeepAlive(sender.address());
                break;
            default:
                break;
            }
        }
    }
    catch (const boost::system::system_error& se) {
        if (se.code() != boost::asio::error::operation_aborted) {
            Util::showError(Util::makeAppErrorText("Receive", se.what()));
            std::exit(EXIT_FAILURE);
        }
    }
    catch (const std::exception& e) {
        Util::showError(Util::makeAppErrorText("Receive", e.what()));
        std::exit(EXIT_FAILURE);
    }
    catch (...) {
        Util::showError("Receive: unknown error");
        std::exit(EXIT_FAILURE);
    }
}

void Server::processConnect(const Net::Address& address, const std::span<char>& packet) {
    const auto connectData = Net::getConnectData(packet);
    if (!connectData) { return; }
    auto compression = Net::compressionFromNetworkValue(connectData->compression);
    if (!compression) { return; }
    clients_->add(address, *compression);

    send(address, std::make_shared<std::vector<char>>(
        Net::createAckConnectPacket(connectData->requestId)
    ));
}

void Server::processDisconnect(const Net::Address& address) {
    clients_->remove(address);
}

void Server::processSetFormat(const Net::Address& address, const std::span<char>& packet) {
    const auto setFormatData = Net::getSetFormatData(packet);
    if (!setFormatData) { return; }
    auto compression = Net::compressionFromNetworkValue(setFormatData->compression);
    if (!compression) { return; }
    clients_->setCompression(address, *compression);

    send(address, std::make_shared<std::vector<char>>(
        Net::createAckSetFormatPacket(setFormatData->requestId)
    ));
}

void Server::processKeystroke(const std::span<char>& packet) const {
    const std::optional<Keystroke> keystroke = Net::getKeystroke(packet);
    if (!keystroke) { return; }
    keystroke->emulate();
    if (keystrokeCallback_) {
        keystrokeCallback_(*keystroke);
    }
}

void Server::processKeepAlive(const Net::Address& address) const {
    clients_->keep(address);
}

void Server::send(const Net::Address& address, const std::shared_ptr<std::vector<char>> packet) {
    auto destination = udp::endpoint(address, clientPort_);
    destination.address(address);
    socketSend_.async_send_to(boost::asio::buffer(packet->data(), packet->size()), destination,
        std::bind(&Server::handleSend, this, packet, _1, _2));
}

// std::shared_ptr with the packet is passed to keep data alive until the handler call
void Server::handleSend(const std::shared_ptr<std::vector<char>> packet,
    const boost::system::error_code& ec, std::size_t bytes) {
    if (ec) {
        throw std::runtime_error(Util::makeAppErrorText("Server send", ec.what()));
    }
}

void Server::startMaintenanceTimer() {
    maintenanceTimer_.expires_after(1s);
    maintenanceTimer_.async_wait(std::bind(&Server::maintain, this, std::placeholders::_1));
}

void Server::maintain(boost::system::error_code ec) {
    if (ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        } else {
            throw std::runtime_error(Util::makeAppErrorText("Timer maintain", ec.what()));
        }
    }
    keepalive();
    clients_->maintain();

    startMaintenanceTimer();
}

void Server::keepalive() {
    if (clientsCache_.empty()) { return; }
    auto packet = std::make_shared<std::vector<char>>(Net::createKeepAlivePacket());
    for (auto&& [compression, addresses] : clientsCache_) {
        for (auto&& address : addresses) {
            send(address, packet);
        }
    }
}
