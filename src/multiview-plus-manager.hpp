#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  MultiviewPlusManager
//
//  Central controller.  Responsibilities:
//   • Find and track the OBS Multiview QWidget
//   • Manage SceneOrderModel lifetime
//   • Build and show the custom context menu
//   • Handle OBS frontend events (scene collection load/save/switch)
//   • Drive program/preview highlight updates
//   • Persist settings via OBS config API (per-scene-collection)
// ─────────────────────────────────────────────────────────────────────────────

#include "multiview-plus.hpp"
#include "scene-order-model.hpp"
#include "multiview-overlay.hpp"

#include <QPointer>
#include <QTimer>
#include <QAction>
#include <QActionGroup>

class MultiviewPlusManager : public QObject {
    Q_OBJECT

public:
    explicit MultiviewPlusManager(QObject *parent = nullptr);
    ~MultiviewPlusManager() override;

    // Called once after OBS has fully loaded (from obs_module_load)
    void initialize();

    // OBS frontend event dispatcher (static trampoline → member)
    static void frontendEventCallback(enum obs_frontend_event event,
                                       void *private_data);

private slots:
    void onContextMenuRequested(const QPoint &globalPos,
                                 const std::string &sceneName);
    void onSceneClicked(const std::string &sceneName, bool alt);
    void onSceneDoubleClicked(const std::string &sceneName);
    void onPollTimer();

private:
    // ── OBS event handlers ───────────────────────────────────────────────────
    void onSceneCollectionChanged();
    void onSceneCollectionLoaded();
    void onSceneListChanged();
    void onProgramSceneChanged();
    void onPreviewSceneChanged();
    void onExit();

    // ── Layout control ───────────────────────────────────────────────────────
    void setLayout(MultiviewLayout mode);

    // ── Scene ordering ───────────────────────────────────────────────────────
    void moveScene(const std::string &name, bool forward);

    // ── Multiview window tracking ────────────────────────────────────────────
    QWidget *findMultiviewWindow();
    void     attachOverlay();
    void     detachOverlay();

    // ── OBS scene switching ──────────────────────────────────────────────────
    void switchToPreview(const std::string &name);
    void switchToProgram(const std::string &name);

    // ── Persistence ──────────────────────────────────────────────────────────
    void loadSettings();
    void saveSettings();

    // Returns path to per-scene-collection config file.
    // OBS stores scene collections in the "basic/scenes/" directory.
    std::string collectionConfigPath() const;

    // ── Helpers ──────────────────────────────────────────────────────────────
    std::string currentProgramScene() const;
    std::string currentPreviewScene() const;
    bool        studioModeActive()    const;

    // ── State ────────────────────────────────────────────────────────────────
    SceneOrderModel                 m_model;
    MultiviewLayout                 m_layout      = MultiviewLayout::Default;
    QPointer<MultiviewOverlay>      m_overlay;
    QPointer<QWidget>               m_multiviewWin;
    QTimer                          m_pollTimer;   // finds Multiview after open
    std::string                     m_lastContextScene; // scene under cursor at menu open
};
