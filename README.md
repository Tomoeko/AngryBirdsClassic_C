# Angry Birds Classic C

First-party C reimplementation of Angry Birds Android 5.2.0 with Metal and GLES2
rendering, interactive menus, resource tools and a level preview.

Place `com.rovio.angrybirds_5.2.0-12200520.apk` in `Files/`.

```sh
git clone --recursive git@github.com:Tomoeko/AngryBirdsClassic_C.git
cd AngryBirdsClassic_C
```

macOS (Metal):

```sh
cmake -S . -B Files/build -DCMAKE_BUILD_TYPE=Release
cmake --build Files/build --parallel
Files/build/angrybirds-preview Files/com.rovio.angrybirds_5.2.0-12200520.apk
```

Windows (GLES2 through OpenGL 2.1): install Visual Studio 2026 with
**Desktop development with C++**
and **C++ CMake tools for Windows**. From its **x64 Native Tools Command Prompt**:

```bat
cmake -S . -B Files\build-windows -G "Visual Studio 18 2026" -A x64
cmake --build Files\build-windows --config Release --parallel
Files\build-windows\Release\angrybirds-preview.exe Files\com.rovio.angrybirds_5.2.0-12200520.apk
```

Linux (GLES2): install the EGL, GLES2 and X11 development libraries.

```sh
cmake -S . -B Files/build -DCMAKE_BUILD_TYPE=Release -DAB_BACKEND=gles2
cmake --build Files/build --parallel
Files/build/angrybirds-preview Files/com.rovio.angrybirds_5.2.0-12200520.apk
```

The app opens the main menu and uses the highest APK UI assets (1024×768),
native display pixels and native audio output. Menus, the first intro, level 1,
winning and failed result cards, Next/Restart/Menu, pause/resume and ending fast-forward
work in Metal.
Danger Above and Mine and Dine include their hidden-egg menu rewards.
Complete startup, the full ability HUD, credits and specialized
episodes remain in progress.
State stays in `Files/AngryBirdsC/userstate/`.

F toggles fullscreen. Escape pauses/resumes the game or leaves a result screen;
outside gameplay it leaves fullscreen. Close the window to quit.
Drag to aim or move the camera; use the scroll wheel to zoom within each level's
camera limits. The pointer stays visible.
Options go before the APK path. `--window-size 1280x720` sets the initial window
size. `--frames N` limits the run;
`--capture Files/frame.ppm` saves a frame.

Initialize an existing checkout with `git submodule update --init --recursive`.
[Console_Common](https://github.com/Tomoeko/Console_Common) supplies shared
platform and rendering support. Builds do not download dependencies.
Private inputs, state, captures and build products stay in `Files/`.

See [recovery tools](docs/recovery-tools.md), [scene preview](docs/scene-preview.md)
and [verified coverage](docs/evidence.md).
