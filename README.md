
![Demo2 Screenshot](Docs/demo2_screenshot.png)

# Quick start on Windows

## Required Tools
- [Git](https://git-for-windows.github.io/) with Git Bash
- CMake
- For Windows: Visual Studio 2019
- For Mac: Xcode
- For Linux: gcc, etc.

If you're on Windows. All commands are meant to run under Git Bash, not MS-DOS.

## Quick Start

### 1) Get the submodules
```bash
git submodule update --init --recursive
```

### 2) Do a full build
You should do a full-build at least once via command line so that SDL DLLs
are created, both for Release and Debug:
```bash
./build.sh -t Debug
./build.sh -t Release
```
Executable can now be found under `_bin`.

### Using the VS solution
1. The solution is located in `_build\win\dpasca-sdl2-template.sln`
2. From *Solution Explorer*, right click on **Demo1** and choose *"Set As StartUp Project"*
3. Press F7 to build. This will build the project and the required asset conversion tools

### Updating the VS solution or makefiles
```bash
./build.sh -d
```

