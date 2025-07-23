#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "SoundRemoteApp.h"

#include <CommCtrl.h>
#include <Windows.h>
#include <Windowsx.h>
#include <shellapi.h>

#include <boost/asio/post.hpp>

#include "AudioUtil.h"
#include "CapturePipe.h"
#include "Clients.h"
#include "Controls.h"
#include "NetUtil.h"
#include "Server.h"
#include "Settings.h"
#include "Util.h"
#include "UpdateChecker.h"

using namespace std::placeholders;

namespace {
    constexpr int windowWidth = 300;            // main window width
    constexpr int windowHeight = 300;			// main window height
    constexpr int timerIdPeakMeter = 1;
    constexpr int timerPeriodPeakMeter = 33;    // in milliseconds
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    std::unique_ptr<SoundRemoteApp> app = SoundRemoteApp::create(_In_ hInstance);
    return app ? app->exec(nCmdShow) : 1;
}

SoundRemoteApp::SoundRemoteApp(_In_ HINSTANCE hInstance): hInst_(hInstance), ioContext_() {}

SoundRemoteApp::~SoundRemoteApp() {
    boost::asio::post(ioContext_, std::bind(&SoundRemoteApp::shutdown, this));

    if (ioContextThread_ && ioContextThread_->joinable()) {
        ioContextThread_->join();
    }
}

std::unique_ptr<SoundRemoteApp> SoundRemoteApp::create(_In_ HINSTANCE hInstance){
    return std::make_unique<SoundRemoteApp>(hInstance);
}

int SoundRemoteApp::exec(int nCmdShow) {
    if (!initInstance(nCmdShow)) {
        return 1;
    }

    run();

    HACCEL hAccelTable = LoadAccelerators(hInst_, MAKEINTRESOURCE(IDC_SOUNDREMOTE));
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(mainWindow_, &msg) && !TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

bool SoundRemoteApp::toggleMenuItem(UINT itemId) const {
    HMENU menu = GetMenu(mainWindow_);
    MENUITEMINFO mii{ sizeof(MENUITEMINFO) };
    mii.fMask = MIIM_STATE;
    GetMenuItemInfo(menu, itemId, FALSE, &mii);
    mii.fState ^= MFS_CHECKED;
    SetMenuItemInfo(menu, itemId, FALSE, &mii);
    return (mii.fState & MFS_CHECKED) != 0;
}

void SoundRemoteApp::run() {
    Util::setMainWindow(mainWindow_);
    initSettings();
    initMenu();
    if (settings_->getCheckUpdates()) {
        checkUpdates(true);
    }
    // Register for system suspend events
    RegisterSuspendResumeNotification(mainWindow_, DEVICE_NOTIFY_WINDOW_HANDLE);
    try {
        const auto clientPort = settings_->getClientPort();
        const auto serverPort = settings_->getServerPort();

        clients_ = std::make_shared<Clients>();
        clients_->addClientsListener(std::bind(&SoundRemoteApp::onClientsUpdate, this, _1));
        server_ = std::make_shared<Server>(clientPort, serverPort, ioContext_, clients_);
        clients_->addClientsListener(std::bind(&Server::onClientsUpdate, server_.get(), _1));
        server_->setKeystrokeCallback(std::bind(&SoundRemoteApp::onReceiveKeystroke, this, _1));
        // io_context will run as long as the server works and waiting for incoming packets.
        ioContextThread_ = std::make_unique<std::thread>(std::bind(&SoundRemoteApp::asioEventLoop, this, _1), std::ref(ioContext_));

        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    }
    catch (const std::exception& e) {
        Util::showError(e.what());
        std::exit(EXIT_FAILURE);
    }
    catch (...) {
        Util::showError("Start server: unknown error");
        std::exit(EXIT_FAILURE);
    }
    // Create audio source by selecting a device.
    onDeviceSelect();
}

void SoundRemoteApp::shutdown() {
    server_->sendDisconnectBlocking();
    stopCapture();
    ioContext_.stop();
}

void SoundRemoteApp::addDevices(HWND comboBox, EDataFlow flow) {
    const auto devices = Audio::getEndpointDevices(flow);
    if (devices.empty()) {
        return;
    }
    if (flow == eRender || flow == eAll) {                      // Add default playback
        addDefaultDevice(comboBox, eRender);
    }
    if (flow == eCapture || flow == eAll) {                     // Add default recording
        addDefaultDevice(comboBox, eCapture);
    }
    for (auto&& iter = devices.cbegin(); iter != devices.end(); ++iter) {
        const auto index = ComboBox_AddString(deviceComboBox_, iter->first.c_str());
        ComboBox_SetItemData(deviceComboBox_, index, index);
        deviceIds_[index] = iter->second;
    }
}

void SoundRemoteApp::addDefaultDevice(HWND comboBox, EDataFlow flow) {
    assert(flow == eRender || flow == eCapture);

    int newItemIndex, deviceKey;
    if (flow == eRender) {
        newItemIndex = ComboBox_AddString(comboBox, defaultRenderDeviceLabel_.data());
        deviceKey = Audio::defaultRenderDeviceKey;
    } else {
        newItemIndex = ComboBox_AddString(comboBox, defaultCaptureDeviceLabel_.data());
        deviceKey = Audio::defaultCaptureDeviceKey;
    }
    ComboBox_SetItemData(comboBox, newItemIndex, (LPARAM)deviceKey);
}

std::wstring SoundRemoteApp::getDeviceId(const int deviceKey) const {
    if (!deviceIds_.contains(deviceKey)) {
        assert(deviceKey == Audio::defaultCaptureDeviceKey || deviceKey == Audio::defaultRenderDeviceKey);
        EDataFlow flow = (deviceKey == Audio::defaultCaptureDeviceKey) ? eCapture : eRender;
        return Audio::getDefaultDevice(flow);
    }
    return deviceIds_.at(deviceKey);
}

long SoundRemoteApp::getCharHeight(HWND hWnd) const {
    HDC hdc = GetDC(hWnd);
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    ReleaseDC(hWnd, hdc);
    return tm.tmHeight;
}

HWND SoundRemoteApp::setTooltip(HWND toolWindow, PTSTR text, HWND parentWindow) {
    HWND tooltip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, parentWindow, NULL, hInst_, NULL);

    // Must explicitly define a tooltip control as topmost
    SetWindowPos(tooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // Associate the tooltip with the tool.
    TOOLINFO toolInfo = { 0 };
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.hwnd = parentWindow;
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.uId = reinterpret_cast<UINT_PTR>(toolWindow);
    toolInfo.lpszText = text;
    SendMessage(tooltip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&toolInfo));

    return tooltip;
}

std::wstring SoundRemoteApp::loadStringResource(UINT resourceId) {
    const WCHAR* unterminatedString = nullptr;
    const auto stringLength = LoadStringW(hInst_, resourceId, (LPWSTR)&unterminatedString, 0);
    return { unterminatedString, static_cast<size_t>(stringLength) };
}

void SoundRemoteApp::onDeviceSelect() {
// Get selected item's deviceKey
    const auto itemIndex = ComboBox_GetCurSel(deviceComboBox_);
    if (CB_ERR == itemIndex) { return; }
    const auto itemData = ComboBox_GetItemData(deviceComboBox_, itemIndex);
    const int deviceKey = static_cast<int>(itemData);

// Get device id string
    const std::wstring deviceId = getDeviceId(deviceKey);

// Change capture device
    boost::asio::post(ioContext_, std::bind(&SoundRemoteApp::changeCaptureDevice, this, deviceId));

    startPeakMeter();
}

void SoundRemoteApp::changeCaptureDevice(const std::wstring& deviceId) {
    if (currentDeviceId_ == deviceId) {
        return;
    }
    currentDeviceId_.clear();
    stopCapture();
    capturePipe_ = std::make_unique<CapturePipe>(deviceId, server_, ioContext_);
    clients_->addClientsListener(std::bind(&CapturePipe::onClientsUpdate, capturePipe_.get(), _1));
    currentDeviceId_ = deviceId;
    capturePipe_->start();
}

void SoundRemoteApp::stopCapture() {
    if (capturePipe_) {
        auto removed = clients_->removeClientsListener(
            std::bind(&CapturePipe::onClientsUpdate, capturePipe_.get(), _1)
        );
        if (removed != 1) {
            throw std::runtime_error(
                Util::makeFatalErrorText(ErrorCode::REMOVE_CAPTURE_PIPE_CLIENTS_LISTENER)
            );
        }
        capturePipe_.reset();
    }
}

void SoundRemoteApp::onClientListUpdate(std::forward_list<std::string> clients) {
    std::ostringstream addresses;
    for (const auto& client : clients) {
        addresses << client << "\r\n";
    }
    SetWindowTextA(clientsList_, addresses.str().c_str());
}

void SoundRemoteApp::onClientsUpdate(std::forward_list<ClientInfo> clients) {
    std::ostringstream addresses;
    for (auto&& client : clients) {
        addresses << client.address.to_string() << "\r\n";
    }
    SetWindowTextA(clientsList_, addresses.str().c_str());
}

void SoundRemoteApp::onAddressButtonClick() const {
    auto addresses = Net::getLocalAddresses();
    std::wstring addressesStr;
    for (auto& adr : addresses) {
        addressesStr += adr + L"\n";
    }
    Util::showInfo(addressesStr, serverAddressesLabel_);
}

void SoundRemoteApp::updatePeakMeter() {
    if (capturePipe_) {
        auto peakValue = capturePipe_->getPeakValue();
        const int peak = static_cast<int>(peakValue * 100);
        SendMessage(peakMeterProgress_, PBM_SETPOS, peak, 0);
    } else {
        stopPeakMeter();
    }
}

void SoundRemoteApp::onReceiveKeystroke(const Keystroke& keystroke) {
    auto currentTextLength = Edit_GetTextLength(keystrokes_);
    Edit_SetSel(keystrokes_, currentTextLength, currentTextLength);
    tm now;
    const auto t = time(nullptr);
    localtime_s(&now, &t);
    wchar_t timeStr[20];
    wcsftime(timeStr, sizeof(timeStr), L"%T%t", &now);
    std::wstring keystrokeDesc = timeStr + keystroke.toString() + L"\r\n";
    Edit_ReplaceSel(keystrokes_, keystrokeDesc.c_str());
}

void SoundRemoteApp::checkUpdates(bool quiet) {
    if (!updateChecker_) {
        updateChecker_ = std::make_unique<UpdateChecker>(mainWindow_);
    }
    updateChecker_->checkUpdates(quiet);
}

void SoundRemoteApp::onUpdateCheckFinish(WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
    case UPDATE_FOUND:
        if (IDYES == MessageBox(
            mainWindow_,
            updateCheckFound_.c_str(),
            updateCheckTitle_.c_str(),
            MB_ICONINFORMATION | MB_YESNO
        )) {
            ShellExecute(
                nullptr,
                TEXT("open"),
                TEXT("https://github.com/SoundRemote/server-windows/releases"),
                nullptr,
                nullptr,
                SW_NORMAL
            );
        }
        return;
    case UPDATE_NOT_FOUND:
        Util::showInfo(updateCheckNotFound_, updateCheckTitle_);
        return;
    case UPDATE_CHECK_ERROR:
        Util::showInfo(updateCheckError_, updateCheckTitle_);
        return;
    }
}

void SoundRemoteApp::visitHomepage() const {
    ShellExecute(
        nullptr,
        TEXT("open"),
        TEXT("https://soundremote.github.io"),
        nullptr,
        nullptr,
        SW_NORMAL
    );
}

void SoundRemoteApp::asioEventLoop(boost::asio::io_context& ctx) {
    for (;;) {
        try {
            ctx.run();
            break;
        }
        catch (const Audio::Error& e) {
            Util::showError(e.what());
            stopCapture();
        }
        catch (const std::exception& e) {
            //logger.log(LOG_ERR) << "[eventloop] An unexpected error occurred running " << name << " task: " << e.what();
            Util::showError(e.what());
            std::exit(EXIT_FAILURE);
        }
        catch (...) {
            Util::showError("Event loop: unknown error");
            std::exit(EXIT_FAILURE);
        }
    }
}

void SoundRemoteApp::initInterface(HWND hWndParent) {
    RECT wndRect;
    GetClientRect(hWndParent, &wndRect);
    const int windowW = wndRect.right;
    const int windowH = wndRect.bottom;

    auto const charH = getCharHeight(hWndParent);
    constexpr int padding = 5;
    constexpr int rightBlockW = 30;
    const int leftBlockW = windowW - rightBlockW - padding * 3;

// Device combobox
    const int deviceComboX = padding;
    const int deviceComboY = padding;
    const int deviceComboW = windowW - padding * 2;
    const int deviceComboH = 100;
    deviceComboBox_ = CreateWindowW(WC_COMBOBOX, (LPCWSTR)NULL, CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
        deviceComboX, deviceComboY, deviceComboW, deviceComboH, hWndParent, NULL, hInst_, NULL);

    RECT deviceComboRect;
    GetClientRect(deviceComboBox_, &deviceComboRect);
    MapWindowPoints(deviceComboBox_, hWndParent, (LPPOINT)&deviceComboRect, 2);
    
// Clients label
    const int clientsLabelX = padding;
    const int clientsLabelY = deviceComboRect.bottom + padding;
    const int clientsLabelW = leftBlockW;
    const int clientsLabelH = charH;
    HWND clientsLabel = CreateWindow(WC_STATIC, clientListLabel_.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT,
        clientsLabelX, clientsLabelY, clientsLabelW, clientsLabelH, hWndParent, NULL, hInst_, NULL);

// Clients
    const int clientListX = padding;
    const int clientListY = clientsLabelY + clientsLabelH + padding;
    const int clientListW = leftBlockW;
    const int clientListH = 60;
    clientsList_ = CreateWindow(WC_EDIT, (LPCWSTR)NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY,
        clientListX, clientListY, clientListW, clientListH, hWndParent, NULL, hInst_, NULL);

// Keystrokes label
    const int keystrokesLabelX = padding;
    const int keystrokesLabelY = clientListY + clientListH + padding;
    const int keystrokesLabelW = leftBlockW;
    const int keystrokesLabelH = charH;
    HWND keystrokesLabel = CreateWindow(WC_STATIC, keystrokeListLabel_.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT,
        keystrokesLabelX, keystrokesLabelY, keystrokesLabelW, keystrokesLabelH, hWndParent, NULL, hInst_, NULL);

// Keystrokes
    const int keystrokesX = padding;
    const int keystrokesY = keystrokesLabelY + keystrokesLabelH + padding;
    const int keystrokesW = leftBlockW;
    const int keystrokesH = windowH - keystrokesY - padding;
    keystrokes_ = CreateWindow(WC_EDIT, (LPCWSTR)NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY,
        keystrokesX, keystrokesY, keystrokesW, keystrokesH, hWndParent, NULL, hInst_, NULL);

// Address button
    const int addressButtonX = windowW - rightBlockW - padding;
    const int addressButtonY = deviceComboRect.bottom + padding;
    const int addressButtonW = rightBlockW;
    const int addressButtonH = rightBlockW;
    addressButton_ = CreateWindowW(WC_BUTTON, L"IP", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        addressButtonX, addressButtonY, addressButtonW, addressButtonH, hWndParent, NULL, hInst_, NULL);
    setTooltip(addressButton_, serverAddressesLabel_.data(), hWndParent);

// Mute button
    Rect muteButtonRect = Rect(addressButtonX, windowH - rightBlockW - padding, rightBlockW, rightBlockW);
    muteButton_ = std::make_unique<MuteButton>(hWndParent, muteButtonRect, muteButtonText_);
    muteButton_->setStateCallback([&](bool v) { capturePipe_->setMuted(v); });

// Peak meter
    const int peakMeterX = addressButtonX;
    const int peakMeterY = addressButtonY + addressButtonH + padding;
    const int peakMeterW = rightBlockW;
    const int peakMeterH = muteButtonRect.y - peakMeterY - padding;
    peakMeterProgress_ = CreateWindowW(PROGRESS_CLASS, (LPCWSTR)NULL, WS_CHILD | WS_VISIBLE | PBS_VERTICAL | PBS_SMOOTH,
        peakMeterX, peakMeterY, peakMeterW, peakMeterH, hWndParent, NULL, hInst_, NULL);
}

void SoundRemoteApp::initControls() {
    //Device ComboBox
    ComboBox_ResetContent(deviceComboBox_);
    deviceIds_.clear();
    addDevices(deviceComboBox_, eRender);
    addDevices(deviceComboBox_, eCapture);
    ComboBox_SetCurSel(deviceComboBox_, 0);
}

void SoundRemoteApp::startPeakMeter() {
    SetTimer(mainWindow_, timerIdPeakMeter, timerPeriodPeakMeter, nullptr);
}

void SoundRemoteApp::stopPeakMeter() {
    KillTimer(mainWindow_, timerIdPeakMeter);
    SendMessage(peakMeterProgress_, PBM_SETPOS, 0, 0);
}

void SoundRemoteApp::initSettings() {
    settings_ = std::make_unique<Settings>("settings.ini");
}

void SoundRemoteApp::initMenu() {
    HMENU menu = GetMenu(mainWindow_);
    MENUITEMINFO mii{ sizeof(MENUITEMINFO) };
    mii.fMask = MIIM_STATE;
    if (settings_->getCheckUpdates()) {
        mii.fState = MFS_CHECKED;
    } else {
        mii.fState = MFS_UNCHECKED;
    }
    SetMenuItemInfo(menu, IDM_CHECK_UPDATES_ON_START, FALSE, &mii);
}

void SoundRemoteApp::initStrings() {
    mainWindowTitle_ = loadStringResource(IDS_APP_TITLE);
    serverAddressesLabel_ = loadStringResource(IDS_SERVER_ADDRESSES);
    defaultRenderDeviceLabel_ = loadStringResource(IDS_DEFAULT_RENDER);
    defaultCaptureDeviceLabel_ = loadStringResource(IDS_DEFAULT_CAPTURE);
    clientListLabel_ = loadStringResource(IDS_CLIENTS);
    keystrokeListLabel_ = loadStringResource(IDS_HOTKEYS);
    muteButtonText_ = loadStringResource(IDS_MUTE);
    updateCheckTitle_ = loadStringResource(IDS_UPDATE_CHECK);
    updateCheckFound_ = loadStringResource(IDS_UPDATE_FOUND);
    updateCheckNotFound_ = loadStringResource(IDS_UPDATE_NOT_FOUND);
    updateCheckError_ = loadStringResource(IDS_UPDATE_CHECK_ERROR);
}

bool SoundRemoteApp::initInstance(int nCmdShow) {
    constexpr wchar_t CLASS_NAME[] = L"SOUNDREMOTE";

    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = staticWndProc;
    wcex.hInstance = hInst_;
    wcex.hIcon = LoadIcon(hInst_, MAKEINTRESOURCE(IDI_SOUNDREMOTE));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SOUNDREMOTE);
    wcex.lpszClassName = CLASS_NAME;
    wcex.hIconSm = nullptr;
    if (RegisterClassExW(&wcex) == 0) {
        return false;
    }

    initStrings();

    mainWindow_ = CreateWindowW(CLASS_NAME, mainWindowTitle_.data(), WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, 0, windowWidth, windowHeight, nullptr, nullptr, hInst_, this);
    if (mainWindow_ == NULL) {
        return false;
    }

    initInterface(mainWindow_);
    initControls();

    ShowWindow(mainWindow_, nCmdShow);
    return true;
}

INT_PTR SoundRemoteApp::about(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

LRESULT SoundRemoteApp::staticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SoundRemoteApp* app = nullptr;
    if (message == WM_CREATE) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        app = static_cast<SoundRemoteApp*>(lpcs->lpCreateParams);
        app->mainWindow_ = hWnd;
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<SoundRemoteApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }
    if (app) {
        return app->wndProc(message, wParam, lParam);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT SoundRemoteApp::wndProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
    case WM_COMMAND:
    {
        const int wmType = HIWORD(wParam);
        const int wmId = LOWORD(wParam);
        if (lParam == 0) {  // If Menu or Accelerator
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_ABOUTBOX), mainWindow_, about);
                return 0;

            case IDM_EXIT:
                DestroyWindow(mainWindow_);
                return 0;

            case IDM_CHECK_UPDATES:
                checkUpdates();
                return 0;

            case IDM_HOMEPAGE:
                visitHomepage();
                return 0;

            case IDM_CHECK_UPDATES_ON_START: {
                bool checked = toggleMenuItem(IDM_CHECK_UPDATES_ON_START);
                settings_->setCheckUpdates(checked);
                return 0;
            }

            default:
                break;
            }
        } else {    // If Control
            const HWND controlHwnd = reinterpret_cast<HWND>(lParam);
            switch (wmType)
            {
            case CBN_SELCHANGE:
                // The only combobox is device select.
                onDeviceSelect();
                return 0;

            case BN_CLICKED: {
                if (controlHwnd == addressButton_) {
                    onAddressButtonClick();
                    return 0;
                }
                if (controlHwnd == muteButton_->handle()) {
                    muteButton_->onClick();
                    return 0;
                }
            }
            break;

            default:    // Other controls
                break;
            }
        }
    }
    break;

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE) {
            DestroyWindow(mainWindow_);
            return 0;
        }
    break;

    // Ignore WM_CLOSE because multiline edit sends it on Esc.
    case WM_CLOSE:
        return 0;

    case WM_TIMER:
    {
        switch ((int)wParam) {
        case timerIdPeakMeter:
            updatePeakMeter();
            return 0;

        default:
            break;
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(mainWindow_, &ps);
        // TODO: Add any drawing code that uses hdc here...
        EndPaint(mainWindow_, &ps);
    }
    return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_UPDATE_CHECK:
        onUpdateCheckFinish(wParam, lParam);
        return 0;

    case WM_POWERBROADCAST:
    {
        if (PBT_APMSUSPEND == wParam) {
            clients_->removeAll();
        }
    }
    break;

    default:
        break;
    }
    return DefWindowProc(mainWindow_, message, wParam, lParam);
}
