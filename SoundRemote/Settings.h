#pragma once

#include <memory>
#include <string>

#include <SimpleIni.h>

class Settings {
public:
	Settings(const std::string& fileName);
	int getServerPort() const;
	int getClientPort() const;
	bool getCheckUpdates() const;

	void setCheckUpdates(bool check);

private:
	std::string fileName_;
	std::unique_ptr<CSimpleIniA> ini_;

	void setDefaultValues();
	void checkMissingSettings();
};
