#include "DeviceEventListener.h"

#include "AppMessages.h"

DeviceEventListener::DeviceEventListener(HWND mainWindow) : cRef_(1), mainWindow_(mainWindow) {
}

HRESULT STDMETHODCALLTYPE DeviceEventListener::OnDeviceStateChanged(
    LPCWSTR pwstrDeviceId,
    DWORD dwNewState
) {
    if (dwNewState == DEVICE_STATE_ACTIVE) {
        SendMessage(mainWindow_, AppMessage::DEVICE_ADDED, 0, 0);
    }
    return S_OK;
}

// When a new device is added, OnDeviceStateChanged with DEVICE_STATE_ACTIVE is also called, so
// this event is ignored.
HRESULT STDMETHODCALLTYPE DeviceEventListener::OnDeviceAdded(LPCWSTR pwstrDeviceId) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceEventListener::OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceEventListener::OnDefaultDeviceChanged(
    EDataFlow flow,
    ERole role,
    LPCWSTR pwstrDefaultDeviceId
) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceEventListener::OnPropertyValueChanged(
    LPCWSTR pwstrDeviceId,
    const PROPERTYKEY key
) {
    return S_OK;
}

ULONG STDMETHODCALLTYPE DeviceEventListener::AddRef() {
    return InterlockedIncrement(&cRef_);
}

ULONG STDMETHODCALLTYPE DeviceEventListener::Release() {
    ULONG ulRef = InterlockedDecrement(&cRef_);
    if (0 == ulRef) {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE DeviceEventListener::QueryInterface(REFIID riid, VOID** ppvInterface) {
    if (IID_IUnknown == riid) {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IMMNotificationClient) == riid) {
        AddRef();
        *ppvInterface = (IMMNotificationClient*)this;
    } else {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}
