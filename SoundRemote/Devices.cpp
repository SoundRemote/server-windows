#include "Devices.h"

#include <cassert>

Devices::Devices(
    std::function<std::wstring()> loadDevice,
    std::function<void(std::wstring)> saveDevice,
    GetDevicesFunction getEndpointDevices,
    std::function<std::wstring(EDataFlow)> getDefaultDevice,
    std::function<void(const std::forward_list<DeviceUIState>&)> deviceListUpdateCallback,
    std::function<void(int)> deviceKeyUpdateCallback,
    std::function<void(std::wstring)> deviceIdUpdateCallback
):
    loadDevice_(loadDevice),
    saveDevice_(saveDevice),
    getEndpointDevices_(getEndpointDevices),
    getDefaultDevice_(getDefaultDevice),
    listUpdate_(deviceListUpdateCallback),
    keyUpdate_(deviceKeyUpdateCallback),
    idUpdate_(deviceIdUpdateCallback) {
}

void Devices::initDevices() {
    currentDeviceKey_ = invalidDeviceKey;
    auto deviceList = initDeviceList();
    listUpdate_(std::move(deviceList));
}

bool Devices::loadDevice() {
    if (deviceIds_.empty()) { return false; }
    auto savedDeviceId = loadDevice_();
    int savedDeviceKey = invalidDeviceKey;
    // savedDeviceId may content a special value for a default device
    if (savedDeviceId == defaultRecordingDeviceId) {
        savedDeviceKey = defaultRecordingDeviceKey;
        savedDeviceId = getDeviceId(defaultRecordingDeviceKey);
    } else if (savedDeviceId == defaultPlaybackDeviceId) {
        savedDeviceKey = defaultPlaybackDeviceKey;
        savedDeviceId = getDeviceId(defaultPlaybackDeviceKey);
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
    if (deviceIds_.empty()) { return; }
    if (hasPlaybackDevices) {
        auto deviceId = getDeviceId(defaultPlaybackDeviceKey);
        saveDevice(defaultPlaybackDeviceKey, deviceId);
        currentDeviceKey_ = defaultPlaybackDeviceKey;
        keyUpdate_(defaultPlaybackDeviceKey);
        idUpdate_(std::move(deviceId));
    } else if (hasRecordingDevices) {
        auto deviceId = getDeviceId(defaultRecordingDeviceKey);
        saveDevice(defaultRecordingDeviceKey, deviceId);
        currentDeviceKey_ = defaultRecordingDeviceKey;
        keyUpdate_(defaultRecordingDeviceKey);
        idUpdate_(std::move(deviceId));
    }
}

void Devices::onDeviceSelected(int newDeviceKey) {
    std::wstring currentDeviceId = getDeviceId(currentDeviceKey_);
    std::wstring newDeviceId = getDeviceId(newDeviceKey);
    currentDeviceKey_ = newDeviceKey;
    saveDevice(newDeviceKey, newDeviceId);
    if (currentDeviceId != newDeviceId) {
        idUpdate_(std::move(newDeviceId));
    }
}

std::forward_list<DeviceUIState> Devices::initDeviceList() {
    deviceIds_.clear();
    hasPlaybackDevices = false;
    hasRecordingDevices = false;

    int key = 1;
    std::forward_list<DeviceUIState> result;
    auto resIter = result.before_begin();

    const auto playbackDevices = getEndpointDevices_(eRender);
    if (!playbackDevices.empty()) {
        hasPlaybackDevices = true;
        resIter = result.emplace_after(resIter, defaultPlaybackDeviceKey);
        for (auto&& nameToId: playbackDevices) {
            resIter = result.emplace_after(resIter, key, nameToId.first);
            deviceIds_[key] = nameToId.second;
            key++;
        }
    }
    const auto recordingDevices = getEndpointDevices_(eCapture);
    if (!recordingDevices.empty()) {
        hasRecordingDevices = true;
        resIter = result.emplace_after(resIter, defaultRecordingDeviceKey);
        for (auto&& nameToId: recordingDevices) {
            resIter = result.emplace_after(resIter, key, nameToId.first);
            deviceIds_[key] = nameToId.second;
            key++;
        }
    }
    return result;
}

std::wstring Devices::getDeviceId(const int deviceKey) const {
    if (deviceIds_.contains(deviceKey)) {
        return deviceIds_.at(deviceKey);
    }
    assert(deviceKey == defaultRecordingDeviceKey || deviceKey == defaultPlaybackDeviceKey);
    EDataFlow flow = (deviceKey == defaultRecordingDeviceKey) ? eCapture : eRender;
    return getDefaultDevice_(flow);
}

int Devices::getDeviceKey(const std::wstring& deviceId) const {
    for (auto&& iter = deviceIds_.cbegin(); iter != deviceIds_.end(); ++iter) {
        if (iter->second == deviceId) {
            return iter->first;
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
