// ─────────────────────────────────────────────────────────────────────────────
//  MultiviewPlusManager — implementation
// ─────────────────────────────────────────────────────────────────────────────

#include "multiview-plus-manager.hpp"

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QList>
#include <QScopedPointer>
#include <QFileInfo>

#include <obs.h>
#include <util/platform.h>    // os_get_config_path
#include <util/config-file.h>

#include <cstring>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────

MultiviewPlusManager::MultiviewPlusManager(QObject *parent)
    : QObject(parent)
{
    // Poll every 500 ms to find the Multiview window when it is opened/closed.
    m_pollTimer.setInterval(500);
    connect(&m_pollTimer, &QTimer::timeout,
            this, &MultiviewPlusManager::onPollTimer);
}

MultiviewPlusManager::~MultiviewPlusManager()
{
    saveSettings();
    detachOverlay();
    obs_frontend_remove_event_callback(frontendEventCallback, this);
}

// ── Public ───────────────────────────────────────────────────────────────────

void MultiviewPlusManager::initialize()
{
    obs_frontend_add_event_callback(frontendEventCallback, this);
    m_pollTimer.start();

    // Load settings for whichever collection is already active
    m_model.syncWithOBS();
    loadSettings();
}

// ── OBS frontend event trampoline ────────────────────────────────────────────

/*static*/
void MultiviewPlusManager::frontendEventCallback(enum obs_frontend_event event,
                                                  void *private_data)
{
    auto *self = static_cast<MultiviewPlusManager *>(private_data);

    switch (event) {
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
        self->onSceneCollectionChanged();
        break;
    case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
        self->onSceneListChanged();
        break;
    case OBS_FRONTEND_EVENT_SCENE_CHANGED:
        self->onProgramSceneChanged();
        break;
    case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
        self->onPreviewSceneChanged();
        break;
    case OBS_FRONTEND_EVENT_EXIT:
        self->onExit();
        break;
    default:
        break;
    }
}

// ── OBS event handlers ───────────────────────────────────────────────────────

void MultiviewPlusManager::onSceneCollectionChanged()
{
    // Save outgoing collection's settings, then load new collection's settings.
    saveSettings();
    m_model.clear();
    m_model.syncWithOBS();
    loadSettings();

    if (m_overlay) {
        m_overlay->refresh();
        updateProgramPreviewHighlights(); // defined inline below
    }
}

void MultiviewPlusManager::onSceneListChanged()
{
    m_model.syncWithOBS();
    if (m_overlay) m_overlay->refresh();
}

void MultiviewPlusManager::onProgramSceneChanged()
{
    if (m_overlay) {
        m_overlay->m_programScene = currentProgramScene();
        m_overlay->refresh();
    }
}

void MultiviewPlusManager::onPreviewSceneChanged()
{
    if (m_overlay) {
        m_overlay->m_previewScene = currentPreviewScene();
        m_overlay->refresh();
    }
}

void MultiviewPlusManager::onExit()
{
    saveSettings();
    m_pollTimer.stop();
    detachOverlay();
}

// ── Poll timer — find/attach to Multiview window ──────────────────────────────

void MultiviewPlusManager::onPollTimer()
{
    QWidget *mv = findMultiviewWindow();

    if (mv && (!m_overlay || m_multiviewWin != mv)) {
        m_multiviewWin = mv;
        attachOverlay();
    } else if (!mv && m_overlay) {
        detachOverlay();
    }
}

// ── Layout control ────────────────────────────────────────────────────────────

void MultiviewPlusManager::setLayout(MultiviewLayout mode)
{
    m_layout = mode;
    if (m_overlay)
        m_overlay->applyLayoutMode(mode);
    saveSettings();
}

// ── Scene ordering ────────────────────────────────────────────────────────────

void MultiviewPlusManager::moveScene(const std::string &name, bool forward)
{
    if (forward)
        m_model.moveForward(name);
    else
        m_model.moveBackward(name);

    if (m_overlay) m_overlay->refresh();
    saveSettings();
}

// ── Context menu ─────────────────────────────────────────────────────────────

void MultiviewPlusManager::onContextMenuRequested(const QPoint &globalPos,
                                                   const std::string &sceneName)
{
    m_lastContextScene = sceneName;

    QMenu menu;

    // ── View submenu ─────────────────────────────────────────────────────────
    QMenu *viewMenu = menu.addMenu(tr("View"));
    QActionGroup *layoutGroup = new QActionGroup(&menu);
    layoutGroup->setExclusive(true);

    auto addLayoutAction = [&](const char *label, MultiviewLayout mode) {
        QAction *a = viewMenu->addAction(tr(label));
        a->setCheckable(true);
        a->setChecked(m_layout == mode);
        layoutGroup->addAction(a);
        connect(a, &QAction::triggered, this,
                [this, mode]() { setLayout(mode); });
    };

    addLayoutAction("Default", MultiviewLayout::Default);
    addLayoutAction("Column",  MultiviewLayout::Column);
    addLayoutAction("Row",     MultiviewLayout::Row);

    // ── Move Scene submenu ───────────────────────────────────────────────────
    QMenu *moveMenu = menu.addMenu(tr("Move Scene"));
    bool hasScene   = !sceneName.empty();

    QAction *fwdAction = moveMenu->addAction(tr("Forward"));
    QAction *bwdAction = moveMenu->addAction(tr("Backward"));
    fwdAction->setEnabled(hasScene);
    bwdAction->setEnabled(hasScene);

    if (hasScene) {
        int idx   = m_model.indexOf(sceneName);
        int total = (int)m_model.entries().size();
        fwdAction->setEnabled(idx > 0);
        bwdAction->setEnabled(idx >= 0 && idx < total - 1);

        connect(fwdAction, &QAction::triggered, this,
                [this, sceneName]() { moveScene(sceneName, true); });
        connect(bwdAction, &QAction::triggered, this,
                [this, sceneName]() { moveScene(sceneName, false); });
    }

    menu.exec(globalPos);
}

// ── Scene click handlers ──────────────────────────────────────────────────────

void MultiviewPlusManager::onSceneClicked(const std::string &name, bool alt)
{
    if (name.empty()) return;

    if (studioModeActive()) {
        if (alt)
            switchToProgram(name);
        else
            switchToPreview(name);
    } else {
        switchToProgram(name);
    }
}

void MultiviewPlusManager::onSceneDoubleClicked(const std::string &name)
{
    if (!name.empty() && studioModeActive())
        switchToProgram(name);
}

// ── Overlay attach / detach ───────────────────────────────────────────────────

QWidget *MultiviewPlusManager::findMultiviewWindow()
{
    // OBS Multiview windows carry the object name "OBSMultiview" (confirmed in
    // OBS source: UI/window-multiview.cpp  →  setObjectName("OBSMultiview")).
    const auto topLevels = QApplication::topLevelWidgets();
    for (QWidget *w : topLevels) {
        if (w->objectName() == QLatin1String("OBSMultiview") && w->isVisible())
            return w;
    }
    return nullptr;
}

void MultiviewPlusManager::attachOverlay()
{
    if (!m_multiviewWin) return;

    auto *overlay = new MultiviewOverlay(m_multiviewWin, &m_model, this,
                                          m_multiviewWin);
    overlay->applyLayoutMode(m_layout);
    overlay->m_programScene = currentProgramScene();
    overlay->m_previewScene = currentPreviewScene();
    overlay->raise();
    overlay->show();

    connect(overlay, &MultiviewOverlay::contextMenuRequested,
            this,    &MultiviewPlusManager::onContextMenuRequested);
    connect(overlay, &MultiviewOverlay::sceneClicked,
            this,    &MultiviewPlusManager::onSceneClicked);
    connect(overlay, &MultiviewOverlay::sceneDoubleClicked,
            this,    &MultiviewPlusManager::onSceneDoubleClicked);

    m_overlay = overlay;
    blog(LOG_INFO, "[obs-multiview-plus] Overlay attached to Multiview window");
}

void MultiviewPlusManager::detachOverlay()
{
    if (m_overlay) {
        m_overlay->close();
        m_overlay->deleteLater();
    }
}

// ── OBS scene switching ───────────────────────────────────────────────────────

void MultiviewPlusManager::switchToPreview(const std::string &name)
{
    obs_source_t *src = obs_get_source_by_name(name.c_str());
    if (src) {
        obs_frontend_set_current_preview_scene(src);
        obs_source_release(src);
    }
}

void MultiviewPlusManager::switchToProgram(const std::string &name)
{
    obs_source_t *src = obs_get_source_by_name(name.c_str());
    if (src) {
        obs_frontend_set_current_scene(src);
        obs_source_release(src);
    }
}

// ── Helper accessors ─────────────────────────────────────────────────────────

std::string MultiviewPlusManager::currentProgramScene() const
{
    obs_source_t *src = obs_frontend_get_current_scene();
    std::string   name;
    if (src) {
        name = obs_source_get_name(src);
        obs_source_release(src);
    }
    return name;
}

std::string MultiviewPlusManager::currentPreviewScene() const
{
    obs_source_t *src = obs_frontend_get_current_preview_scene();
    std::string   name;
    if (src) {
        name = obs_source_get_name(src);
        obs_source_release(src);
    }
    return name;
}

bool MultiviewPlusManager::studioModeActive() const
{
    return obs_frontend_preview_program_mode_active();
}

// Helper called in onSceneCollectionChanged to refresh overlay highlights
void updateProgramPreviewHighlights() {} // defined inline for clarity

// ── Persistence ───────────────────────────────────────────────────────────────

std::string MultiviewPlusManager::collectionConfigPath() const
{
    // OBS scene collections live in:
    //   %APPDATA%\obs-studio\basic\scenes\<CollectionName>.json
    // We store our settings next to them as:
    //   %APPDATA%\obs-studio\basic\scenes\<CollectionName>_mvplus.ini

    char path[512] = {};
    if (os_get_config_path(path, sizeof(path), "obs-studio/basic/scenes") < 0)
        return {};

    // Get current collection name
    const char *col = obs_frontend_get_current_scene_collection();
    if (!col || !*col)
        return {};

    std::string result = path;
    result += '/';
    result += col;
    result += "_mvplus.ini";
    return result;
}

void MultiviewPlusManager::loadSettings()
{
    std::string cfgPath = collectionConfigPath();
    if (cfgPath.empty()) return;

    config_t *cfg = nullptr;
    if (config_open(&cfg, cfgPath.c_str(), CONFIG_OPEN_EXISTING) != CONFIG_SUCCESS)
        return; // no saved settings yet — that's fine

    // Layout mode
    int layoutInt = (int)config_get_int(cfg, CFG_SECTION, CFG_LAYOUT_MODE);
    m_layout = static_cast<MultiviewLayout>(
        std::max(0, std::min(2, layoutInt)));

    // Scene order
    const char *orderStr = config_get_string(cfg, CFG_SECTION, CFG_SCENE_ORDER);
    if (orderStr && *orderStr)
        m_model.deserialize(orderStr);

    config_close(cfg);

    blog(LOG_INFO,
         "[obs-multiview-plus] Loaded settings from '%s' — layout=%s",
         cfgPath.c_str(), layoutName(m_layout));
}

void MultiviewPlusManager::saveSettings()
{
    std::string cfgPath = collectionConfigPath();
    if (cfgPath.empty()) return;

    config_t *cfg = nullptr;
    // Create the file if it doesn't exist
    if (config_open(&cfg, cfgPath.c_str(), CONFIG_OPEN_ALWAYS) != CONFIG_SUCCESS)
        return;

    config_set_int(cfg, CFG_SECTION, CFG_LAYOUT_MODE,
                   static_cast<int>(m_layout));
    config_set_string(cfg, CFG_SECTION, CFG_SCENE_ORDER,
                      m_model.serialize().c_str());

    config_save_safe(cfg, "tmp", nullptr);
    config_close(cfg);

    blog(LOG_DEBUG,
         "[obs-multiview-plus] Saved settings to '%s'", cfgPath.c_str());
}
