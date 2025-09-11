# DxDispatch

DxDispatch is simple command-line executable for launching DirectX 12 compute programs without writing all the C++ boilerplate. The input to the tool is a JSON model that defines resources, dispatchables (compute shaders, DirectML operators, ONNX models), and commands to execute. The model abstraction makes it easy to experiment, but it also preserves low-level control and flexibility.

Some of the things you can do with this tool:
- Launch [DirectML](https://github.com/Microsoft/directml) operators to understand how they work with different inputs.
- Run custom HLSL compute shaders that are compiled at runtime using the [DirectX Shader Compiler](https://github.com/Microsoft/DirectXShaderCompiler).
- Run ONNX models using ONNX Runtime with the DirectML execution provider.
- Debug binding and API usage issues. DxDispatch hooks into the Direct3D and DirectML debug layers and prints errors and warnings directly to the console; no need to attach a debugger.
- Experiment with performance by benchmarking dispatches.
- Take GPU or timing captures using [PIX on Windows](https://devblogs.microsoft.com/pix/download/). Labeled events and resources make it easy to correlate model objects to D3D objects.

This tool is *not* designed to be a general-purpose framework for building large computational models or running in production scenarios. The focus is on experimentation and learning!

# Getting Started

See the [guide](doc/Guide.md) for detailed usage instructions. The [models](./models) directory contains some simple examples to get started. For example, here's an example that invokes DML's reduction operator:

```
> dxdispatch.exe models/dml_reduce.json

Running on 'NVIDIA GeForce RTX 2070 SUPER'
Resource 'input': 1, 2, 3, 4, 5, 6, 7, 8, 9
Resource 'output': 6, 15, 24
```

# System Requirements

The exact system requirements vary depending on how you configure and run DxDispatch. The default builds rely on redistributable versions of DirectX components when possible, which provides the latest features to the widest range of systems. For default builds of DxDispatch you should consider the following as the minimum system requirements across the range of platforms:

- A DirectX 12 capable hardware device.
- Windows 10 November 2019 Update (Version 1909; Build 18363) or newer.
- If testing shaders that use Shader Model 6.6:
  - AMD: [Adrenalin 21.4.1 preview driver](https://www.amd.com/en/support/kb/release-notes/rn-rad-win-21-4-1-dx12-agility-sdk)
  - NVIDIA: drivers with version 466.11 or higher

# Building, Testing, and Installing

DxDispatch relies on several external dependencies that are downloaded when the project is configured. See [ThirdPartyNotices.txt](./ThirdPartyNotices.txt) for relevant license info.

Configure presets are listed configuration in [CMakePresets.json](CMakePresets.json):
```
> cmake --list-presets
Available configure presets:

  "win-x64"       - Windows x64
  "win-x86"       - Windows x86
  "win-arm64"     - Windows ARM64
  "xbox-scarlett" - Xbox Scarlett
  "linux-x64"     - Linux x64
```

To generate the project, provide one of the above names (e.g. `win-x64`) to cmake:
```
> cmake --preset <configure_preset_name>
```

You can build from the generated VS solution under `build\<configure_preset_name>\dxdispatch.sln`. 

Alternatively, build from the command line by using `--build` option and appending the build configuration to the preset name (e.g. the `win-x64` configure preset has the build presets named `win-x64-release` and `win-x64-debug`).

```
> cmake --build --preset <configure_preset_name>-(release|debug)
```

To run tests, change your working directory to the build folder and execute `ctest` (only supported on some platforms). You need to specify the build configuration (relwithdebinfo or debug) since the presets use VS, which is a multi-configuration generator:
```
> cd build\<configure_preset_name>

# Test release config
> ctest -C RelWithDebInfo .

# Test debug config
> ctest -C Debug .
```

# Build Configuration

DxDispatch tries to depend on pre-built redistributable versions of its external dependencies. However, the build can be configured to use alternative sources when desired or necessary. Each component can use one of the available (✅) sources in the table below, with the <b><u>default</u></b> selection for each platform listed first. Not all configurations are tested, and some platforms don't include the optional<sup>+</sup> components.

<table>
  <tr>
    <th>Preset</th>
    <th><a href="https://docs.microsoft.com/windows/ai/directml/dml-intro">DirectML</a></th>
    <th><a href="https://docs.microsoft.com/windows/win32/direct3d12/what-is-directx-12-">Direct3D 12</a></th>
    <th><a href="https://github.com/microsoft/DirectXShaderCompiler">DX Compiler</a><sup>+</sup></th>
    <th><a href="https://devblogs.microsoft.com/pix/winpixeventruntime/">PIX Event Runtime</a><sup>+</sup></th>
    <th><a href="https://onnxruntime.ai/">ONNX Runtime</a><sup>+</sup></th>
  </tr>
  <tr>
    <td>win-x64</td>
    <td><b>✅ <u>nuget</u></b><br>✅ winsdk<br>✅ local</td>
    <td><b>✅ <u>nuget</u></b><br>✅ winsdk</td>
    <td><b>✅ <u>archive</u></b></td>
    <td><b>✅ <u>nuget</u></b></td>
    <td><b>✅ <u>nuget</u></b></td>
  </tr>
  <tr>
    <td>win-x86</td>
    <td><b>✅ <u>nuget</u></b><br>✅ winsdk<br>✅ local</td>
    <td><b>✅ <u>nuget</u></b><br>✅ winsdk</td>
    <td>❌ none</td>
    <td>❌ none</td>
    <td><b>✅ <u>nuget</u></b></td>
  </tr>
  <tr>
    <td>win-arm64</td>
    <td><b>✅ <u>nuget</u></b><br>✅ winsdk<br>✅ local</td>
    <td><b>✅ <u>nuget</u></b><br>✅ winsdk</td>
    <td><b>✅ <u>archive</u></b></td>
    <td><b>✅ <u>nuget</u></b></td>
    <td><b>✅ <u>nuget</u></b></td>
  </tr>
  <tr>
    <td>linux-x64</td>
    <td><b>✅ <u>nuget</u></b><br>✅ local</td>
    <td><b>✅ <u>wsl</u></b></td>
    <td>❌ none</td>
    <td>❌ none</td>
    <td>❌ none</td>
  </tr>
  <tr>
    <td>xbox-scarlett</td>
    <td><b>✅ <u>nuget</u></b><br>✅ local</td>
    <td><b>✅ <u>gdk</u></b></td>
    <td><b>✅ <u>gdk</u></b></td>
    <td><b>✅ <u>gdk</u></b></td>
    <td><b>✅ <u>local</u></b></td>
  </tr>
</table>

Refer to the respective CMake files ([directml.cmake](cmake/directml.cmake), [d3d12.cmake](cmake/d3d12.cmake), [dxcompiler.cmake](cmake/dxcompiler.cmake), [pix.cmake](cmake/pix.cmake), [onnxruntime.cmake](cmake/onnxruntime.cmake)) for descriptions of the CMake cache variables that can be set to change the build configuration. CMake cache variables persist, so make sure to reconfigure or delete your build directory when changing variables.

# Custom Build for Xbox Scarlett

The custom build for Xbox Scarlett only support HLSL dispatchables, no DML ops, no ONNX model dispatchables. To configure the CMake build, do not use the preset `cmake --preset xbox-scarlett`, use the following build configure command followed by the build commands as follows:

```
> cmake . -B build -A Gaming.Xbox.Scarlett.x64 -DDXD_ONNXRUNTIME_TYPE=none -DDXD_GDK_SYSTEM_EDITION=250401 -G "Visual Studio 17 2022"

> cmake --build build --config [Debug | Release]
```

The GDK Edition in this example is set to `250401` which indicates the *April 2025 Update 1* greensigned release of the Xbox GDK found at `\\edge-svcs\drops\greensignedpackages\2025_04_Update1__GDK`

## CMake Pre-Build Failures
If you run into an error during the CMake pre-build steps, make sure to add **Game development with C++** component in VS 2022 installation, then reinstalling the GDK *after that VS installation/modification*, and that should fix it.

## Set Up the Scarlett Dev Kit
Follow steps outlined in [this document](https://microsoft.visualstudio.com/WindowsAI/_git/dmldocs?path=/Xbox/Docs/devkit_setup.md&_a=preview) to configure a Scarlett devkit.

## Run DxDispatch in the Dev Kit
After the devkit is up and running, use the following commands to work with it.

```
> xbconnect

Connections at 192.168.1.116, client build 10.0.26100.4046:
       HOST: 10.0.26100.5362 (Host OS) @192.168.1.115
     SYSTEM: 10.0.26100.5362 (System OS) @192.168.1.116
      TITLE: Not running.

Default console set to "192.168.1.116" (192.168.1.116)
```

If the default console hasn't been set, run **Xbox Manager GDK** to set it up.

<img src="doc/images/Xbox%20Manager.jpg" alt="Xbox Manager" width="75%">

All the Xbox cmd tools *(xb\*.exe)* are under `\Program Files (x86)\Microsoft GDK\bin` directory, so adding this path as part of the user's path will allow it to be called elsewhere in the terminal or console. 

Copying `dxdispatch.exe` along with all the needed executable files to a dedicated directory in the `xd:` drive called `dxd` in the devkit as follows:

```
> xbdir xd:\

Directory of XD:\

09/08/2022  03:48 PM  D         boot
09/08/2022  03:47 PM  D         Drives
09/08/2022  03:46 PM  D         Temp
10/28/2022  12:23 PM  D         WUDownloadCache

                 0 byte(s)
                 0 file(s)
                 4 dir(s)

> dir
 Volume in drive C is Windows
 Volume Serial Number is 9EF7-1161

 Directory of C:\DirectML.public\DxDispatch\build\Release

09/10/2025  06:43 PM    <DIR>          .
09/10/2025  06:42 PM    <DIR>          ..
07/08/2025  01:45 PM           301,024 concrt140.dll
09/10/2025  06:43 PM        18,988,104 dxcompiler_xs.dll
09/10/2025  06:43 PM            38,400 dxdispatch.exe
09/10/2025  06:43 PM         1,121,792 dxdispatchImpl.dll
09/10/2025  06:43 PM             1,057 dxdispatchImpl.exp
09/10/2025  06:43 PM             2,208 dxdispatchImpl.lib
06/06/2025  01:01 PM       345,976,832 gameos.xvd
09/10/2025  06:43 PM        40,995,152 libdirectml.so
06/06/2025  12:51 PM           245,312 libHttpClient.GDK.dll
09/10/2025  06:43 PM               812 MicrosoftGame.Config
09/10/2025  06:42 PM         6,408,264 model.lib
07/08/2025  01:45 PM           546,312 msvcp140.dll
07/08/2025  01:45 PM            26,088 msvcp140_1.dll
07/08/2025  01:45 PM           270,304 msvcp140_2.dll
07/08/2025  01:45 PM            39,424 msvcp140_atomic_wait.dll
07/08/2025  01:45 PM            21,984 msvcp140_codecvt_ids.dll
07/08/2025  01:45 PM           343,008 vccorlib140.dll
07/08/2025  01:45 PM            72,712 vcomp140.dll
07/08/2025  01:45 PM           113,632 vcruntime140.dll
07/08/2025  01:45 PM            38,368 vcruntime140_1.dll
07/08/2025  01:45 PM            29,168 vcruntime140_threads.dll
06/06/2025  12:51 PM           198,208 XCurl.dll
              22 File(s)    415,778,165 bytes
               2 Dir(s)  157,652,697,088 bytes free

> xbmkdir xd:\dxd

> xbcp . xd:\dxd
```
Copy relevant test files to the same directory in the devkit.
```
> xbcp ..\..\models\hlsl_add_fp32.json xd:\dxd

> xbcp ..\..\models\hlsl_add.hlsl xd:\dxd
```
Test if the Game OS is up and running in the devkit.
```
> xbapp querygameos
Game OS is not running.
Game is not running.
The operation completed successfully.
```
If it's not running, start it. Thanks to the DxDispatch build configuration, the matching version of the Game OS XVD is already there.
```
> xbapp applyconfig xd:\dxd\gameos.xvd
Game OS is starting up. Waiting for the OS to be ready for interaction...
The operation completed successfully.
```
Run `dxdispatch.exe` with the HLSL dispatchable on the devkit as follows. Parameterize dxdispatch to your liking.
```
> xbrun /o /x /title d:\dxd\dxdispatch d:\dxd\hlsl_add_fp32.json -i 2 -r 10 -v 2
Running on 'Xbox'
Initialize 'add': 52.1629 ms
Dispatch 'add': 2 iterations
CPU Timings (Cold) : 1 samples, 0.0128 ms average, 0.0128 ms min, 0.0128 ms median, 0.0128 ms max
GPU Timings (Cold) : 1 samples, 0.0015 ms average, 0.0015 ms min, 0.0015 ms median, 0.0015 ms max
CPU Timings (Hot)  : 1 samples, 0.0031 ms average, 0.0031 ms min, 0.0031 ms median, 0.0031 ms max
GPU Timings (Hot)  : 1 samples, 0.0012 ms average, 0.0012 ms min, 0.0012 ms median, 0.0012 ms max
The timings of each iteration:
iteration 0: 0.0128 ms (CPU), 0.0015 ms (GPU)
iteration 1: 0.0031 ms (CPU), 0.0012 ms (GPU)
Resource 'Out': 2, 7, 6, 11, 2, 7
```