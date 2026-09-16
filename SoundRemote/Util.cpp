#include "Util.h"

#include <Windows.h>

#include <sstream>
#include <stdexcept>

void* Util::mainWindow_ = nullptr;
const std::regex Util::versionRegex_{ "(\\d+)\\.(\\d+)\\.(\\d+)\\.?(\\d+)?" };

void Util::setMainWindow(void* mainWindowHWND) {
    mainWindow_ = mainWindowHWND;
}

void Util::showError(const std::string& text) {
    MessageBoxA(reinterpret_cast<HWND>(mainWindow_), text.c_str(), "Error", MB_ICONERROR | MB_OK);
}

void Util::showInfo(const std::wstring& text, const std::wstring& caption) {
    MessageBoxW(reinterpret_cast<HWND>(mainWindow_), text.c_str(), caption.c_str(),
        MB_ICONINFORMATION | MB_OK);
}

std::string Util::makeAppErrorText(const std::string& where, const std::string& what) {
    return where + ": " + what;
}

std::string Util::makeFatalErrorText(ErrorCode ec) {
    std::ostringstream ss;
    ss << "Fatal error: " << static_cast<int>(ec);
    return ss.str();
}

int Util::isNewerVersion(const std::string& current, const std::string& latest) {
    std::smatch currentMatch;
    if (!std::regex_search(current, currentMatch, versionRegex_)) {
        return 0 - 1;
    }
    std::smatch latestMatch;
    if (!std::regex_search(latest, latestMatch, versionRegex_)) {
        return 0-2;
    }
    for (int i = 1; i < currentMatch.size(); ++i) {
        // If the one version is shorter, i.e. 1.2.3 and 1.2.3.4
        if (!(currentMatch[i].matched && latestMatch[i].matched)) {
            if (currentMatch[i].matched) {
                return 0;
            } else {
                return 1;
            }
        }
        unsigned long curV = 0, latestV = 0;
        try {
            curV = std::stoul(currentMatch[i].str());
            latestV = std::stoul(latestMatch[i].str());
        } catch (...) {
            return -3;
        }
        if (curV < latestV) {
            return 1;
        } else if (latestV < curV) {
            return 0;
        }
    }
    if (currentMatch.suffix().length() != 0 && latestMatch.suffix().length() == 0) {
        return 1;
    }
    return 0;
}
