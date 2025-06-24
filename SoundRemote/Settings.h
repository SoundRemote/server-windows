#pragma once

#include <memory>
#include <string>

#include <SimpleIni.h>

class Settings {
public:
	Settings(const std::string& fileName);
	int getServerPort() const;
	int getClientPort() const;

private:
	std::string fileName_;
	std::unique_ptr<CSimpleIniA> ini_;

	void setDefaultValues();
	void checkMissingSettings();
};
