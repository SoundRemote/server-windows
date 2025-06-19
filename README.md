# SoundRemote server
Desktop app that pairs up with Android device via [SoundRemote client](https://github.com/SoundRemote/client-android) to:
* Capture and send audio to the client device.
* Emulate keyboard shortcuts received from the client. Certain shortcuts like `Ctrl + Alt + Delete` or `Win + L` aren't currently supported.

![Main window](https://github.com/user-attachments/assets/1b86c980-132d-4661-87ed-dbe3dd670a8a "Main window")

## Build
Prerequisites:
 - Microsoft Visual Studio 2022.
 - [Opus audio codec](https://github.com/xiph/opus) library.
Build `opus.lib` and put it in `$(SolutionDir)\lib\opus\x64` or `$(SolutionDir)\lib\opus\Win32`.
 - [Boost](https://www.boost.org/) has to be accessible by `BOOST_ROOT` environment variable.
It must point to the Boost root directory, for example `C:\Program Files\boost\boost_1_82_0`.

## Testing
Tests are implemented with GoogleTest. To run tests install the [gmock](https://www.nuget.org/packages/gmock/) NuGet package from Google.
