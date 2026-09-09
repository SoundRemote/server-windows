#include <barrier>
#include <forward_list>
#include <functional>
#include <memory>
#include <thread>

#include "pch.h"
#include "Clients.h"

namespace {
	using boost::asio::ip::make_address_v4;
	using namespace std::placeholders;

	class ClientsListener {
	public:
		virtual void onClientsUpdate(std::forward_list<ClientInfo> clients) = 0;
	};

	class MockClientsListener : public ClientsListener {
	public:
		MOCK_METHOD(void, onClientsUpdate, (std::forward_list<ClientInfo> clients), (override));
	};

    class ClientsTest : public testing::Test {
    protected:
        void SetUp() override {
            clients_ = std::make_unique<Clients>();
        }
		void TearDown() override {
			clients_.release();
		}
        std::unique_ptr<Clients> clients_;
    };

	TEST_F(ClientsTest, AddClientsListener) {
		std::vector<MockClientsListener> listeners(10);
		for (auto&& listener : listeners) {
			// Expect 2 calls - the initial update and on a new client
			EXPECT_CALL(listener, onClientsUpdate).Times(2);
		}

		for (auto&& listener : listeners) {
			clients_->addClientsListener(std::bind(&MockClientsListener::onClientsUpdate, &listener,
				_1));
		}
		clients_->add(make_address_v4("127.0.0.1"), Audio::Compression::none);
	}

	TEST_F(ClientsTest, Add) {
		const auto threadCount = 5;
		const auto operationsPerThread = 50;
		const auto operationCount = threadCount * operationsPerThread;
		MockClientsListener listener;
		// Add 1 expected call for the initial update
		EXPECT_CALL(listener, onClientsUpdate).Times(operationCount + 1);

		clients_->addClientsListener(std::bind(&MockClientsListener::onClientsUpdate, &listener,
			_1));
		std::barrier barrier(threadCount);
		auto addClients = [&](int start) {
			barrier.arrive_and_wait();
			for (int i = start; i < start + operationsPerThread; i++) {
				auto address = "192.168.0." + std::to_string(i);
				clients_->add(make_address_v4(address), Audio::Compression::none);
			}
		};
		std::vector<std::jthread> threads(threadCount);
		for (auto i = 0; i < operationCount; i+= operationsPerThread) {
			threads.emplace_back(addClients, i);
		}
	}

	TEST_F(ClientsTest, SetCompression) {
		using Audio::Compression;
		const auto compressions = {
			Compression::kbps_64,
			Compression::kbps_128,
			Compression::kbps_192,
			Compression::kbps_256,
			Compression::kbps_320
		};
		const auto address = make_address_v4("127.0.0.1");
		const auto threadCount = 5;

		MockClientsListener listener;
		// Expect the initial update
		EXPECT_CALL(listener, onClientsUpdate);
		std::forward_list<ClientInfo> clients;
		clients.push_front(ClientInfo(address, Compression::none));
		EXPECT_CALL(listener, onClientsUpdate(clients));
		for (auto&& c : compressions) {
			clients.front().compression = c;
			EXPECT_CALL(listener, onClientsUpdate(clients));
		}

		clients_->addClientsListener(std::bind(&MockClientsListener::onClientsUpdate, &listener,
			_1));
		clients_->add(address, Compression::none);
		std::barrier barrier(threadCount);
		auto compressionsSetter = [&](Audio::Compression compression) {
			barrier.arrive_and_wait();
			clients_->setCompression(address, compression);
		};
		std::vector<std::jthread> threads(threadCount);
		for (auto&& c : compressions) {
			threads.emplace_back(compressionsSetter, c);
		}
	}

	TEST_F(ClientsTest, Remove) {
		const auto threadCount = 5;
		const auto operationsPerThread = 50;
		const auto operationCount = threadCount * operationsPerThread;
		MockClientsListener listener;
		// Add 1 expected call for the initial update
		EXPECT_CALL(listener, onClientsUpdate).Times(operationCount + 1);
		for (int i = 0; i < operationCount; i++) {
			auto address = "192.168.0." + std::to_string(i);
			clients_->add(make_address_v4(address), Audio::Compression::none);
		}

		clients_->addClientsListener(std::bind(&MockClientsListener::onClientsUpdate, &listener,
			_1));
		std::barrier barrier(threadCount);
		auto removeClients = [&](int start) {
			barrier.arrive_and_wait();
			for (int i = start; i < start + operationsPerThread; i++) {
				auto address = "192.168.0." + std::to_string(i);
				clients_->remove(make_address_v4(address));
			}
		};
		std::vector<std::jthread> threads(threadCount);
		for (auto i = 0; i < operationCount; i += operationsPerThread) {
			threads.emplace_back(removeClients, i);
		}
	}
}
