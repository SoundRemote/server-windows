#include <string>
#include <fstream>
#include <filesystem>

#include "pch.h"
#include "Settings.h"
#include "NetDefines.h"

namespace {

const std::string rootPath{ testing::TempDir() + "soundremotetests\\" };

class SettingsTest : public testing::Test {
protected:

	void SetUp() override {
		std::filesystem::remove_all(rootPath);
		bool rc = std::filesystem::create_directory(rootPath);
		ASSERT_TRUE(rc);
	}

	void TearDown() override {
		std::filesystem::remove_all(rootPath);
	}
};

TEST_F(SettingsTest, ServerPort_default_value) {
	const std::string iniFile = rootPath + "settings.ini";

	auto settings = Settings(iniFile);

	ASSERT_EQ(Net::defaultServerPort, settings.getServerPort());
}

TEST_F(SettingsTest, ServerPort_read) {
	const std::string iniFile = rootPath + "settings.ini";

	int expected = 568;
	std::ofstream os(iniFile, std::ios::out | std::ios::trunc);
	ASSERT_TRUE(os.is_open());
	os << "[network]" << std::endl << "server_port = " << expected;
	os.close();
	auto settings = Settings(iniFile);

	ASSERT_EQ(expected, settings.getServerPort());
}

TEST_F(SettingsTest, ClientPort_default_value) {
	const std::string iniFile = rootPath + "settings.ini";

	auto settings = Settings(iniFile);

	ASSERT_EQ(Net::defaultClientPort, settings.getClientPort());
}


TEST_F(SettingsTest, ClientPort_read) {
	const std::string iniFile = rootPath + "settings.ini";

	int expected = 6879;
	std::ofstream os(iniFile, std::ios::out | std::ios::trunc);
	ASSERT_TRUE(os.is_open());
	os << "[network]" << std::endl << "client_port=" << expected;
	os.close();

	auto settings = Settings(iniFile);

	ASSERT_EQ(expected, settings.getClientPort());
}

// Test migration of server port and client port from .ini files created by app
// version 0.5.2 or earlier, before .ini file had sections.
TEST_F(SettingsTest, Migration) {
	const std::string iniFile = rootPath + "old.ini";

	int expectedServerPort = 123;
	int expectedClientPort = 456;
	std::ofstream os(iniFile, std::ios::out | std::ios::trunc);
	ASSERT_TRUE(os.is_open());
	os << "server_port=" << expectedServerPort << std::endl << "client_port=" << expectedClientPort;
	os.close();

	auto settings = Settings(iniFile);

	ASSERT_EQ(expectedServerPort, settings.getServerPort());
	ASSERT_EQ(expectedClientPort, settings.getClientPort());
}

TEST_F(SettingsTest, CheckUpdates_default_value) {
	const std::string iniFile = rootPath + "settings.ini";

	auto settings = Settings(iniFile);

	ASSERT_TRUE(settings.getCheckUpdates());
}

TEST_F(SettingsTest, CheckUpdates_write_read) {
	const std::string iniFile = rootPath + "settings.ini";

	// Change to false with a Settings object
	Settings(iniFile).setCheckUpdates(false);
	// Check value with another Settings object
	auto settings = Settings(iniFile);

	ASSERT_FALSE(settings.getCheckUpdates());
}

TEST_F(SettingsTest, CaptureDevice_default_value) {
	const std::string iniFile = rootPath + "settings.ini";

	auto settings = Settings(iniFile);

	ASSERT_EQ(L"default_playback", settings.getCaptureDevice());
}

TEST_F(SettingsTest, CaptureDevice_write_read) {
	const std::string iniFile = rootPath + "settings.ini";
	auto expected = std::wstring{ L"expected dev1ce" };

	Settings(iniFile).setCaptureDevice(expected);
	auto settings = Settings(iniFile);

	ASSERT_EQ(expected, settings.getCaptureDevice());
}

}
