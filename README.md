# Lucky

A C++ game framework built on SDL3 and SDL_GPU.

## Building

Requires Visual Studio 2022 (v143 toolset).

1. Clone with submodules:
   ```
   git clone --recurse-submodules https://github.com/jessechounard/Lucky.git
   ```

2. Open `ProjectFiles/Lucky.sln` in Visual Studio 2022 and build.

The output is a static library (`Lucky.lib`) in `Build/Output/<Configuration>/`.

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
- [SquirrelNoise5](Dependencies/SquirrelNoise5.hpp) (CC-BY-3.0 US) - vendored

See [Dependencies/LICENSES.md](Dependencies/LICENSES.md) for full license texts.

## License

MIT License. See [LICENSE.md](LICENSE.md).
