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
				[this](EDataFlow flow) -> std::optional<std::wstring> {
					return (flow == eRender)
						? getDefaultPlaybackDeviceId()
						: getDefaultRecordingDeviceId();
				},
				//deviceListUpdateCallback
				[this](auto list, auto key) {
					deviceList = list;
					deviceKey = key;
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

		std::optional<EndpointDevice> getDefaultPlaybackDevice() {
			return playbackEndpointDevices.empty()
				? std::nullopt
				: std::optional(playbackEndpointDevices.front());
		}

		std::optional<EndpointDevice> getDefaultRecordingDevice() {
			return recordingEndpointDevices.empty()
				? std::nullopt
				: std::optional(recordingEndpointDevices.front());
		}

		std::optional<std::wstring> getDefaultPlaybackDeviceId() {
			return playbackEndpointDevices.empty()
				? std::nullopt
				: std::optional(playbackEndpointDevices.front().id);
		}

		std::optional<std::wstring> getDefaultRecordingDeviceId() {
			return recordingEndpointDevices.empty()
				? std::nullopt
				: std::optional(recordingEndpointDevices.front().id);
		}
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
		loadedDeviceId = Devices::defaultRecordingDeviceId;

		bool loadResult = devices_->loadDevice();

		EXPECT_TRUE(loadResult);
		EXPECT_EQ(deviceKey, Devices::defaultRecordingDeviceKey);
		EXPECT_EQ(deviceId, getDefaultRecordingDeviceId());
	}

	// loadDevice()
	// - when there is a default playback device
	// - loads default playback device
	TEST_F(DevicesTest, loadDeviceLoadsDefaultPlaybackDeviceWhenPresent) {
		loadedDeviceId = Devices::defaultPlaybackDeviceId;

		bool loadResult = devices_->loadDevice();

		EXPECT_TRUE(loadResult);
		EXPECT_EQ(deviceKey, Devices::defaultPlaybackDeviceKey);
		EXPECT_EQ(deviceId, getDefaultPlaybackDeviceId());
	}

	// loadDevice()
	// - existing device
	// - returns true and updates device key and device id accordingly
	TEST_F(DevicesTest, loadDeviceExistingWorksCorrectly) {
		EndpointDevice targetDevice = recordingEndpointDevices.back();
		loadedDeviceId = targetDevice.id;

		bool loadResult = devices_->loadDevice();

		EXPECT_TRUE(loadResult);
		EXPECT_EQ(deviceId, targetDevice.id);
		std::optional<int> newTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(newTargetKey);
		EXPECT_EQ(deviceKey, newTargetKey);
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
		EXPECT_EQ(deviceId, getDefaultPlaybackDeviceId());
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
		EXPECT_EQ(deviceId, getDefaultRecordingDeviceId());
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
		devices_->onDeviceSelected(Devices::defaultPlaybackDeviceKey);

		EXPECT_EQ(deviceId, getDefaultPlaybackDeviceId());
		EXPECT_EQ(savedDeviceId, Devices::defaultPlaybackDeviceId);
	}

	// onDeviceSelected()
	// - select default recording device
	// - id callback is called, device id saved
	TEST_F(DevicesTest, onDeviceSelectedDefaultRecordingWorksCorrectly) {
		devices_->onDeviceSelected(Devices::defaultRecordingDeviceKey);

		EXPECT_EQ(deviceId, getDefaultRecordingDeviceId());
		EXPECT_EQ(savedDeviceId, Devices::defaultRecordingDeviceId);
	}

	// onDeviceSelected()
	// - select a non-default device
	// - id callback is called, device id saved
	TEST_F(DevicesTest, onDeviceSelectedNonDefaultWorksCorrectly) {
		EndpointDevice targetDevice = recordingEndpointDevices.back();
		std::optional<int> targetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(targetKey);

		devices_->onDeviceSelected(targetKey.value());

		EXPECT_EQ(deviceId, targetDevice.id);
		EXPECT_EQ(savedDeviceId, targetDevice.id);
	}

	// onDeviceSelected()
	// - select default playback device, then same device explicitly
	// - id callback is called on first select, device id saved both times
	TEST_F(DevicesTest, onDeviceSelectedExplicitWorksCorrectly) {
		EndpointDevice targetDevice = getDefaultPlaybackDevice().value();
		std::optional<int> targetKey = findDeviceKey(targetDevice.name, deviceList.value());
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
		EndpointDevice targetDevice = playbackEndpointDevices.back();
		std::optional<int> originalTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
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
		std::optional<int> newTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(newTargetKey);
		EXPECT_EQ(deviceKey, newTargetKey);
	}

	// onDeviceAdded()
	// - pre-select a recording device, then add a recording device
	// - list and key are updated, id isn't
	TEST_F(DevicesTest, onDeviceAddedCurrentRecording) {
		// Establish a target device
		EndpointDevice targetDevice = recordingEndpointDevices.back();
		std::optional<int> originalTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
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
		std::optional<int> newTargetKey = newTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(newTargetKey);
		EXPECT_EQ(deviceKey, newTargetKey);
	}

	// onDeviceAdded()
	// - no device is selected
	// - only list is updated
	TEST_F(DevicesTest, onDeviceAddedCurrentNothing) {
		deviceList.reset();

		devices_->onDeviceAdded();

		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceKey);
		EXPECT_FALSE(deviceId);
	}

	// --- onDeviceRemoved ---

	// onDeviceRemoved()
	// - no device is selected (default after ctor)
	// - only list is updated
	TEST_F(DevicesTest, onDeviceRemovedNoDeviceSelected) {
		// remember a device id and remove it
		std::wstring idRemoved = recordingEndpointDevices.back().id;
		recordingEndpointDevices.pop_back();
		// remember expected device list size
		size_t expectedSize = deviceList.value().size() - 1;
		deviceList.reset();

		devices_->onDeviceRemoved(idRemoved);

		EXPECT_EQ(deviceList.value().size(), expectedSize);
		EXPECT_FALSE(deviceKey);
		EXPECT_FALSE(deviceId);
	}

	// onDeviceRemoved()
	// - current default playback device is removed, there are other playback devices
	// - device id is updated to empty optional, device key is updated to default playback
	TEST_F(DevicesTest, onDeviceRemovedNonLastDefaultPlaybackDevice) {
		devices_->onDeviceSelected(Devices::defaultPlaybackDeviceKey);
		std::wstring idRemoved = getDefaultPlaybackDeviceId().value();
		// Remove one device, there should be at least one remaining
		playbackEndpointDevices.pop_front();
		deviceList.reset();

		devices_->onDeviceRemoved(idRemoved);

		EXPECT_TRUE(deviceList);
		EXPECT_EQ(deviceKey, Devices::defaultPlaybackDeviceKey);
		EXPECT_EQ(deviceId, std::nullopt);
	}

	// onDeviceRemoved()
	// - current default recording device is removed, there are other recording devices
	// - device id is updated to empty optional, device key is updated to default recording
	TEST_F(DevicesTest, onDeviceRemovedNonLastDefaultRecordingDevice) {
		devices_->onDeviceSelected(Devices::defaultRecordingDeviceKey);
		std::wstring idRemoved = getDefaultRecordingDeviceId().value();
		// Remove one device, there should be at least one remaining
		recordingEndpointDevices.pop_front();
		deviceList.reset();

		devices_->onDeviceRemoved(idRemoved);

		EXPECT_TRUE(deviceList);
		EXPECT_EQ(deviceKey, Devices::defaultRecordingDeviceKey);
		EXPECT_EQ(deviceId, std::nullopt);
	}

	// onDeviceRemoved()
	// - current default playback device is removed, there are no more playback devices
	// - device id is updated to empty optional, device key is not updated (no device selected)
	TEST_F(DevicesTest, onDeviceRemovedLastDefaultPlaybackDevice) {
		devices_->onDeviceSelected(Devices::defaultPlaybackDeviceKey);
		std::wstring idRemoved = getDefaultPlaybackDeviceId().value();
		// Remove all devices
		playbackEndpointDevices.clear();
		deviceList.reset();

		devices_->onDeviceRemoved(idRemoved);

		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceKey);
		EXPECT_EQ(deviceId, std::nullopt);
	}

	// onDeviceRemoved()
	// - current default recording device is removed, there are no more recording devices
	// - device id is updated to empty optional, device key is not updated (no device selected)
	TEST_F(DevicesTest, onDeviceRemovedLastDefaultRecordingDevice) {
		devices_->onDeviceSelected(Devices::defaultRecordingDeviceKey);
		std::wstring idRemoved = getDefaultRecordingDeviceId().value();
		// Remove all devices
		recordingEndpointDevices.clear();
		deviceList.reset();

		devices_->onDeviceRemoved(idRemoved);

		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceKey);
		EXPECT_EQ(deviceId, std::nullopt);
	}

	// onDeviceRemoved()
	// - current playback device is removed
	// - device id is updated to empty optional, device key is not updated (no device selected)
	TEST_F(DevicesTest, onDeviceRemovedCurrentPlaybackDevice) {
		// Establish a target device
		EndpointDevice targetDevice = playbackEndpointDevices.back();
		std::optional<int> targetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(targetKey);
		// Select the target device
		devices_->onDeviceSelected(targetKey.value());
		// Cleanup after selecting device
		deviceList.reset();
		deviceId.reset();
		deviceKey.reset();

		devices_->onDeviceRemoved(targetDevice.id);

		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceKey);
		EXPECT_EQ(deviceId, std::nullopt);
	}

	// onDeviceRemoved()
	// - current recording device is removed
	// - device id is updated to empty optional, device key is not updated (no device selected)
	TEST_F(DevicesTest, onDeviceRemovedCurrentRecordingDevice) {
		// Establish a target device
		EndpointDevice targetDevice = recordingEndpointDevices.back();
		std::optional<int> targetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(targetKey);
		// Select the target device
		devices_->onDeviceSelected(targetKey.value());
		// Cleanup after selecting device
		deviceList.reset();
		deviceId.reset();
		deviceKey.reset();

		devices_->onDeviceRemoved(targetDevice.id);

		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceKey);
		EXPECT_EQ(deviceId, std::nullopt);
	}

	// onDeviceRemoved()
	// - non-current playback device is removed
	// - device id is not updated, device key is updated
	TEST_F(DevicesTest, onDeviceRemovedNonCurrentPlaybackDevice) {
		// Establish a target device
		EndpointDevice targetDevice = playbackEndpointDevices.back();
		std::optional<int> originalTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(originalTargetKey);
		// Select the target device
		devices_->onDeviceSelected(originalTargetKey.value());
		// Cleanup after selecting device
		deviceList.reset();
		deviceId.reset();
		deviceKey.reset();

		devices_->onDeviceRemoved(targetDevice.id);

		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceKey);
		EXPECT_EQ(deviceId, std::nullopt);
	}

	// onDeviceRemoved()
	// - non-current recording device is removed
	// - device id is not updated, device key is updated
	TEST_F(DevicesTest, onDeviceRemovedNonCurrentRecordingDevice) {
		// Establish a target device
		EndpointDevice targetDevice = recordingEndpointDevices.back();
		std::optional<int> originalTargetKey = findDeviceKey(targetDevice.name, deviceList.value());
		EXPECT_TRUE(originalTargetKey);
		// Select the target device
		devices_->onDeviceSelected(originalTargetKey.value());
		// Cleanup after selecting device
		deviceList.reset();
		deviceId.reset();
		deviceKey.reset();

		devices_->onDeviceRemoved(targetDevice.id);

		EXPECT_TRUE(deviceList);
		EXPECT_FALSE(deviceKey);
		EXPECT_EQ(deviceId, std::nullopt);
	}

	// --- onDefaultDeviceChanged ---

	// onDefaultDeviceChanged()
	// - default playback device selected, no new default playback device is available.
	// - device id is set to nullopt
	TEST_F(DevicesTest, onDefaultDeviceChangedPlaybackDefaultUnavailable) {
		devices_->onDeviceSelected(Devices::defaultPlaybackDeviceKey);
		EXPECT_TRUE(deviceId);

		devices_->onDefaultDeviceChanged(eRender, std::nullopt);

		EXPECT_FALSE(deviceId);
	}

	// onDefaultDeviceChanged()
	// - default recording device selected, no new default recording device is available.
	// - device id is set to nullopt
	TEST_F(DevicesTest, onDefaultDeviceChangedRecordingDefaultUnavailable) {
		devices_->onDeviceSelected(Devices::defaultRecordingDeviceKey);
		EXPECT_TRUE(deviceId);

		devices_->onDefaultDeviceChanged(eCapture, std::nullopt);

		EXPECT_FALSE(deviceId);
	}

	// onDefaultDeviceChanged()
	// - default playback device selected
	// - device id is updated
	TEST_F(DevicesTest, onDefaultDeviceChangedPlaybackDefaultChanged) {
		devices_->onDeviceSelected(Devices::defaultPlaybackDeviceKey);
		std::wstring expectedId = playbackEndpointDevices.back().id;

		devices_->onDefaultDeviceChanged(eRender, expectedId);

		EXPECT_EQ(deviceId, expectedId);
	}

	// onDefaultDeviceChanged()
	// - default recording device selected
	// - device id is updated
	TEST_F(DevicesTest, onDefaultDeviceChangedRecordingDefaultChanged) {
		devices_->onDeviceSelected(Devices::defaultRecordingDeviceKey);
		std::wstring expectedId = recordingEndpointDevices.back().id;

		devices_->onDefaultDeviceChanged(eCapture, expectedId);

		EXPECT_EQ(deviceId, expectedId);
	}

}
