# SoundRemote server
Desktop app that pairs up with Android device via [SoundRemote client](https://github.com/ashipo/SoundRemote-android) to:
* Capture and send audio to the client device.
* Emulate keyboard shortcuts received from the client. Certain shortcuts like `Ctrl + Alt + Delete` or `Win + L` aren't currently supported.

![Main window](https://github.com/ashipo/SoundRemote-server/assets/24320267/9d3bf544-05eb-46b4-a1f4-8c89917c8913 "Main window")

## Build
Prerequisites:
 - Microsoft Visual Studio 2022.
 - [Opus audio codec](https://github.com/xiph/opus) library.
Build `opus.lib` and put it in `$(SolutionDir)\lib\opus\x64` or `$(SolutionDir)\lib\opus\Win32`.
 - [Boost](https://www.boost.org/) has to be accessible by `BOOST_ROOT` environment variable.
It must point to the Boost root directory, for example `C:\Program Files\boost\boost_1_82_0`.

## Testing
Tests are implemented with GoogleTest. To run tests install the [gmock](https://www.nuget.org/packages/gmock/) NuGet package from Google.
