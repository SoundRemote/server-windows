#include "pch.h"
#include "Devices.h"

namespace {

	class DevicesTest : public testing::Test {
	protected:
		void SetUp() override {
			devices_ = std::make_unique<Devices>(
				// loadDevice
				[this]() { return loadedDeviceId; },
				// saveDevice
				[this](std::wstring id) { savedDeviceId = id; },
				// getEndpointDevices
				[this](EDataFlow flow) {
					if (flow == eRender) {
						return playbackEndpointDevices;
					} else {
						return recordingEndpointDevices;
					}
				},
				// getDefaultDeviceId
				// returns the first flow device
				[this](EDataFlow flow) -> std::optional<std::wstring> {
					if (flow == eRender) {
						if (playbackEndpointDevices.empty()) {
							return std::nullopt;
						} else {
							return playbackEndpointDevices.front().id;
						}
					} else {
						if (recordingEndpointDevices.empty()) {
							return std::nullopt;
						} else {
							return recordingEndpointDevices.front().id;
						}
					}
				},
				//deviceListUpdateCallback
				[this](auto list) {
					deviceList = list;
				},
				// deviceKeyUpdateCallback
				[this](auto key) {
					deviceKey = key;
				},
				// deviceIdUpdateCallback
				[this](auto id) {
					deviceId = id;
				}
			);
		}

		void TearDown() override {
			devices_.release();
		}

		std::unique_ptr<Devices> devices_;
		// device id returned by loadDevice function
		std::wstring loadedDeviceId{ L"" };
		// device id passed to saveDevice function
		std::optional<std::wstring> savedDeviceId;
		// playback devices
		std::list<EndpointDevice> playbackEndpointDevices{
			{L"playback device 0", L"playback_0_id"},
			{L"playback device 1", L"playback_1_id"}
		};
		// recording devices
		std::list<EndpointDevice> recordingEndpointDevices{
			{L"recording device 0", L"recording_0_id"},
			{L"recording device 1", L"recording_1_id"}
		};
		// callback list
		std::optional<std::list<DeviceUIState>> deviceList;
		// callback key
		std::optional<int> deviceKey;
		// callback id
		std::optional<std::wstring> deviceId;
	};

	// --- helpers ---

	std::optional<int> findDeviceKey(
		const std::wstring& name,
		const std::list<DeviceUIState>& devices
	) {
		auto iter = devices.cbegin();
		while (iter != devices.cend() && iter->name != name) {
			iter++;
		}
		if (iter == devices.cend()) { return std::nullopt; }
		return iter->key;
	}

	// Constructor
	// - when there are playback and recording devices
	// - device list callback is called
	TEST_F(DevicesTest, constructorCallsListUpdateCallback) {
		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceList.value().empty());
	}

	// --- loadDevice ---

	// loadDevice()
	// - when there is a default recording device
	// - loads default recording device
	TEST_F(DevicesTest, loadDeviceLoadsDefaultRecordingDeviceWhenPresent) {
		const std::wstring& expectedId = recordingEndpointDevices.front().id;
		loadedDeviceId = Devices::defaultRecordingDeviceId;

		bool loadResult = devices_->loadDevice();

		EXPECT_TRUE(loadResult);
		EXPECT_EQ(deviceKey, Devices::defaultRecordingDeviceKey);
		EXPECT_EQ(deviceId, expectedId);
	}

	// loadDevice()
	// - when there is a default playback device
	// - loads default playback device
	TEST_F(DevicesTest, loadDeviceLoadsDefaultPlaybackDeviceWhenPresent) {
		const std::wstring& expectedId = playbackEndpointDevices.front().id;
		loadedDeviceId = Devices::defaultPlaybackDeviceId;

		bool loadResult = devices_->loadDevice();

		EXPECT_TRUE(loadResult);
		EXPECT_EQ(deviceKey, Devices::defaultPlaybackDeviceKey);
		EXPECT_EQ(deviceId, expectedId);
	}

	// loadDevice()
	// - existing device
	// - returns true and updates device key and device id accordingly
	TEST_F(DevicesTest, loadDeviceExistingWorksCorrectly) {
		const auto& targetDevice = recordingEndpointDevices.front();
		const std::wstring& targetName = targetDevice.name;
		const std::wstring& targetId = targetDevice.id;
		loadedDeviceId = targetId;

		bool loadResult = devices_->loadDevice();

		EXPECT_TRUE(loadResult);
		EXPECT_EQ(deviceId, targetId);
		// Relying on uniqueness of names of all devices to check device key.
		for (auto&& deviceUIState : deviceList.value()) {
			if (deviceUIState.key == deviceKey) {
				EXPECT_EQ(deviceUIState.name, targetName);
				return;
			}
		}
	}

	// loadDevice()
	// - not existing device
	// - returns false and doesn't update device key or device id
	TEST_F(DevicesTest, loadDeviceNotExistingWorksCorrectly) {
		loadedDeviceId = L"this device id is not in the list";

		bool loadResult = devices_->loadDevice();

		EXPECT_FALSE(loadResult);
		EXPECT_FALSE(deviceKey);
		EXPECT_FALSE(deviceId);
	}

	// loadDevice()
	// - try to load default playback device when there are no playback devices
	// - returns false and doesn't update device key or device id
	TEST_F(DevicesTest, loadDeviceNotExistingDefaultPlaybackReturnsFalse) {
		loadedDeviceId = Devices::defaultPlaybackDeviceId;
		playbackEndpointDevices.clear();
		SetUp();

		bool loadResult = devices_->loadDevice();

		EXPECT_FALSE(loadResult);
		EXPECT_FALSE(deviceKey);
		EXPECT_FALSE(deviceId);
	}

	// loadDevice()
	// - try to load default recording device when there are no recording devices
	// - returns false and doesn't update device key or device id
	TEST_F(DevicesTest, loadDeviceNotExistingDefaultRecordingReturnsFalse) {
		loadedDeviceId = Devices::defaultRecordingDeviceId;
		recordingEndpointDevices.clear();
		SetUp();

		bool loadResult = devices_->loadDevice();

		EXPECT_FALSE(loadResult);
		EXPECT_FALSE(deviceKey);
		EXPECT_FALSE(deviceId);
	}

	// --- selectDefaultDevice ---

	// selectDefaultDevice()
	// - there are playback devices
	// - loads default playback device
	TEST_F(DevicesTest, selectDefaultDeviceLoadsDefaultPlaybackDevice) {
		devices_->selectDefaultDevice();

		EXPECT_EQ(deviceKey, Devices::defaultPlaybackDeviceKey);
		EXPECT_EQ(savedDeviceId, Devices::defaultPlaybackDeviceId);
	}


	// selectDefaultDevice()
	// - there are no playback devices, but there are recording devices
	// - loads default recording device
	TEST_F(DevicesTest, selectDefaultDeviceLoadsDefaultRecordingDevice) {
		playbackEndpointDevices.clear();
		SetUp();

		devices_->selectDefaultDevice();

		EXPECT_EQ(deviceKey, Devices::defaultRecordingDeviceKey);
		EXPECT_EQ(deviceId, recordingEndpointDevices.front().id);
		EXPECT_EQ(savedDeviceId, Devices::defaultRecordingDeviceId);
	}

	// selectDefaultDevice()
	// - there are no devices
	// - loads nothing and calls no callbacks
	TEST_F(DevicesTest, selectDefaultDeviceNoDevicesLoadsNothing) {
		playbackEndpointDevices.clear();
		recordingEndpointDevices.clear();
		SetUp();

		devices_->selectDefaultDevice();

		EXPECT_FALSE(deviceKey);
		EXPECT_FALSE(deviceId);
		EXPECT_FALSE(savedDeviceId);
	}

	// --- onDeviceSelected ---

	// onDeviceSelected()
	// - select default playback device
	// - id callback is called, device id saved
	TEST_F(DevicesTest, onDeviceSelectedDefaultPlaybackWorksCorrectly) {
		const auto& expectedDeviceId = playbackEndpointDevices.front().id;

		devices_->onDeviceSelected(Devices::defaultPlaybackDeviceKey);

		EXPECT_EQ(deviceId, expectedDeviceId);
		EXPECT_EQ(savedDeviceId, Devices::defaultPlaybackDeviceId);
	}

	// onDeviceSelected()
	// - select default recording device
	// - id callback is called, device id saved
	TEST_F(DevicesTest, onDeviceSelectedDefaultRecordingWorksCorrectly) {
		const auto& expectedDeviceId = recordingEndpointDevices.front().id;

		devices_->onDeviceSelected(Devices::defaultRecordingDeviceKey);

		EXPECT_EQ(deviceId, expectedDeviceId);
		EXPECT_EQ(savedDeviceId, Devices::defaultRecordingDeviceId);
	}

	// onDeviceSelected()
	// - select a non-default device
	// - id callback is called, device id saved
	TEST_F(DevicesTest, onDeviceSelectedNonDefaultWorksCorrectly) {
		const auto& targetDevice = recordingEndpointDevices.front();
		auto targetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(targetKey);

		devices_->onDeviceSelected(targetKey.value());

		EXPECT_EQ(deviceId, targetDevice.id);
		EXPECT_EQ(savedDeviceId, targetDevice.id);
	}

	// onDeviceSelected()
	// - select default playback device, then same device explicitly
	// - id callback is called on first select, device id saved both times
	TEST_F(DevicesTest, onDeviceSelectedExplicitWorksCorrectly) {
		// Select the default playback device (1st device on the list)
		const auto& targetDevice = playbackEndpointDevices.front();
		auto targetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(targetKey);

		// 1) select default playback device
		devices_->onDeviceSelected(Devices::defaultPlaybackDeviceKey);

		EXPECT_EQ(deviceId, targetDevice.id);
		EXPECT_EQ(savedDeviceId, Devices::defaultPlaybackDeviceId);
		deviceId.reset();
		savedDeviceId.reset();

		// 2) select the same device explicitly
		devices_->onDeviceSelected(targetKey.value());

		EXPECT_FALSE(deviceId);
		EXPECT_EQ(savedDeviceId, targetDevice.id);
	}

	// --- onDeviceAdded ---

	// onDeviceAdded()
	// - pre-select default playback device, then add a playback device
	// - list and key are updated, id isn't
	TEST_F(DevicesTest, onDeviceAddedCurrentDefaultPlayback) {
		devices_->onDeviceSelected(Devices::defaultPlaybackDeviceKey);
		// Cleanup after selecting device
		deviceList.reset();
		deviceKey.reset();
		deviceId.reset();

		playbackEndpointDevices.emplace_front(L"some name", L"some id");
		devices_->onDeviceAdded();

		EXPECT_TRUE(deviceList);
		EXPECT_EQ(deviceKey, Devices::defaultPlaybackDeviceKey);
		EXPECT_FALSE(deviceId);
	}

	// onDeviceAdded()
	// - pre-select default recording device, then add a recording device
	// - list and key are updated, id isn't
	TEST_F(DevicesTest, onDeviceAddedCurrentDefaultRecording) {
		devices_->onDeviceSelected(Devices::defaultRecordingDeviceKey);
		// Cleanup after selecting device
		deviceList.reset();
		deviceId.reset();
		deviceKey.reset();

		recordingEndpointDevices.emplace_front(L"some name", L"some id");
		devices_->onDeviceAdded();

		EXPECT_TRUE(deviceList);
		EXPECT_EQ(deviceKey, Devices::defaultRecordingDeviceKey);
		EXPECT_FALSE(deviceId);
	}

	// onDeviceAdded()
	// - pre-select a playback device, then add a playback device
	// - list and key are updated, id isn't
	TEST_F(DevicesTest, onDeviceAddedCurrentPlayback) {
		// Establish a target device
		const auto& targetDevice = playbackEndpointDevices.front();
		const auto originalTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(originalTargetKey);
		// Select the target device
		devices_->onDeviceSelected(originalTargetKey.value());
		// Cleanup after selecting device
		deviceList.reset();
		deviceId.reset();
		deviceKey.reset();

		playbackEndpointDevices.emplace_front(L"some name", L"some id");
		devices_->onDeviceAdded();

		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceId);
		const auto newTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(newTargetKey);
		EXPECT_EQ(deviceKey, newTargetKey);
	}

	// onDeviceAdded()
	// - pre-select a recording device, then add a recording device
	// - list and key are updated, id isn't
	TEST_F(DevicesTest, onDeviceAddedCurrentRecording) {
		// Establish a target device
		const auto& targetDevice = recordingEndpointDevices.front();
		const auto originalTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(originalTargetKey);
		// Select the target device
		devices_->onDeviceSelected(originalTargetKey.value());
		// Cleanup after selecting device
		deviceList.reset();
		deviceId.reset();
		deviceKey.reset();

		recordingEndpointDevices.emplace_front(L"some name", L"some id");
		devices_->onDeviceAdded();

		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceId);
		const auto newTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(newTargetKey);
		EXPECT_EQ(deviceKey, newTargetKey);
	}

	// onDeviceAdded()
	// - nothing is selected
	// - only list is updated
	TEST_F(DevicesTest, onDeviceAddedCurrentNothing) {
		deviceList.reset();

		devices_->onDeviceAdded();
		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceKey);
		EXPECT_FALSE(deviceId);
	}
}
