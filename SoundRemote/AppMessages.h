#pragma once

#include <windows.h>

namespace AppMessage {
	constexpr UINT UPDATE_CHECK = WM_APP + 0;
	constexpr UINT DEVICE_ADDED = WM_APP + 1;
	// LPARAM is a pointer to a std::wstring, receiver must free it.
	constexpr UINT DEVICE_REMOVED = WM_APP + 2;
}
