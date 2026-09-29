#pragma once

#include <windows.h>

namespace AppMessage {
	constexpr UINT UPDATE_CHECK = WM_APP + 0;
	constexpr UINT DEVICE_ADDED = WM_APP + 1;
	// LPARAM is a pointer to a std::wstring containing device id, receiver must free it.
	constexpr UINT DEVICE_REMOVED = WM_APP + 2;
	// WPARAM is an EDataFlow value.
	// LPARAM is either:
	//   - 0 if no default device is available.
	//   - pointer to a std::wstring containing device id, receiver must free it.
	constexpr UINT DEFAULT_DEVICE_CHANGED = WM_APP + 3;
}
