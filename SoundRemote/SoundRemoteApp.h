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
	std::unordered_map<int, std::wstring> deviceIds_;	// <Number stored as data in CombBox items, Device id string>
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
	void startPeakMeter();
	void stopPeakMeter();
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
	long getCharHeight(HWND hWnd) const;

	// Description:
	//   Creates a tooltip for a control.
	// Parameters:
	//   toolWindow - window handle of the control to add the tooltip to.
	//   text - string to use as the tooltip text.
	//   parentWindow - parent window handle.
	// Returns:
	//   The handle to the tooltip.
	HWND setTooltip(HWND toolWindow, PTSTR text, HWND parentWindow);
	std::wstring loadStringResource(UINT resourceId);
	void initSettings();

	// Event handlers
	void onDeviceSelect();
	void onClientListUpdate(std::forward_list<std::string> clients);
	void onClientsUpdate(std::forward_list<ClientInfo> clients);
	void onAddressButtonClick() const;
	void updatePeakMeter();
	void onReceiveKeystroke(const Keystroke& keystroke);
	void checkUpdates();
	void onUpdateCheckFinish(WPARAM wParam, LPARAM lParam);
	void visitHomepage() const;

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
