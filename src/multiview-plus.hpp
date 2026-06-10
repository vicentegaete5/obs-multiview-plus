#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  obs-multiview-plus  –  Multiview Layout & Scene-Order Plugin
//  Target: OBS Studio 32+  |  Windows
// ─────────────────────────────────────────────────────────────────────────────

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QObject>
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QPoint>
#include <QString>
#include <QList>
#include <QTimer>

#include <string>
#include <vector>
#include <memory>
#include <optional>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-multiview-plus", "en-US")

// ── Constants ────────────────────────────────────────────────────────────────

#define PLUGIN_NAME    "obs-multiview-plus"
#define PLUGIN_VERSION "1.0.0"

// Config section / key names
#define CFG_SECTION     "MultiviewPlus"
#define CFG_LAYOUT_MODE "LayoutMode"
#define CFG_SCENE_ORDER "SceneOrder"   // comma-separated scene names

// ── Layout mode ──────────────────────────────────────────────────────────────

enum class MultiviewLayout : int {
    Default = 0,
    Column  = 1,
    Row     = 2,
};

inline const char *layoutName(MultiviewLayout m)
{
    switch (m) {
    case MultiviewLayout::Column:  return "Column";
    case MultiviewLayout::Row:     return "Row";
    default:                       return "Default";
    }
}

// ── Scene info snapshot ──────────────────────────────────────────────────────

struct SceneEntry {
    std::string name;       // obs scene source name
    bool        visible;    // shown in multiview?
};

// ── Forward declarations ─────────────────────────────────────────────────────

class MultiviewPlusManager;
class MultiviewOverlay;
class SceneOrderModel;
