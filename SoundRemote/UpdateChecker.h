#pragma once

#include <Windows.h>

#include <mutex>
#include <string>

constexpr auto WM_UPDATE_CHECK = (WM_APP + 0);
//wParam values for WM_UPDATE_CHECK
constexpr auto UPDATE_FOUND = 0;
constexpr auto UPDATE_NOT_FOUND = 1;
constexpr auto UPDATE_CHECK_ERROR = 2;

class UpdateChecker {
public:
	UpdateChecker(HWND mainWindow);

	/// <summary>
	/// Checks for a newer version and shows a message window with the result, depending
	/// on the <c>quiet</c> argument.
	/// </summary>
	/// <param name="quiet">inform only if there is an update</param>
	void checkUpdates(bool quiet = false);

private:
	HWND mainWindow_ = nullptr;
	std::mutex checkMutex_;

	/// <summary>
	/// Returns product version or an empty string on error
	/// </summary>
	/// <returns>
	/// Product version as string, for example "0.1.2.3"
	/// </returns>
	std::string getVersion() const;

	/// <summary>
	/// Fetches latest release JSON
	/// </summary>
	/// <returns>
	/// UTF-8 JSON string
	/// </returns>
	std::u8string getLatestRelease() const;

	/// <summary>
	/// Parses tag from the release JSON
	/// </summary>
	/// <param name="json">UTF-8 JSON string</param>
	/// <returns>
	/// Tag name
	/// </returns>
	std::string parseReleaseTag(const std::u8string& json) const;

	/// <summary>
	/// Thread worker
	/// </summary>
	/// <param name="quiet">inform only if there is an update</param>
	void checkWorker(bool quiet);

	/// <summary>
	/// Shows a message window with the result
	/// </summary>
	/// <param name="result">one of <c>UPDATE_FOUND</c>, <c>UPDATE_NOT_FOUND</c>,
	/// <c>UPDATE_CHECK_ERROR</c></param>
	/// <param name="quiet">show message window only if <c>result</c> == <c>UPDATE_FOUND</c></param>
	void showResult(int result, bool quiet) const ;
};
