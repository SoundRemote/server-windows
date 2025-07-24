#pragma once

#include <mmdeviceapi.h>

#include <atomic>
#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>

#include "resource.h"

class MuteButton;
class CapturePipe;
class Clients;
struct ClientInfo;
class Keystroke;
class Server;
class Settings;
class UpdateChecker;

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
	std::wstring defaultRenderDeviceLabel_;
	std::wstring defaultCaptureDeviceLabel_;
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
	// Data
	std::wstring currentDeviceId_;
	// Device key - number stored as data in select device ComboBox items
	// to
	// Device id string
	std::unordered_map<int, std::wstring> deviceIds_;
	// Utility
	boost::asio::io_context ioContext_;
	std::unique_ptr<std::thread> ioContextThread_;
	std::shared_ptr<Server> server_;
	std::unique_ptr<CapturePipe> capturePipe_;
	std::unique_ptr<Settings> settings_;
	std::shared_ptr<Clients> clients_;
	std::unique_ptr<UpdateChecker> updateChecker_;

	bool initInstance(int nCmdShow);
	// UI related
	void initStrings();
	void initInterface(HWND hWndParent);
	void initControls();
	void startPeakMeter() const;
	void stopPeakMeter() const;
	/// <summary>
	/// Adds devices for passed <code>EDataFlow</code>, including items for default devices.
	/// </summary>
	/// <param name="comboBox">ComboBox to add to.</param>
	/// <param name="flow">Can be eRender, eCapture or eAll</param>
	void addDevices(HWND comboBox, EDataFlow flow);
	/// <summary>
	/// Adds default device for EDataFlow::eRender or EDataFlow::eCapture.
	/// </summary>
	/// <param name="comboBox">ComboBox to add to.</param>
	/// <param name="flow">Must be EDataFlow::eRender or EDataFlow::eCapture.</param>
	void addDefaultDevice(HWND comboBox, EDataFlow flow);
	std::wstring getDeviceId(const int deviceIndex) const;
	/// <summary>
	/// Gets device key by its ID.
	/// Returns <c>invalidDeviceKey</c> if device with such ID could not be found.
	/// Returns <c>defaultRenderDeviceKey</c> for the default playback ID.
	/// Returns <c>defaultCaptureDeviceKey</c> for the default recording ID.
	/// </summary>
	/// <param name="deviceId">- device ID</param>
	/// <returns>device key</returns>
	int getDeviceKey(const std::wstring& deviceId) const;
	/// <summary>
	/// Gets saved capture device from the settings and selects it in the device
	/// combobox.
	/// </summary>
	void restoreCaptureDevice();
	/// <summary>
	/// Saves capture device to the settings
	/// </summary>
	/// <param name="deviceKey">- needed to save special IDs for the default
	/// playback and default recording devices</param>
	/// <param name="deviceId">- device system ID</param>
	void rememberCaptureDevice(int deviceKey, const std::wstring& deviceId);
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

	// Event handlers
	void onDeviceSelect();
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
	void changeCaptureDevice(const std::wstring& deviceId);
	void stopCapture();
	void asioEventLoop(boost::asio::io_context& ctx);

	static LRESULT CALLBACK staticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT wndProc(UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK about(HWND, UINT, WPARAM, LPARAM);
};
