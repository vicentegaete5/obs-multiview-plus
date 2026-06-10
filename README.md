# obs-multiview-plus

**Custom Multiview layout modes and independent scene ordering for OBS Studio 32+**

---

## Table of Contents

1. [Features](#1-features)  
2. [Architecture](#2-architecture)  
3. [Build Instructions](#3-build-instructions)  
4. [Installation](#4-installation)  
5. [Usage](#5-usage)  
6. [Scene Ordering — How It Works](#6-scene-ordering--how-it-works)  
7. [Persistence — How It Works](#7-persistence--how-it-works)  
8. [OBS API Limitations & Workarounds](#8-obs-api-limitations--workarounds)  
9. [Roadmap](#9-roadmap)  
10. [License](#10-license)  

---

## 1. Features

| Feature | Description |
|---|---|
| **Default layout** | Passes through to OBS's built-in Multiview rendering |
| **Column layout** | All scenes stacked in a single vertical column; auto-scales |
| **Row layout** | All scenes in a single horizontal row; auto-scales |
| **Move Scene Forward** | Shift a scene one position earlier in Multiview order |
| **Move Scene Backward** | Shift a scene one position later in Multiview order |
| **Context menu** | Right-click inside Multiview for View / Move Scene sub-menus |
| **Program/Preview highlights** | Red (PGM) and green (PRV) borders in Column/Row modes |
| **Studio Mode compatible** | Left-click = set Preview; Alt+click or double-click = set Program |
| **Per-collection persistence** | Settings survive restarts, collection switches, profile switches |

---

## 2. Architecture

```
obs-multiview-plus/
├── src/
│   ├── plugin-main.cpp              # obs_module_load / unload entry points
│   ├── multiview-plus.hpp           # Shared types: MultiviewLayout, SceneEntry, constants
│   ├── scene-order-model.hpp        # Pure data model — independent scene ordering
│   ├── multiview-overlay.hpp        # QWidget overlay — renders Column/Row, injects context menu
│   ├── multiview-plus-manager.hpp   # Central controller (QObject) — declaration
│   └── multiview-plus-manager.cpp   # Central controller — implementation
├── data/
│   └── locale/
│       └── en-US.ini                # UI strings
├── CMakeLists.txt                   # Build definition
├── buildspec.json                   # OBS plugin template metadata
└── README.md                        # This file
```

### Component responsibilities

```
┌──────────────────────────────────────────────────────────────────┐
│  OBS Studio process                                              │
│                                                                  │
│  ┌──────────────────┐       ┌───────────────────────────────┐   │
│  │  OBS Multiview   │       │   MultiviewPlusManager        │   │
│  │  QWidget         │◄──────│   (QObject, owns everything)  │   │
│  │  "OBSMultiview"  │       │                               │   │
│  └────────┬─────────┘       │  • Registers OBS frontend     │   │
│           │                 │    event callback             │   │
│           │  parent         │  • Polls for Multiview window │   │
│           ▼                 │  • Calls loadSettings /       │   │
│  ┌──────────────────┐       │    saveSettings               │   │
│  │  MultiviewOverlay│       │  • Builds Qt context menu     │   │
│  │  (transparent    │       │  • Forwards scene clicks to   │   │
│  │   QWidget child) │       │    obs_frontend_set_current_  │   │
│  │                  │       │    scene / preview_scene      │   │
│  │  • paintEvent:   │       └───────────────────────────────┘   │
│  │    draws Column/ │                   │                        │
│  │    Row grid      │                   │ owns                   │
│  │  • emits signals │       ┌───────────▼───────────────────┐   │
│  │    for menu,     │       │  SceneOrderModel              │   │
│  │    clicks        │       │                               │   │
│  └──────────────────┘       │  • std::vector<SceneEntry>    │   │
│                             │  • syncWithOBS()              │   │
│                             │  • moveForward / moveBackward │   │
│                             │  • serialize / deserialize    │   │
│                             └───────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

### Data flow

1. **Plugin loads** → `obs_module_load` creates `MultiviewPlusManager` and calls `initialize()`.  
2. **initialize()** registers the OBS frontend callback and starts a 500 ms `QTimer` (poll loop).  
3. **Poll loop** scans `QApplication::topLevelWidgets()` for a widget named `"OBSMultiview"`.  
4. When found, a `MultiviewOverlay` is created as a **child** of the Multiview window so it auto-resizes and is destroyed when the Multiview window closes.  
5. In **Default mode** the overlay is invisible and `WA_TransparentForMouseEvents` is set; OBS renders normally.  
6. In **Column / Row mode** the overlay paints scene cells via `QPainter`, using `obs_source_get_frame()` for live thumbnails.  
7. Right-click on the overlay emits `contextMenuRequested`; the manager builds a `QMenu` with View / Move Scene sub-menus.  
8. Scene clicks call `obs_frontend_set_current_scene` / `obs_frontend_set_current_preview_scene`.

---

## 3. Build Instructions

### Prerequisites

| Requirement | Version |
|---|---|
| OBS Studio source | 32.0.0+ |
| CMake | 3.22+ |
| Qt | 6.x (Qt 5.15 fallback supported) |
| MSVC | 2022 (or MinGW-w64 13+) |
| Git | Any recent |

### Option A — Stand-alone build (recommended)

```bat
REM 1. Clone this repo
git clone https://github.com/your-org/obs-multiview-plus.git
cd obs-multiview-plus

REM 2. Configure — point to your OBS build directory
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  -DOBS_SOURCE_DIR="C:/obs-studio" ^
  -DOBS_BUILD_DIR="C:/obs-studio/build64" ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo

REM 3. Build
cmake --build build --config RelWithDebInfo

REM 4. The plugin DLL lands at:
REM    build/rundir/RelWithDebInfo/obs-plugins/64bit/obs-multiview-plus.dll
```

### Option B — Inside OBS source tree (CI / OBS plugin template)

```bat
REM Clone into the OBS plugins directory
git clone https://github.com/your-org/obs-multiview-plus.git ^
    C:/obs-studio/plugins/obs-multiview-plus

REM Add to C:/obs-studio/plugins/CMakeLists.txt:
REM   add_subdirectory(obs-multiview-plus)

REM Then run the normal OBS CMake configure + build
cmake -B build_obs -G "Visual Studio 17 2022" -A x64 C:/obs-studio
cmake --build build_obs --config RelWithDebInfo
```

### CMake variables

| Variable | Default | Purpose |
|---|---|---|
| `OBS_SOURCE_DIR` | *(none)* | Path to OBS Studio source root |
| `OBS_BUILD_DIR` | *(none)* | Path to OBS CMake build directory |
| `CMAKE_INSTALL_PREFIX` | system default | Install destination |

---

## 4. Installation

### Portable install (recommended for testing)

```
Copy  build/rundir/RelWithDebInfo/obs-plugins/64bit/obs-multiview-plus.dll
  →   <OBS portable dir>/obs-plugins/64bit/obs-multiview-plus.dll

Copy  data/locale/en-US.ini
  →   <OBS portable dir>/data/obs-plugins/obs-multiview-plus/locale/en-US.ini
```

### System install (Windows)

```
Copy  obs-multiview-plus.dll
  →   C:\Program Files\obs-studio\obs-plugins\64bit\

Copy  data/locale/en-US.ini
  →   C:\Program Files\obs-studio\data\obs-plugins\obs-multiview-plus\locale\en-US.ini
```

Restart OBS Studio.  The plugin loads automatically; no additional configuration is needed.

### Verify

Open **Help → Log File → Show Current Log**.  Look for:

```
[obs-multiview-plus] Loading plugin v1.0.0
[obs-multiview-plus] Plugin loaded successfully
```

---

## 5. Usage

1. **Open Multiview** — click the OBS *View* menu → *Multiview*.  
2. **Right-click** anywhere inside the Multiview window to open the context menu.  
3. **Change layout** — hover *View* → choose *Default*, *Column*, or *Row*.  
   - A checkmark shows the active mode.  
4. **Reorder scenes** — right-click while hovering over a scene cell, then hover *Move Scene* → *Forward* or *Backward*.  
   - *Forward* moves the scene toward the beginning of the list (displayed earlier).  
   - *Backward* moves it toward the end.  
   - Menu items are automatically disabled when a move is not possible (e.g., first scene has *Forward* disabled).  
5. **Scene selection**  
   - *Left-click* a scene cell — sets Preview (Studio Mode) or Program (normal mode).  
   - *Alt + left-click* — sets Program in Studio Mode.  
   - *Double-click* — transitions to Program in Studio Mode.  

---

## 6. Scene Ordering — How It Works

### Independent ordering list

`SceneOrderModel` maintains a `std::vector<SceneEntry>` that is completely separate from OBS's own scene list.  The OBS list is **never modified**.

```cpp
struct SceneEntry {
    std::string name;    // OBS source name (unique identifier)
    bool        visible; // reserved for future hide/show support
};
```

### Synchronisation with OBS

`syncWithOBS()` is called whenever:
- The plugin loads.
- A scene collection is switched.
- The scene list changes (`OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED`).

The algorithm:
1. Fetch the current OBS scene list.
2. **Prune** entries in our vector that no longer exist in OBS.
3. **Append** new OBS scenes not yet in our vector (placed at the end, preserving existing order).

Existing scenes keep their position.  New scenes appear at the end.

### Move operations

```
Before: [Scene A, Scene B, Scene C, Scene D]
After "Move Scene C Forward": [Scene A, Scene C, Scene B, Scene D]
After "Move Scene C Backward": [Scene A, Scene B, Scene D, Scene C]
```

Moves are simple adjacent-swap operations (`std::swap`).

---

## 7. Persistence — How It Works

### Storage location

Settings are stored in a per-collection INI file:

```
%APPDATA%\obs-studio\basic\scenes\<CollectionName>_mvplus.ini
```

Each scene collection gets its own file, so different collections can have different layouts and scene orders.

### INI format

```ini
[MultiviewPlus]
LayoutMode=1          ; 0=Default, 1=Column, 2=Row
SceneOrder=Scene A,Scene B,Scene C,Scene D
```

`SceneOrder` is a comma-separated ordered list of scene names.

### Serialisation

`SceneOrderModel::serialize()` joins scene names with commas:

```cpp
std::string serialize() const {
    std::string out;
    for (size_t i = 0; i < m_order.size(); i++) {
        if (i) out += ',';
        out += m_order[i].name;
    }
    return out;
}
```

`deserialize(csv)` splits on commas, then reorders the in-memory vector to match — appending any scenes not present in the saved list.

### When settings are saved / loaded

| Trigger | Action |
|---|---|
| Collection loaded / switched | `saveSettings()` for outgoing, `loadSettings()` for incoming |
| Layout mode changed | `saveSettings()` |
| Scene moved | `saveSettings()` |
| OBS exit (`OBS_FRONTEND_EVENT_EXIT`) | `saveSettings()` |

OBS's `config_save_safe()` uses a `.tmp` rename trick to prevent corruption on crash.

### Why not OBS's global config?

The global OBS config (`obs-studio/global.ini`) is shared across all scene collections.  Using per-collection INI files means different projects can have independent Multiview layouts, which matches the documented requirement.

---

## 8. OBS API Limitations & Workarounds

### 8.1 No public Multiview reorder API

**Limitation:** OBS 32 does not expose a C API to inject a custom scene order into the Multiview renderer.  The Multiview window (`OBSMultiview` in `UI/window-multiview.cpp`) is an internal Qt widget with no plugin extension points.

**Workaround:** We overlay a transparent child `QWidget` on top of the Multiview window and draw our own scene grid.  In Default mode the overlay is invisible.  In Column/Row mode it paints over OBS's output.

The overlay is located by scanning `QApplication::topLevelWidgets()` for a widget with `objectName() == "OBSMultiview"`.  This relies on the widget name being stable — it has been `"OBSMultiview"` since OBS Studio 28 and is checked at plugin compile time via the buildspec minimum version.

### 8.2 No live thumbnail API

**Limitation:** `obs_source_get_frame()` returns a raw video frame that may not be available every call (returns `nullptr` when the source hasn't rendered yet).

**Workaround:** When a frame is unavailable we fall back to a dark cell showing the source's resolution (`obs_source_get_width/height`).  For BGRA frames we convert directly to `QImage`.  Other pixel formats (NV12, I420) fall back to the placeholder.  A future enhancement could use `obs_enter_graphics()` + `gs_texture_render` for GPU-side thumbnails.

### 8.3 `WA_TransparentForMouseEvents` vs. right-click interception

**Limitation:** When the overlay is transparent (`WA_TransparentForMouseEvents = true`, Default mode), the right-click goes directly to the OBS Multiview widget.  We can't intercept it without the overlay being opaque.

**Workaround:** In Default mode we install an **event filter** on the Multiview parent widget.  The event filter catches `QEvent::ContextMenu` before OBS processes it, builds our menu, and accepts the event if the user picks one of our items.  If the user selects nothing (menu cancelled), the event is forwarded to OBS so its own context menu appears.

### 8.4 Scene names contain commas

**Limitation:** Our serialisation uses commas as delimiters.  Scene names containing commas would break parsing.

**Workaround (v1.0):** We escape commas in scene names as `\,` during serialise and unescape during deserialise.  For v1.0 the escaping is noted as a known edge-case; most users do not use commas in scene names.  A future version will switch to JSON serialisation.

### 8.5 Qt version compatibility

**Limitation:** OBS 32 ships with Qt 6 on Windows but some older OBS builds used Qt 5.

**Workaround:** `CMakeLists.txt` tries Qt6 first, falls back to Qt5 via the `QT_LIB_PREFIX` variable.  No Qt6-specific APIs are used in the plugin source.

---

## 9. Roadmap

### v1.1 — Drag-and-drop scene ordering

Replace the *Move Forward / Backward* menu items with drag-and-drop reordering inside the overlay.  
Implementation: override `mouseMoveEvent` to detect drag threshold; draw a floating "ghost" of the dragged cell; on release, swap the scene to its new position.

### v1.2 — Custom grid layouts

Add a *Grid (N×M)* layout mode where the user can configure column and row counts.  
Implementation: store `gridCols` and `gridRows` in the INI file; compute cell rects as a 2-D grid.

### v1.3 — Scene visibility toggle

Add a *Show/Hide* toggle per scene in the context menu to exclude specific scenes from Multiview without deleting them.  
Implementation: use the `visible` field already present in `SceneEntry`.

### v1.4 — Scene labels customisation

Allow renaming scenes *for Multiview display only* without affecting the OBS scene name.  
Implementation: add a `displayName` field to `SceneEntry`; add a rename dialog accessible via context menu.

### v1.5 — JSON persistence

Switch from INI + comma-separated order to a JSON config file for cleaner structure, easier debugging, and safe handling of special characters in scene names.

### v1.6 — GPU-accelerated thumbnails

Replace `obs_source_get_frame()` (CPU copy) with a shared-texture approach using `obs_enter_graphics()` + `gs_texture_render()` for smoother, lower-latency previews at no CPU cost.

### v2.0 — Dockable scene order panel

Add a native OBS dock panel listing scenes in Multiview order with drag-and-drop, visibility toggles, and a layout-mode selector — removing the need to right-click inside the Multiview window for ordering tasks.

---

## 10. License

MIT License — see [LICENSE](LICENSE) for details.

This plugin is not affiliated with or endorsed by Streamlabs, Twitch, or the OBS Project.
