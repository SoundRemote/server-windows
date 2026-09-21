#include "Devices.h"

Devices::Devices(
    std::function<std::wstring()> loadDevice,
    std::function<void(std::wstring)> saveDevice,
    GetDevicesFunction getEndpointDevices,
    std::function<std::optional<std::wstring>(EDataFlow)> getDefaultDeviceId,
    std::function<void(const std::list<DeviceUIState>&)> deviceListUpdateCallback,
    std::function<void(int)> deviceKeyUpdateCallback,
    std::function<void(std::optional<std::wstring>)> deviceIdUpdateCallback
):
    loadDevice_(loadDevice),
    saveDevice_(saveDevice),
    getEndpointDevices_(getEndpointDevices),
    getDefaultDeviceId_(getDefaultDeviceId),
    listUpdate_(deviceListUpdateCallback),
    keyUpdate_(deviceKeyUpdateCallback),
    idUpdate_(deviceIdUpdateCallback)
{
    listUpdate_(initDeviceList());
}

bool Devices::loadDevice() {
    if (deviceIds_.empty()) { return false; }
    auto savedDeviceId = loadDevice_();
    int savedDeviceKey = invalidDeviceKey;
    // savedDeviceId may content a special value for a default device
    if (savedDeviceId == defaultPlaybackDeviceId) {
        // If need to load default playback device, but there is no default playback device now.
        if (!currentDefaultPlaybackDeviceId_) {
            return false;
        }
        savedDeviceKey = defaultPlaybackDeviceKey;
        savedDeviceId = currentDefaultPlaybackDeviceId_.value();
    } else if (savedDeviceId == defaultRecordingDeviceId) {
        // If need to load default recording device, but there is no default recording device now.
        if (!currentDefaultRecordingDeviceId_) {
            return false;
        }
        savedDeviceKey = defaultRecordingDeviceKey;
        savedDeviceId = currentDefaultRecordingDeviceId_.value();
    } else {
        savedDeviceKey = getDeviceKey(savedDeviceId);
    }
    if (invalidDeviceKey == savedDeviceKey || currentDeviceKey_ == savedDeviceKey) {
        return false;
    }
    currentDeviceKey_ = savedDeviceKey;
    keyUpdate_(savedDeviceKey);
    idUpdate_(savedDeviceId);
    return true;
}

void Devices::selectDefaultDevice() {
    if (deviceIds_.empty() || defaultPlaybackDeviceKey == currentDeviceKey_) {
        return;
    }
    if (currentDefaultPlaybackDeviceId_) {
        currentDeviceKey_ = defaultPlaybackDeviceKey;
        saveDevice(defaultPlaybackDeviceKey, currentDefaultPlaybackDeviceId_.value());
        keyUpdate_(defaultPlaybackDeviceKey);
        idUpdate_(currentDefaultPlaybackDeviceId_.value());
        return;
    }
    // Try default recording device
    if (!currentDefaultRecordingDeviceId_ || defaultRecordingDeviceKey == currentDeviceKey_) {
        return;
    }
    currentDeviceKey_ = defaultRecordingDeviceKey;
    saveDevice(defaultRecordingDeviceKey, currentDefaultRecordingDeviceId_.value());
    keyUpdate_(defaultRecordingDeviceKey);
    idUpdate_(currentDefaultRecordingDeviceId_.value());
}

void Devices::onDeviceSelected(const int selectedDeviceKey) {
    if (selectedDeviceKey == currentDeviceKey_) { return; }
    const auto newDeviceId = getDeviceId(selectedDeviceKey);
    if (!newDeviceId) {
        currentDeviceKey_ = invalidDeviceKey;
        idUpdate_(std::nullopt);
        return;
    }
    const auto currentDeviceId = getDeviceId(currentDeviceKey_);

    currentDeviceKey_ = selectedDeviceKey;
    saveDevice(selectedDeviceKey, *newDeviceId);
    if (currentDeviceId != newDeviceId) {
        idUpdate_(*newDeviceId);
    }
}

void Devices::onDeviceAdded() {
    // If current device is a default device
    if (currentDeviceKey_ == defaultPlaybackDeviceKey ||
        currentDeviceKey_ == defaultRecordingDeviceKey) {
        const int key = currentDeviceKey_;
        // Update list and select the same default device reselect the same device key.
        listUpdate_(initDeviceList());
        currentDeviceKey_ = key;
        keyUpdate_(key);
        return;
    }
    // If not a default device.
    // - update list
    // - get the new device key by device id
    // - update device key
    const auto deviceId = getDeviceId(currentDeviceKey_);
    listUpdate_(initDeviceList());
    if (!deviceId) { return; }
    currentDeviceKey_ = getDeviceKey(deviceId.value());
    keyUpdate_(currentDeviceKey_);
}

void Devices::onDeviceRemoved(const std::wstring& removedDeviceId) {
    if (currentDeviceKey_ == Devices::invalidDeviceKey) {
        listUpdate_(initDeviceList());
        return;
    }

    const auto currentDeviceId = getDeviceId(currentDeviceKey_);
    if (currentDeviceId == removedDeviceId) {
        idUpdate_(std::nullopt);
    }
    const int oldDeviceKey = currentDeviceKey_;

    listUpdate_(initDeviceList());

    if ((oldDeviceKey == Devices::defaultPlaybackDeviceKey && currentDefaultPlaybackDeviceId_) ||
        (oldDeviceKey == Devices::defaultRecordingDeviceKey && currentDefaultRecordingDeviceId_)) {
        currentDeviceKey_ = oldDeviceKey;
        keyUpdate_(currentDeviceKey_);
    } else if (currentDeviceId && (currentDeviceId != removedDeviceId)) {
        const int newDeviceKey = getDeviceKey(currentDeviceId.value());
        currentDeviceKey_ = newDeviceKey;
        keyUpdate_(currentDeviceKey_);
    }
}

std::list<DeviceUIState> Devices::initDeviceList() {
    currentDeviceKey_ = invalidDeviceKey;
    deviceIds_.clear();
    currentDefaultPlaybackDeviceId_.reset();
    currentDefaultRecordingDeviceId_.reset();

    int key = 1;
    std::list<DeviceUIState> result;

    const auto playbackDevices = getEndpointDevices_(eRender);
    if (!playbackDevices.empty()) {
        currentDefaultPlaybackDeviceId_ = getDefaultDeviceId_(eRender);
        result.emplace_back(defaultPlaybackDeviceKey);
        for (auto&& endpointDevice: playbackDevices) {
            result.emplace_back(key, endpointDevice.name);
            deviceIds_[key] = endpointDevice.id;
            key++;
        }
    }
    const auto recordingDevices = getEndpointDevices_(eCapture);
    if (!recordingDevices.empty()) {
        currentDefaultRecordingDeviceId_ = getDefaultDeviceId_(eCapture);
        result.emplace_back(defaultRecordingDeviceKey);
        for (auto&& endpointDevice: recordingDevices) {
            result.emplace_back(key, endpointDevice.name);
            deviceIds_[key] = endpointDevice.id;
            key++;
        }
    }
    return result;
}

std::optional<std::wstring> Devices::getDeviceId(const int deviceKey) const {
    if (invalidDeviceKey == deviceKey) { return {}; }
    if (auto device = deviceIds_.find(deviceKey); device != deviceIds_.end()) {
        return device->second;
    }
    switch (deviceKey) {
    case defaultPlaybackDeviceKey:
        return currentDefaultPlaybackDeviceId_;
    case defaultRecordingDeviceKey:
        return currentDefaultRecordingDeviceId_;
    default:
        return {};
    }
}

int Devices::getDeviceKey(const std::wstring& deviceId) const {
    for (auto&& keyToId : deviceIds_) {
        if (keyToId.second == deviceId) {
            return keyToId.first;
        }
    }
    return invalidDeviceKey;
}

void Devices::saveDevice(int deviceKey, const std::wstring& deviceId) const {
    switch (deviceKey) {
    case defaultPlaybackDeviceKey:
        saveDevice_(defaultPlaybackDeviceId);
        break;

    case defaultRecordingDeviceKey:
        saveDevice_(defaultRecordingDeviceId);
        break;

    default:
        saveDevice_(deviceId);
        break;
    };
}
