#include "UpdateChecker.h"

#include <winhttp.h>

#include <array>
#include <format>
#include <thread>
#include <vector>

#include <boost/json/src.hpp>

#include "Util.h"

// Preprocessor definitions added for the boost::json lib:
// BOOST_JSON_NO_LIB
// BOOST_CONTAINER_NO_LIB

using namespace boost::json;

namespace {
    class HttpHandleCloser {
    public:
        HttpHandleCloser(HINTERNET* pHandle): pHandle_(pHandle) {}
        ~HttpHandleCloser() {
            HINTERNET handle = *pHandle_;
            if (handle) { WinHttpCloseHandle(handle); }
        }
    private:
        HINTERNET* pHandle_;
    };
}

UpdateChecker::UpdateChecker(HWND mainWindow): mainWindow_(mainWindow) {
}

void UpdateChecker::checkUpdates(bool quiet) {
    std::jthread worker(&UpdateChecker::checkWorker, this, quiet);
    worker.detach();
}

std::string UpdateChecker::getVersion() const {
    TCHAR modulePath[MAX_PATH];
    if (!GetModuleFileName(nullptr, modulePath, MAX_PATH)) {
        return {};
    }

    DWORD dwHandle;
    auto versionInfoSize = GetFileVersionInfoSize(modulePath, &dwHandle);
    if (!versionInfoSize) {
        return {};
    }

    std::vector<unsigned char> buffer(versionInfoSize);
    if (!GetFileVersionInfo(modulePath, dwHandle, versionInfoSize, buffer.data())) {
        return {};
    }

    struct {
        WORD language;
        WORD codePage;
    } *translate = nullptr;
    UINT uiSize;
    if (!VerQueryValue(
        buffer.data(),
        TEXT("\\VarFileInfo\\Translation"),
        (LPVOID*)&translate,
        &uiSize
    )) {
        return {};
    }

    auto productVersionBlock = std::format(
        "\\StringFileInfo\\{:04x}{:04x}\\ProductVersion",
        translate[0].language,
        translate[0].codePage
    );
    LPSTR version = nullptr;
    UINT versionSize;
    if (!VerQueryValueA(
        buffer.data(),
        productVersionBlock.c_str(),
        (LPVOID*)&version,
        &versionSize
    )) {
        return {};
    }
    return std::string{ version };
}

// https://docs.github.com/en/rest/releases/releases?apiVersion=2022-11-28#get-the-latest-release
std::u8string UpdateChecker::getLatestRelease() const {
    BOOL results = FALSE;
    HINTERNET
        session = nullptr,
        connect = nullptr,
        request = nullptr;
    // Auto close HINTERNET handles
    HttpHandleCloser sessionCloser(&session);
    HttpHandleCloser connectCloser(&connect);
    HttpHandleCloser requestCloser(&request);

    session = WinHttpOpen(
        TEXT("SR server"),
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        WINHTTP_FLAG_SECURE_DEFAULTS
    );
    if (!session) { return {}; }

    connect = WinHttpConnect(
        session,
        TEXT("api.github.com"),
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );
    if (!connect) { return {}; }

    auto acceptHeader = TEXT("application/vnd.github+json");
    std::array<LPCWSTR, 2> acceptTypes{ acceptHeader, nullptr };
    request = WinHttpOpenRequest(
        connect,
        nullptr,
        TEXT("/repos/ashipo/FilmDB/releases/latest"),
        nullptr,
        WINHTTP_NO_REFERER,
        acceptTypes.data(),
        WINHTTP_FLAG_SECURE
    );
    if (!request) { return {}; }

    auto apiHeader = TEXT("X-GitHub-Api-Version: 2022-11-28");
    results = WinHttpSendRequest(
        request,
        apiHeader,
        -1L,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    );
    if (!results) { return {}; }

    results = WinHttpReceiveResponse(request, nullptr);
    if (!results) { return {}; }

    DWORD dwSize = 0;
    std::u8string result;
    do {
        // Check for available data.
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(request, &dwSize)) { return {}; }

        std::vector<char> outBuffer(dwSize + 1);
        if (WinHttpReadData(request, outBuffer.data(), dwSize, nullptr)) {
            result.append(reinterpret_cast<char8_t*>(outBuffer.data()));
        } else {
            return {};
        }
    } while (dwSize > 0);
    return result;
}

std::string UpdateChecker::parseReleaseTag(const std::u8string& json) const {
    boost::system::error_code ec;
    value jsonValue = parse(reinterpret_cast<const char*>(json.c_str()), ec);
    if (ec) { return {}; }

    auto jsonObject = jsonValue.if_object();
    if (!jsonObject) { return {}; }
       
    auto tagValue = jsonObject->if_contains("tag_name");
    if (!tagValue) { return {}; }

    auto tagString = tagValue->if_string();
    if (!tagString) { return {}; }

    return { tagString->c_str() };
}

void UpdateChecker::checkWorker(bool quiet) {
    const std::unique_lock<std::mutex> lock(checkMutex_, std::try_to_lock);
    if (!lock) { return; }

    auto version = getVersion();
    if (version.empty()) {
        showResult(UPDATE_CHECK_ERROR, quiet);
        return;
    }

    auto latestReleaseJson = getLatestRelease();
    if (latestReleaseJson.empty()) {
        showResult(UPDATE_CHECK_ERROR, quiet);
        return;
    }

    auto latestReleaseTag = parseReleaseTag(latestReleaseJson);
    if (latestReleaseTag.empty()) {
        showResult(UPDATE_CHECK_ERROR, quiet);
        return;
    }

    auto isNewerResult = Util::isNewerVersion(version, latestReleaseTag);
    switch (isNewerResult) {
    case 0:
        showResult(UPDATE_NOT_FOUND, quiet);
        return;
    case 1:
        showResult(UPDATE_FOUND, quiet);
        return;
    default:
        showResult(UPDATE_CHECK_ERROR, quiet);
        return;
    }
}

void UpdateChecker::showResult(int result, bool quiet) const {
    if (UPDATE_FOUND == result) {
        PostMessage(mainWindow_, WM_UPDATE_CHECK, result, 0);
        return;
    }
    // No updates or an error
    if (!quiet) {
        PostMessage(mainWindow_, WM_UPDATE_CHECK, result, 0);
    }
}
