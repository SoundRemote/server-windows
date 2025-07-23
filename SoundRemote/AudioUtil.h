#pragma once

#include <mmdeviceapi.h>

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "Util.h"

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }

#define THROW_ON_ERROR(hres, location)  \
            if (FAILED(hres)) { \
                hr = hres; \
                where = location; \
                goto Exit; }

#define SAFE_RELEASE(punk)  \
              if ((punk) != nullptr)  \
                { (punk)->Release(); (punk) = nullptr; }

namespace Audio {
// Constants, types, enums

	constexpr auto defaultRenderDeviceKey = -1;
	constexpr auto defaultCaptureDeviceKey = -2;

	enum class Compression { none = 0, kbps_64 = 64'000, kbps_128 = 128'000, kbps_192 = 192'000, kbps_256 = 256'000, kbps_320 = 320'000 };

	namespace Opus {
		// Supported sample rates
		enum class SampleRate { khz_8 = 8'000, khz_12 = 12'000, khz_16 = 16'000, khz_24 = 24'000, khz_48 = 48'000 };
		// Supported channels
		enum class Channels { mono = 1, stereo = 2 };
		// Opus frame length in ms. Opus can encode frames of 2.5, 5, 10, 20, 40, or 60 ms. This value determines Opus frame size.
		// At 48 kHz the permitted values of Opus frame size are 120(2.5ms), 240(5ms), 480(10ms), 960(20ms), 1920(40ms), and 2880(60ms).
		constexpr int frameLength = 10;
		// Maximum Opus packet size in bytes.
		constexpr int maxPacketSize = 2 * static_cast<int>(Compression::kbps_320) * frameLength / (1000 * 8);
	}

	enum class SampleType {
		Unknown = 0,
		SignedInt = 1,
		UnSignedInt = 2,
		Float = 3
	};

	enum class Location {
		NOWHERE = 0,
		CAPTURE_COINITIALIZE = 1,
		CAPTURE_COCREATEINSTANCE = 2,
		CAPTURE_GETDEVICE = 3,
		CAPTURE_ACTIVATE_AUDIOCLIENT = 4,
		CAPTURE_QUERY_ENDPOINT = 5,
		CAPTURE_ENDPOINT_GETDATAFLOW = 6,
		CAPTURE_AC_ISFORMATSUPPORTED = 7,
		CAPTURE_AC_GETMIXFORMAT = 8,
		CAPTURE_AC_INITIALIZE_RENDER = 9,
		CAPTURE_AC_INITIALIZE_CAPTURE = 10,
		CAPTURE_MEMALLOC = 11,
		CAPTURE_AC_GETBUFFERSIZE = 12,
		CAPTURE_AC_GETSERVICE = 13,
		CAPTURE_AC_START = 20,
		CAPTURE_AC_STOP = 21,
		CAPTURE_ACC_GETNEXTPACKETSIZE = 22,
		CAPTURE_ACC_GETBUFFER = 23,
		CAPTURE_ACC_RELEASEBUFFER = 24,
		CAPTURE_ACTIVATE_METERINFO = 25,

		RESAMPLER_COCREATEINSTANCE = 101,
		RESAMPLER_QUERY_TRANSFORM = 102,
		RESAMPLER_QUERY_PROPS = 103,
		RESAMPLER_PROPS_SETHALFFILTERLENGTH = 104,
		RESAMPLER_CREATEMEDIATYPE_INPUT = 105,
		RESAMPLER_INITMEDIATYPE_INPUT = 106,
		RESAMPLER_TRANSFORM_SETINPUTTYPE = 107,
		RESAMPLER_CREATEMEDIATYPE_OUTPUT = 108,
		RESAMPLER_INITMEDIATYPE_OUTPUT = 109,
		RESAMPLER_TRANSFORM_SETOUTPUTTYPE = 110,
		RESAMPLER_TRANSFORM_MESSAGE_FLUSH = 111,
		RESAMPLER_TRANSFORM_MESSAGE_BEGIN = 112,
		RESAMPLER_TRANSFORM_MESSAGE_START = 113,
		RESAMPLER_CREATE_INPUT_BUFFER = 114,
		RESAMPLER_INPUT_BUFFER_LOCK = 115,
		RESAMPLER_INPUT_BUFFER_UNLOCK = 116,
		RESAMPLER_INPUT_BUFFER_SETLENGTH = 117,
		RESAMPLER_CREATE_INPUT_SAMPLE = 118,
		RESAMPLER_INPUT_SAMPLE_ADDBUFFER = 119,
		RESAMPLER_TRANSFORM_PROCESSINPUT = 120,
		RESAMPLER_TRANSFORM_GETOUTPUTSTREAMINFO = 121,
		RESAMPLER_CREATE_OUTPUT_BUFFER = 122,
		RESAMPLER_CREATE_OUTPUT_SAMPLE = 123,
		RESAMPLER_OUTPUT_SAMPLE_ADDBUFFER = 124,
		RESAMPLER_TRANSFORM_PROCESSOUTPUT = 125,
		RESAMPLER_OUTPUT_SAMPLE_CONVERT = 126,
		RESAMPLER_PROCESSED_BUFFER_GETCURRENTLENGTH = 127,
		RESAMPLER_PROCESSED_BUFFER_LOCK = 128,
		RESAMPLER_PROCESSED_BUFFER_UNLOCK = 129,

		ENCODER_CREATE = 202,
		ENCODER_SET_BITRATE= 203,
		ENCODER_ENCODE = 204,

		UTIL_GETDEVICES_COINITIALIZE = 301,
		UTIL_GETDEVICES_CREATE_ENUMERATOR = 302,
		UTIL_GETDEVICES_ENUMERATOR_ENUMENDPOINTS = 303,
		UTIL_GETDEVICES_ENDPOINTS_GETCOUNT = 304,
		UTIL_GETDEVICES_DEVICES_ITEM = 305,
		UTIL_GETDEVICES_DEVICE_GETID = 306,
		UTIL_GETDEVICES_DEVICE_OPENPROPERTYSTORE = 307,
		UTIL_GETDEVICES_PROPS_GETVALUE = 308,
		UTIL_GETDEFAULTDEVICE_COINITIALIZE = 310,
		UTIL_GETDEFAULTDEVICE_CREATE_ENUMERATOR = 311,
		UTIL_GETDEFAULTDEVICE_ENUMERATOR_GETDEFAULTENDPOINT = 312,
		UTIL_GETDEFAULTDEVICE_DEVICE_GETID = 313
	};

// Data classes and structs

	struct Format {
		int sampleRate = 48000;
		int channelCount = 2;
		int sampleSize = 16;
		SampleType sampleType = SampleType::SignedInt;
		Util::Endian byteOrder = Util::Endian::Little;
	};

// Utility classes and structs

	class Error : public std::runtime_error {
	public:
		Error(const std::string& what) : std::runtime_error(what) {};
	};

	// Function object to be used as a deleter with std::unique_ptr to the objects requiring CoTaskMemFree.
	// std::unique_ptr<tWAVEFORMATEX, Audio::CoDeleter<tWAVEFORMATEX>> waveFormat;
	template<typename T>
	struct CoDeleter {
		void operator()(T* var) const { CoTaskMemFree(var); }
	};

	/// <summary>
	/// Calls <code>CoUninitialize()</code> in dtor.
	/// </summary>
	struct CoUninitializer {
		~CoUninitializer() { CoUninitialize(); };
	};

// Functions

	std::unordered_map<std::wstring, std::wstring> getEndpointDevices(const EDataFlow dataFlow);

	/// <summary>
	/// Gets default device id string.
	/// </summary>
	/// <param name="flow">EDataFlow::eCapture or EDataFlow::eRender.</param>
	/// <returns>Device id string.</returns>
	std::wstring getDefaultDevice(EDataFlow flow);

	void throwOnError(const HRESULT hr, Location where);
	/// <summary>
	/// Shows error message and terminates the app if passed <code>HRESULT</code> is an error.
	/// </summary>
	/// <param name="hr">operation result to check.</param>
	/// <param name="where">describes error location.</param>
	void exitOnError(const HRESULT hr, Location where);
	void processError(const long errorCode, Location where);
	std::string audioErrorText(const HRESULT errorCode, Location where);
}
