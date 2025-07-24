#pragma once

#include <memory>
#include <string>

#include <SimpleIni.h>

constexpr auto defaultRenderDeviceId = L"default_playback";
constexpr auto defaultCaptureDeviceId = L"default_recording";

class Settings {
public:
	Settings(const std::string& fileName);
	int getServerPort() const;
	int getClientPort() const;
	bool getCheckUpdates() const;
	std::wstring getCaptureDevice() const;
	/// <summary>
	/// Updates the 'check_updates' preference and modifies the file only if the
	/// new value is different from the current one.
	/// </summary>
	/// <param name="value">- to check for updates on startup</param>
	void setCheckUpdates(bool value);
	/// <summary>
	/// Updates the 'capture_device' preference and modifies the file only if the
	/// new value is different from the current one.
	/// </summary>
	/// <param name="deviceId">- device ID</param>
	void setCaptureDevice(const std::wstring& deviceId);

private:
	std::string fileName_;
	std::unique_ptr<CSimpleIniCaseW> ini_;

	void setDefaultValues();
	void checkMissingSettings();
};
