#pragma once

#include "DeviceUIState.h"

#include <mmdeviceapi.h>

#include <forward_list>
#include <functional>
#include <optional>

class Devices {
public:
	using GetDevicesFunction =
		std::function<std::unordered_map<std::wstring, std::wstring>(EDataFlow)>;

	static constexpr auto defaultPlaybackDeviceKey = -1;
	static constexpr auto defaultRecordingDeviceKey = -2;
	static constexpr auto invalidDeviceKey = -3;

	static constexpr auto defaultPlaybackDeviceId = L"default_playback";
	static constexpr auto defaultRecordingDeviceId = L"default_recording";

	/// <summary>
	/// Devices repository.
	/// </summary>
	/// <param name="loadDevice">
	/// - select previously saved device.
	/// </param>
	/// <param name="saveDevice">
	/// - save currently selected device.
	/// </param>
	/// <param name="getEndpointDevices">
	/// - get all endpoint devices for a flow.
	/// </param>
	/// <param name="getDefaultDevice">
	/// - get default device id for a flow.
	/// </param>
	/// <param name="deviceListUpdateCallback">
	/// - device list update callback.
	/// </param>
	/// <param name="deviceKeyUpdateCallback">
	/// - device key update callback.
	/// </param>
	/// <param name="deviceIdUpdateCallback">
	/// - device id update callback.
	/// </param>
	Devices(
		std::function<std::wstring()> loadDevice,
		std::function<void(std::wstring)> saveDevice,
		GetDevicesFunction getEndpointDevices,
		std::function<std::wstring(EDataFlow)> getDefaultDevice,
		std::function<void(const std::forward_list<DeviceUIState>&)> deviceListUpdateCallback,
		std::function<void(int)> deviceKeyUpdateCallback,
		std::function<void(std::wstring)> deviceIdUpdateCallback
	);

	/// <summary>
	/// Init device list. Doesn't select any device.
	/// </summary>
	void initDevices();

	/// <summary>
	/// Loads the saved capture device.
	/// </summary>
	/// <returns>true if device was loaded successfully, false otherwise.</returns>
	bool loadDevice();

	/// <summary>
	/// Selects the default playback device. If there are no playback devices selects the default
	/// recording device.
	/// </summary>
	void selectDefaultDevice();

	/// <summary>
	/// To be called when a device was selected.
	/// </summary>
	/// <param name="newDeviceKey">- selected device key</param>
	void onDeviceSelected(int newDeviceKey);

private:
	/// <summary>
	/// Builds device list.
	/// </summary>
	/// <returns>The device list.</returns>
	std::forward_list<DeviceUIState> initDeviceList();

	/// <summary>
	/// Returns device id by a device key.
	/// </summary>
	/// <param name="deviceKey">- device key to find</param>
	/// <returns>device id string.</returns>
	std::wstring getDeviceId(const int deviceKey) const;

	/// <summary>
	/// Looks for device id in the Device key-id map and returns the corresponding key.
	/// If id was not found, returns <c>invalidDeviceKey</c>.
	/// </summary>
	/// <param name="deviceId">device id to find</param>
	/// <returns>device key</returns>
	int getDeviceKey(const std::wstring& deviceId) const;

	/// <summary>
	/// Saves device.
	/// </summary>
	/// <param name="deviceKey">- device key</param>
	/// <param name="deviceId">- device id</param>
	void saveDevice(int deviceKey, const std::wstring& deviceId) const;

	/// Device key-id map
	std::unordered_map<int, std::wstring> deviceIds_;
	int currentDeviceKey_ = invalidDeviceKey;
	std::optional<std::wstring> currentDefaultPlaybackDeviceId_;
	std::optional<std::wstring> currentDefaultRecordingDeviceId_;

	std::function<std::wstring()> loadDevice_;
	std::function<void(std::wstring)> saveDevice_;
	GetDevicesFunction getEndpointDevices_;
	std::function<std::wstring(EDataFlow)> getDefaultDevice_;
	std::function<void(const std::forward_list<DeviceUIState>&)> listUpdate_;
	std::function<void(int)> keyUpdate_;
	std::function<void(std::wstring)> idUpdate_;
};
