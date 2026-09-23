#pragma once

#include "DeviceUIState.h"

#include <mmdeviceapi.h>

#include <functional>
#include <list>
#include <optional>

#include "EndpointDevice.h"

class Devices {
public:
	using GetDevicesFunction = std::function<std::list<EndpointDevice>(EDataFlow)>;

	static constexpr auto defaultPlaybackDeviceKey = -1;
	static constexpr auto defaultRecordingDeviceKey = -2;

	static constexpr auto defaultPlaybackDeviceId = L"default_playback";
	static constexpr auto defaultRecordingDeviceId = L"default_recording";

	/// <summary>
	/// Devices repository. Constructor initializes device list. Doesn't select any device.
	/// </summary>
	/// <param name="loadDevice">
	/// - Select previously saved device.
	/// </param>
	/// <param name="saveDevice">
	/// - Save currently selected device.
	/// </param>
	/// <param name="getEndpointDevices">
	/// - Get all endpoint devices for a flow.
	/// </param>
	/// <param name="getDefaultDeviceId">
	/// - Get default device id for a flow.
	/// </param>
	/// <param name="deviceListUpdateCallback">
	/// - Device list and key update callback. When the key optional is empty, nothing should be
	/// selected.
	/// </param>
	/// <param name="deviceKeyUpdateCallback">
	/// - Device key update callback. When the optional is empty, nothing should be selected.
	/// </param>
	/// <param name="deviceIdUpdateCallback">
	/// - Device id update callback.
	/// </param>
	Devices(
		std::function<std::wstring()> loadDevice,
		std::function<void(std::wstring)> saveDevice,
		GetDevicesFunction getEndpointDevices,
		std::function<std::optional<std::wstring>(EDataFlow)> getDefaultDeviceId,
		std::function<void(const std::list<DeviceUIState>& devices, std::optional<int> key)>
			deviceListUpdateCallback,
		std::function<void(std::optional<int> key)> deviceKeyUpdateCallback,
		std::function<void(std::optional<std::wstring> id)> deviceIdUpdateCallback
	);

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
	void onDeviceSelected(const int selectedDeviceKey);

	/// <summary>
	/// To be called when a device was added.
	/// </summary>
	void onDeviceAdded();

	/// <summary>
	/// To be called when a device was removed.
	/// </summary>
	void onDeviceRemoved(const std::wstring& removedDeviceId);

private:
	/// <summary>
	/// Builds device list.
	/// <para>Resets: current device key, device key-id map, default playback devices ids</para>
	/// </summary>
	/// <returns>The device list.</returns>
	std::list<DeviceUIState> initDeviceList();

	/// <summary>
	/// Returns device id by a device key. Returns real device id for default devices.
	/// </summary>
	/// <param name="deviceKey">- device key to find.</param>
	/// <returns>
	/// Device id or an empty <c>optional</c> if failed to get device id.
	/// </returns>
	std::optional<std::wstring> getDeviceId(const std::optional<int> deviceKey) const;

	/// <summary>
	/// Looks for device id in the Device key-id map and returns the corresponding key.
	/// If id was not found, returns an empty <c>optional</c>.
	/// </summary>
	/// <param name="deviceId">device id to find</param>
	/// <returns>device key</returns>
	std::optional<int> getDeviceKey(const std::wstring& deviceId) const;

	/// <summary>
	/// Saves device.
	/// </summary>
	/// <param name="deviceKey">- device key</param>
	/// <param name="deviceId">- device id</param>
	void saveDevice(int deviceKey, const std::wstring& deviceId) const;

	/// Device key-id map
	std::unordered_map<int, std::wstring> deviceIds_;
	std::optional<int> currentDeviceKey_;
	std::optional<std::wstring> currentDefaultPlaybackDeviceId_;
	std::optional<std::wstring> currentDefaultRecordingDeviceId_;

	std::function<std::wstring()> loadDevice_;
	std::function<void(std::wstring deviceId)> saveDevice_;
	GetDevicesFunction getEndpointDevices_;
	std::function<std::optional<std::wstring>(EDataFlow flow)> getDefaultDeviceId_;
	std::function<void(const std::list<DeviceUIState>& devices, std::optional<int> deviceKey)>
		listUpdate_;
	std::function<void(std::optional<int> deviceKey)> keyUpdate_;
	std::function<void(std::optional<std::wstring> deviceId)> idUpdate_;
};
