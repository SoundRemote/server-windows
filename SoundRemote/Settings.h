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

	void setCheckUpdates(bool check);
	void setCaptureDevice(const std::wstring& deviceId);

private:
	std::string fileName_;
	std::unique_ptr<CSimpleIniCaseW> ini_;

	void setDefaultValues();
	void checkMissingSettings();
};
