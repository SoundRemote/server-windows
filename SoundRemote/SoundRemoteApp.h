#pragma once

#include <mmdeviceapi.h>

#include <atomic>
#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>

#include "resource.h"
#include "DeviceUIState.h"

class MuteButton;
class CapturePipe;
class Clients;
struct ClientInfo;
class Keystroke;
class Server;
class Settings;
class UpdateChecker;
class Devices;

class SoundRemoteApp {
public:
	SoundRemoteApp(_In_ HINSTANCE hInstance);
	~SoundRemoteApp();
	static std::unique_ptr<SoundRemoteApp> create(_In_ HINSTANCE hInstance);
	int exec(int nCmdShow);
private:
	HINSTANCE hInst_ = nullptr;						// current instance
	// Strings
	std::wstring mainWindowTitle_;
	std::wstring serverAddressesLabel_;
	std::wstring defaultPlaybackDeviceLabel_;
	std::wstring defaultRecordingDeviceLabel_;
	std::wstring clientListLabel_;
	std::wstring keystrokeListLabel_;
	std::wstring muteButtonText_;
	std::wstring updateCheckTitle_;
	std::wstring updateCheckFound_;
	std::wstring updateCheckNotFound_;
	std::wstring updateCheckError_;
	// Controls
	HWND mainWindow_ = nullptr;
	HWND deviceComboBox_ = nullptr;
	HWND clientsList_ = nullptr;
	HWND addressButton_ = nullptr;
	HWND peakMeterProgress_ = nullptr;
	HWND keystrokes_ = nullptr;
	std::unique_ptr<MuteButton> muteButton_;
	// Utility
	boost::asio::io_context ioContext_;
	std::unique_ptr<std::thread> ioContextThread_;
	std::shared_ptr<Server> server_;
	std::unique_ptr<CapturePipe> capturePipe_;
	std::unique_ptr<Settings> settings_;
	std::shared_ptr<Clients> clients_;
	std::unique_ptr<UpdateChecker> updateChecker_;
	std::unique_ptr<Devices> devices_;

	bool initInstance(int nCmdShow);
	// UI related
	void initStrings();
	void initInterface(HWND hWndParent);
	void startPeakMeter() const;
	void stopPeakMeter() const;
	long getCharHeight(HWND hWnd) const;

	// Description:
	//   Creates a tooltip for a control.
	// Parameters:
	//   toolWindow - window handle of the control to add the tooltip to.
	//   text - string to use as the tooltip text.
	//   parentWindow - parent window handle.
	// Returns:
	//   The handle to the tooltip.
	HWND setTooltip(HWND toolWindow, PTSTR text, HWND parentWindow) const;
	std::wstring loadStringResource(UINT resourceId) const;
	void initSettings();
	void initMenu();
	void initDevices();

	// Event handlers

	void onDeviceSelect();
	void onDeviceListUpdated(const std::forward_list<DeviceUIState>& devices) const;
	void onDeviceKeyUpdated(int deviceKey) const;
	void onDeviceIdUpdated(const std::wstring& deviceId);
	void onClientListUpdate(std::forward_list<std::string> clients) const;
	void onClientsUpdate(std::forward_list<ClientInfo> clients) const;
	void onAddressButtonClick() const;
	void updatePeakMeter();
	void onReceiveKeystroke(const Keystroke& keystroke) const;
	void checkUpdates(bool quiet = false);
	void onUpdateCheckFinish(WPARAM wParam, LPARAM lParam);
	void visitHomepage() const;

	/// <summary>
	/// Toggles a menu item
	/// </summary>
	/// <param name="itemId">menu item identifier</param>
	/// <returns>is item checked after toggle</returns>
	bool toggleMenuItem(UINT itemId) const;

	/// <summary>
	/// Starts server and audio processing.
	/// </summary>
	void run();
	void shutdown();
	void stopCapture();
	void startCapture(const std::wstring& deviceId);
	void asioEventLoop(boost::asio::io_context& ctx);

	static LRESULT CALLBACK staticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT wndProc(UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK about(HWND, UINT, WPARAM, LPARAM);
};
