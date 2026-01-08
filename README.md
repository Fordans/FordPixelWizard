## FordPixelWizard (Prototype)

Desktop prototype app that converts a normal image into a **Pixel Art** style image using:
- **Block-based processing** (explicit N×N blocks; not resize-based)
- **Palette limitation** via **K-means** (OpenCV), clustering in **Lab** color space for more perceptual results
- Optional **pre-blur** and **edge enhancement**

### Tech stack
- **C++17**
- **UI**: Dear ImGui
- **Windowing**: GLFW
- **Rendering**: OpenGL2 backend (keeps build simple on Windows)
- **Image processing**: OpenCV (C++)

### Build (Windows)
1. Install **OpenCV** (C++ package) and make sure `OpenCVConfig.cmake` exists.
2. Set `OpenCV_DIR` to the folder that contains `OpenCVConfig.cmake`:

```bat
setx OpenCV_DIR "C:\vendor\opencv\build\x64\vc16\lib"
```

If your OpenCV build is under `...\vc16\...` (VS2019 toolset), and you see toolset/link errors with VS2022,
either install the **MSVC v142** toolset, or build with:

```bat
build.bat Release v142
```

3. Build:

```bat
build.bat
```

If your OpenCV is installed under `C:\vendor\opencv`, `build.bat` will try to auto-detect `OpenCVConfig.cmake` under that root automatically.

Binary output (depends on generator):
- Visual Studio generator: `build\vs2022_x64\Release\FordPixelWizard.exe`
- Ninja generator: `build\FordPixelWizard.exe`

### Usage
- Click **"Browse..."** to select an image file (`.png`/`.jpg`)
- Adjust parameters (Block Size, Palette Size, etc.)
- Click **"Pixelize (Pixel Art)"** to process
- Review the result in the preview panel
- Click **"Save"** to save the pixel art result

### Distribution / Packaging

To create a portable distribution package that works on any Windows PC:

```bat
package.bat Release
```

This will create a `dist\FordPixelWizard` folder containing:
- The executable
- All required DLLs (OpenCV, VC++ Runtime)
- Icon files
- README for end users

The entire `dist\FordPixelWizard` folder can be distributed as-is (portable) or packaged into an installer.

See `DEPLOYMENT.md` for detailed distribution instructions.

**Quick check dependencies:**
```bat
check_dependencies.bat
```


