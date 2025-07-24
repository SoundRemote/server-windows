#include "Settings.h"

#include "NetDefines.h"

namespace Section {
	constexpr auto network{ L"network" };
	constexpr auto general{ L"general" };
}

namespace Setting {
	constexpr auto serverPort{ L"server_port" };
	constexpr auto clientPort{ L"client_port" };
	constexpr auto checkUpdates{ L"check_updates" };
	constexpr auto captureDevice{ L"capture_device" };
}

namespace DefaultValue {
	constexpr auto serverPort{ Net::defaultServerPort };
	constexpr auto clientPort{ Net::defaultClientPort };
	constexpr bool checkUpdates{ true };
	constexpr auto captureDevice = defaultRenderDeviceId;
}

Settings::Settings(const std::string& fileName): fileName_(fileName) {
	ini_ = std::make_unique<CSimpleIniCaseW>(true);
	SI_Error rc = ini_->LoadFile(fileName.c_str());
	if (rc == SI_OK) {
		checkMissingSettings();
		return;
	}
	setDefaultValues();
	ini_->SaveFile(fileName_.c_str());
}

int Settings::getServerPort() const {
	return ini_->GetLongValue(Section::network, Setting::serverPort);
}

int Settings::getClientPort() const {
	return ini_->GetLongValue(Section::network, Setting::clientPort);
}

bool Settings::getCheckUpdates() const {
	return ini_->GetBoolValue(Section::general, Setting::checkUpdates);
}

std::wstring Settings::getCaptureDevice() const {
	return ini_->GetValue(Section::general, Setting::captureDevice);
}

void Settings::setCheckUpdates(bool value) {
	if (ini_->GetBoolValue(Section::general, Setting::checkUpdates) == value) {
		return;
	}
	ini_->SetBoolValue(Section::general, Setting::checkUpdates, value);
	ini_->SaveFile(fileName_.c_str());
}

void Settings::setCaptureDevice(const std::wstring& deviceId) {
	if (ini_->GetValue(Section::general, Setting::captureDevice) == deviceId) {
		return;
	}
	ini_->SetValue(Section::general, Setting::captureDevice, deviceId.c_str());
	ini_->SaveFile(fileName_.c_str());
}

void Settings::setDefaultValues() {
	ini_->SetLongValue(Section::network, Setting::serverPort, DefaultValue::serverPort);
	ini_->SetLongValue(Section::network, Setting::clientPort, DefaultValue::clientPort);
	ini_->SetBoolValue(Section::general, Setting::checkUpdates, DefaultValue::checkUpdates);
	ini_->SetValue(Section::general, Setting::captureDevice, DefaultValue::captureDevice);
}

void Settings::checkMissingSettings() {
	bool saveNeeded = false;
	if (!ini_->KeyExists(Section::network, Setting::serverPort)) {
		// todo: remove eventually 
		// Migrate value from version 0.5.2 or older
		auto serverPort = ini_->GetLongValue(L"", Setting::serverPort, DefaultValue::serverPort);
		ini_->SetLongValue(Section::network, Setting::serverPort, serverPort);
		saveNeeded = true;
	}
	if (!ini_->KeyExists(Section::network, Setting::clientPort)) {
		// todo: remove eventually 
		// Migrate value from version 0.5.2 or older
		auto clientPort = ini_->GetLongValue(L"", Setting::clientPort, DefaultValue::clientPort);
		ini_->SetLongValue(Section::network, Setting::clientPort, clientPort);
		saveNeeded = true;
	}
	if (!ini_->KeyExists(Section::general, Setting::checkUpdates)) {
		ini_->SetBoolValue(Section::general, Setting::checkUpdates, DefaultValue::checkUpdates);
		saveNeeded = true;
	}
	if (!ini_->KeyExists(Section::general, Setting::captureDevice)) {
		ini_->SetValue(Section::general, Setting::captureDevice, DefaultValue::captureDevice);
		saveNeeded = true;
	}
	auto keysWithoutSection = ini_->GetSectionSize(L"");
	if (keysWithoutSection > 0) {
		ini_->Delete(L"", nullptr);
		saveNeeded = true;
	}
	if (saveNeeded) {
		ini_->SaveFile(fileName_.c_str());
	}
}
