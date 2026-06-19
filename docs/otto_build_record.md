# Otto build record

This project is edited in:

```text
F:\Otto2\xiaozhi-esp32-main (1)\xiaozhi-esp32-main
```

The ESP-IDF build should use this fixed short-path mirror:

```text
D:\OttoBuild\xz_com10_build
```

Reason: the source path contains spaces and parentheses, which previously caused ESP-IDF build issues. Reuse the same mirror path instead of creating a new temporary copy.

Toolchain used successfully:

```text
IDF_PATH=D:\Espressif\v5.5.2\esp-idf
IDF_TOOLS_PATH=C:\Espressif\tools
IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v5.5.2\venv
Python=C:\Espressif\tools\python\v5.5.2\venv\Scripts\python.exe
Target=esp32s3
Board=otto-robot
Flash port=COM10
```

For repeat builds, run:

```powershell
.\scripts\otto_build_com10.ps1 -Flash
```

Use `-Clean` when the generated GIF assets or CMake asset packaging rules changed.
