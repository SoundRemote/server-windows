#include "Keystroke.h"

#include <Windows.h>

#include <vector>

Keystroke::Keystroke(int key, int mods): key_(key) {
	if (mods == 0)
		return;
	if (mods & static_cast<int>(ModKey::Win))
		mods_.insert(ModKeyVk::Win);
	if (mods & static_cast<int>(ModKey::Ctrl))
		mods_.insert(ModKeyVk::Ctrl);
	if (mods & static_cast<int>(ModKey::Shift))
		mods_.insert(ModKeyVk::Shift);
	if (mods & static_cast<int>(ModKey::Alt))
		mods_.insert(ModKeyVk::Alt);
}

void Keystroke::emulate() const {
	const auto keyCount = mods_.size() + 1;
	const auto inputLen = keyCount * 2;
	std::vector<INPUT> inputs{ inputLen };

	// All inputs
	for (auto i = 0; i < inputLen; ++i) {
		inputs[i].type = INPUT_KEYBOARD;
	}
	// Second half of the inputs is KEYUP
	for (auto i = keyCount; i < inputLen; ++i) {
		inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
	}

	// Set all but the middle pair of inputs to mods
	int index = 0;
	for (ModKeyVk modKey : mods_) {
		inputs[index].ki.wVk = static_cast<int>(modKey);
		inputs[inputLen - 1 - index].ki.wVk = static_cast<int>(modKey);
		++index;
	}
	
	// Set the pair of inputs in the middle to the main key
	inputs[inputLen / 2 - 1].ki.wVk = key_;
	inputs[inputLen / 2 - 1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
	inputs[inputLen / 2].ki.wVk = key_;
	inputs[inputLen / 2].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;

	const int eventsInserted = SendInput(static_cast<UINT>(inputLen), inputs.data(), sizeof(INPUT));
	if (eventsInserted != inputLen) {
		//TODO: handle SendInput error
	}
}

std::wstring Keystroke::toString() const {
	std::vector<std::wstring> keys;
	if (mods_.contains(ModKeyVk::Win)) {
		keys.emplace_back(L"Win");
	}
	if (mods_.contains(ModKeyVk::Ctrl)) {
		keys.emplace_back(L"Ctrl");
	}
	if (mods_.contains(ModKeyVk::Shift)) {
		keys.emplace_back(L"Shift");
	}
	if (mods_.contains(ModKeyVk::Alt)) {
		keys.emplace_back(L"Alt");
	}
	keys.emplace_back(getVkCodeDescription(key_));
	std::wstring result;
	for (auto&& keyString: keys) {
		if (!result.empty()) {
			result.append(L" + ");
		}
		result.append(keyString);
	}
	return result;
}

std::wstring Keystroke::getVkCodeDescription(int vkCode) const {
	switch (vkCode) {
	case VK_BROWSER_BACK:
		return L"Browser Back";
	case VK_BROWSER_FORWARD:
		return L"Browser Forward";
	case VK_BROWSER_REFRESH:
		return L"Browser Refresh";
	case VK_BROWSER_STOP:
		return L"Browser Stop";
	case VK_BROWSER_SEARCH:
		return L"Browser Search";
	case VK_BROWSER_FAVORITES:
		return L"Browser Favorites";
	case VK_BROWSER_HOME:
		return L"Browser Home";

	case VK_VOLUME_MUTE:
		return L"Mute";
	case VK_VOLUME_DOWN:
		return L"Volume Down";
	case VK_VOLUME_UP:
		return L"Volume Up";
	case VK_MEDIA_NEXT_TRACK:
		return L"Next Track";
	case VK_MEDIA_PREV_TRACK:
		return L"Previous Track";
	case VK_MEDIA_STOP:
		return L"Stop Media";
	case VK_MEDIA_PLAY_PAUSE:
		return L"Play/Pause Media";

	case VK_LAUNCH_MAIL:
		return L"Start Mail";
	case VK_LAUNCH_MEDIA_SELECT:
		return L"Select Media";
	case VK_LAUNCH_APP1:
		return L"Start Application 1";
	case VK_LAUNCH_APP2:
		return L"Start Application 2";

	case VK_SLEEP:
		return L"Computer Sleep";

	case VK_SNAPSHOT:
		return L"Print Screen";
	case VK_PAUSE:
		return L"Pause";

	default:
		break;
	}

	UINT scanCode = MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC);
	// Handle extended keys
	// https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#extended-key-flag
	switch (vkCode) {
	case VK_RCONTROL: case VK_RMENU:
	case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
	case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
	case VK_NUMLOCK: case VK_CANCEL: case VK_DIVIDE:
	case VK_LWIN: case VK_RWIN: case VK_APPS:
		scanCode |= KF_EXTENDED;
		break;
	}

	const auto descSize = 512;
	WCHAR desc[descSize];
	int result = GetKeyNameTextW(scanCode << 16, desc, descSize);
	if (result == 0) {
		return L"Unknown key";
	}
	return { desc, static_cast<size_t>(result) };
}
