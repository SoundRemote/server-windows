#include "Settings.h"

#include "NetDefines.h"

namespace Section {
	const char* network{ "network" };
}

namespace Setting {
	const char* serverPort{ "server_port" };
	const char* clientPort{ "client_port" };
}

Settings::Settings(const std::string& fileName): fileName_(fileName) {
	ini_ = std::make_unique<CSimpleIniA>();
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

void Settings::setDefaultValues() {
	ini_->SetLongValue(Section::network, Setting::serverPort, Net::defaultServerPort);
	ini_->SetLongValue(Section::network, Setting::clientPort, Net::defaultClientPort);
}

void Settings::checkMissingSettings() {
	bool saveNeeded = false;
	if (!ini_->KeyExists(Section::network, Setting::serverPort)) {
		// todo: remove eventually 
		// Migrate value from version 0.5.2 or older
		auto serverPort = ini_->GetLongValue("", Setting::serverPort, Net::defaultServerPort);
		ini_->SetLongValue(Section::network, Setting::serverPort, serverPort);
		saveNeeded = true;
	}
	if (!ini_->KeyExists(Section::network, Setting::clientPort)) {
		// todo: remove eventually 
		// Migrate value from version 0.5.2 or older
		auto clientPort = ini_->GetLongValue("", Setting::clientPort, Net::defaultClientPort);
		ini_->SetLongValue(Section::network, Setting::clientPort, clientPort);
		saveNeeded = true;
	}
	auto keysWithoutSection = ini_->GetSectionSize("");
	if (keysWithoutSection > 0) {
		ini_->Delete("", nullptr);
		saveNeeded = true;
	}
	if (saveNeeded) {
		ini_->SaveFile(fileName_.c_str());
	}
}
