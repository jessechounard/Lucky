# Lucky

A C++ game framework built on SDL3 and SDL_GPU.

## Building

### Prerequisites

- **Visual Studio 2022** with the v143 toolset.
- **`shadercross`** must be on your `PATH`. Lucky's HLSL shaders are
  cross-compiled to SPIR-V, DXIL, MSL, and JSON reflection at build time
  using the [`shadercross`](https://github.com/libsdl-org/SDL_shadercross)
  command-line tool. The easiest way to obtain a Windows binary is from the
  unofficial nightly build mirror at
  [nightly.link](https://nightly.link/libsdl-org/SDL_shadercross/workflows/main/main?preview).
  Download an artifact, extract `shadercross.exe`, and place it in any
  directory on your `PATH`.

### Building Lucky

1. Clone with submodules:
   ```
   git clone --recurse-submodules https://github.com/jessechounard/Lucky.git
   ```

2. Open `ProjectFiles/Lucky.sln` in Visual Studio 2022 and build.

The static library (`Lucky.lib`) is written to `Build/Output/<Configuration>/`,
and the compiled shader artifacts to
`Build/Output/<Configuration>/Content/Shaders/`. Consumer projects link
against `Lucky.lib` and copy the `Content/Shaders/` directory into their own
output as a post-build step.

## Running tests

The `Lucky.Tests` project is a console application that runs the unit tests
using [doctest](https://github.com/doctest/doctest). After building the
solution, run:

```
Build\Output\Debug\Lucky.Tests.exe
```

You can also set `Lucky.Tests` as the startup project in Visual Studio and run
it with `F5`. Pass `--help` to see doctest's command-line options (filtering
by test name, listing tests, etc.).

## Dependencies

All dependencies are included as git submodules in `Dependencies/`:

- [SDL](https://github.com/libsdl-org/SDL) (zlib)
- [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross) (zlib)
- [spdlog](https://github.com/gabime/spdlog) (MIT)
- [HarfBuzz](https://github.com/harfbuzz/harfbuzz) (Old MIT)
- [GLM](https://github.com/g-truc/glm) (MIT)
- [nlohmann/json](https://github.com/nlohmann/json) (MIT)
- [dr_libs](https://github.com/mackron/dr_libs) (Public Domain / MIT-0)
- [stb](https://github.com/nothings/stb) (MIT / Public Domain)
- [doctest](https://github.com/doctest/doctest) (MIT) - test only
- [Tracy](https://github.com/wolfpld/tracy) (BSD 3-Clause) - Profile build only
- [SquirrelNoise5](Dependencies/SquirrelNoise5.hpp) (CC-BY-3.0 US) - vendored

See [Dependencies/LICENSES.md](Dependencies/LICENSES.md) for full license texts.

## License

MIT License. See [LICENSE.md](LICENSE.md).
