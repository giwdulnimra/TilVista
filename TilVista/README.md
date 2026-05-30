# TilVista – C++ / Qt6  –  CLion Setup

## Voraussetzungen

| Tool | Version | Download |
|---|---|---|
| Qt 6 | ≥ 6.5 | https://www.qt.io/download-qt-installer |
| CMake | ≥ 3.20 | bundled with CLion or https://cmake.org |
| Compiler | MSVC 2022 / MinGW 13 / GCC 13 / Clang 16 | |
| ffmpeg | optional | https://ffmpeg.org/download.html |

---

## 1  Qt installieren

Starte den **Qt Online Installer** und wähle mindestens:

```
Qt 6.x  →  Desktop  →  MSVC 2022 64-bit   (Windows)
                  oder  MinGW 13.1 64-bit  (Windows, kein MSVC)
                  oder  GCC 64-bit         (Linux)
Qt 6.x  →  Qt Multimedia              ← erforderlich für Video-Playback
Qt 6.x  →  Qt Multimedia Backends     ← Windows Media Foundation / GStreamer
```

---

## 2  CLion konfigurieren

### CMAKE_PREFIX_PATH setzen

`Settings → Build, Execution, Deployment → CMake`

Feld **CMake options** (Beispiel für Qt 6.7 + MSVC):
```
-DCMAKE_PREFIX_PATH=C:/Qt/6.7.0/msvc2022_64
```

Linux / macOS (Qt aus dem Installer):
```
-DCMAKE_PREFIX_PATH=/home/<user>/Qt/6.7.0/gcc_64
```

### Profil anlegen

- **Build type**: Debug (Entwicklung), Release (Deployment)
- **Generator**: Ninja (schneller) oder "Visual Studio 17 2022"
- **Toolchain**: MSVC 2022 oder MinGW (muss in CLion unter
  `Settings → Build → Toolchains` hinterlegt sein)

### Projekt öffnen

`File → Open` → das Verzeichnis `TilVista/` wählen
(das mit `CMakeLists.txt`).  CLion liest CMake automatisch ein.

---

## 3  Bauen

```
Strg+F9   (Build)
Shift+F10 (Run)
```

Alternativ über das Hammer-Icon im oberen Toolbar.

---

## 4  ffmpeg (optional, für Video-Frame-Cycling)

Windows:
```powershell
winget install ffmpeg
# oder manuell: bin/ in PATH eintragen
```

Linux:
```bash
sudo apt install ffmpeg        # Debian/Ubuntu
sudo dnf install ffmpeg        # Fedora
```

macOS:
```bash
brew install ffmpeg
```

Ohne ffmpeg fällt die Video-Vorschau automatisch auf den System-Icon-Fallback zurück.

---

## 5  Verzeichnisstruktur zur Laufzeit

```
<Executable>/
└── optegnelser/
    ├── Kaivo/
    │   ├── tilvista_dirs.json
    │   ├── Fotos_250508_img.dshow
    │   └── Fotos_250508_all.catalogue
    ├── reviewpics.txt        (Lesezeichen aus der Slideshow)
    └── loadingerrors.log     (optional)
```

---

## 6  Projektstruktur

```
TilVista/
├── CMakeLists.txt
├── README_CLION.md
├── resources/
│   └── resources.qrc
└── src/
    ├── main.cpp
    ├── core/
    │   ├── pathutils.h
    │   └── pathutils.cpp
    ├── workers/
    │   ├── scanworker.h / .cpp
    │   ├── dbworkers.h / .cpp
    │   ├── catalogueworkers.h / .cpp
    │   └── videoframesworker.h / .cpp
    └── ui/
        ├── previewlabel.h / .cpp
        ├── videopreviewwidget.h / .cpp
        ├── dirbar.h / .cpp
        ├── dirdatabasepanel.h / .cpp
        ├── aleavuetab.h / .cpp
        ├── sattumapictab.h / .cpp
        ├── slideshowwindow.h / .cpp
        ├── mainwindow.h / .cpp
        └── (main.cpp lives in src/)
```

---

## 7  Bekannte Eigenheiten

**`QMediaPlayer` + Multimedia-Backend**
Auf Windows wird standardmäßig das *Windows Media Foundation* Backend
verwendet – es unterstützt `.mp4`, `.mkv`, `.mov` und mehr.
Auf Linux wird **GStreamer** benötigt:
```bash
sudo apt install gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
                 gstreamer1.0-libav
```

**`Q_OS_WIN` Sleep-Prevention**
`preventSleep()` ruft `SetThreadExecutionState` auf – erfordert keine
zusätzlichen Libraries, aber der Aufruf hat nur Effekt wenn der Thread
*läuft* (nicht suspended). Im Release-Build passiert das aus dem
Main-Thread des SlideshowWindow.

**MSVC vs. MinGW**
Beide funktionieren. MSVC kompiliert schneller und hat bessere Debugger-
Integration in CLion. MinGW erzeugt kleinere Binaries und braucht kein
MSVC-Redistributable auf dem Zielrechner.
