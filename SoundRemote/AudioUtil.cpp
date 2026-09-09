#include "AudioUtil.h"

#include <atlbase.h>
#include <functiondiscoverykeys_devpkey.h>

#include <sstream>

std::unordered_map<std::wstring, std::wstring> Audio::getEndpointDevices(const EDataFlow dataFlow) {
    HRESULT hr;

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    exitOnError(hr, Location::UTIL_GETDEVICES_COINITIALIZE);
    CoUninitializer coUninitializer;

    CComPtr<IMMDeviceEnumerator> enumerator;
    hr = enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    exitOnError(hr, Location::UTIL_GETDEVICES_CREATE_ENUMERATOR);

    CComPtr<IMMDeviceCollection> devices;
    hr = enumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &devices);
    exitOnError(hr, Location::UTIL_GETDEVICES_ENUMERATOR_ENUMENDPOINTS);

    UINT deviceCount;
    hr = devices->GetCount(&deviceCount);
    exitOnError(hr, Location::UTIL_GETDEVICES_ENDPOINTS_GETCOUNT);

    std::unordered_map<std::wstring, std::wstring> result;
    for (UINT i = 0; i < deviceCount; ++i) {
        CComPtr<IMMDevice> device;
        hr = devices->Item(i, &device);
        exitOnError(hr, Location::UTIL_GETDEVICES_DEVICES_ITEM);

        LPWSTR pDeviceId = nullptr;
        hr = device->GetId(&pDeviceId);
        exitOnError(hr, Location::UTIL_GETDEVICES_DEVICE_GETID);
        std::unique_ptr<WCHAR, decltype(&CoTaskMemFree)> deviceId(pDeviceId, CoTaskMemFree);
        pDeviceId = nullptr;

        CComPtr<IPropertyStore> props;
        hr = device->OpenPropertyStore(STGM_READ, &props);
        exitOnError(hr, Location::UTIL_GETDEVICES_DEVICE_OPENPROPERTYSTORE);

        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = props->GetValue(PKEY_Device_FriendlyName, &varName);
        exitOnError(hr, Location::UTIL_GETDEVICES_PROPS_GETVALUE);

        result[varName.pwszVal] = deviceId.get();
    }
    return result;
}

std::wstring Audio::getDefaultDevice(EDataFlow flow) {
    HRESULT hr;

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    exitOnError(hr, Location::UTIL_GETDEFAULTDEVICE_COINITIALIZE);
    CoUninitializer coUninitializer;

    CComPtr<IMMDeviceEnumerator> enumerator;
    hr = enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    exitOnError(hr, Location::UTIL_GETDEFAULTDEVICE_CREATE_ENUMERATOR);

    CComPtr<IMMDevice> device;
    hr = enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device);
    exitOnError(hr, Location::UTIL_GETDEFAULTDEVICE_ENUMERATOR_GETDEFAULTENDPOINT);

    LPWSTR deviceId = nullptr;
    hr = device->GetId(&deviceId);
    exitOnError(hr, Location::UTIL_GETDEFAULTDEVICE_DEVICE_GETID);

    std::wstring result{ deviceId };
    CoTaskMemFree(deviceId);
    return result;
}

void Audio::throwOnError(const HRESULT hr, Location where) {
    if (FAILED(hr))
        throw Audio::Error(audioErrorText(hr, where));
}

void Audio::exitOnError(const HRESULT hr, Location where) {
    if (FAILED(hr)) {
        Util::showError(audioErrorText(hr, where));
        exit(EXIT_FAILURE);
    }
}

void Audio::processError(const long errorCode, Location where) {
    throw Audio::Error(audioErrorText(errorCode, where));
}

std::string Audio::audioErrorText(const HRESULT errorCode, Location where) {
    // Special cases
    if (Location::CAPTURE_AC_INITIALIZE_CAPTURE == where && E_ACCESSDENIED == errorCode) {
        return "Microphone access denied. You can change this in the system privacy settings.";
    }
    std::ostringstream ss;
    ss << "Audio capture error " << static_cast<int>(where) << ". [" << std::hex << std::showbase
        << errorCode << ']';
    return ss.str();
}
