// ─────────────────────────────────────────────────────────────────────────────
//  multiview-plus-manager-eventfilter-patch.cpp
//
//  This file shows the additions to multiview-plus-manager.cpp that wire up
//  MultiviewEventFilter so right-click works in Default layout mode.
//
//  In the final build, merge these additions into multiview-plus-manager.cpp.
// ─────────────────────────────────────────────────────────────────────────────

/*
  ── In multiview-plus-manager.hpp, add private member: ──────────────────────

    #include "multiview-event-filter.hpp"
    ...
    MultiviewEventFilter *m_eventFilter = nullptr;


  ── In attachOverlay(), after creating the overlay, add: ────────────────────

    // Install event filter for Default-mode right-click interception
    m_eventFilter = new MultiviewEventFilter(this);
    m_multiviewWin->installEventFilter(m_eventFilter);

    connect(m_eventFilter, &MultiviewEventFilter::contextMenuRequested,
            this, [this](const QPoint &globalPos, const QPoint &localPos) {
                // In Default mode there is no overlay to hit-test, so we pass
                // an empty scene name.  The manager's context menu shows View
                // sub-menu but disables Move Scene items.
                std::string sceneName;
                if (m_overlay && m_layout != MultiviewLayout::Default)
                    sceneName = m_overlay->sceneAtPoint(localPos);
                onContextMenuRequested(globalPos, sceneName);
            });


  ── In detachOverlay(), before deleteLater(), add: ───────────────────────────

    if (m_eventFilter && m_multiviewWin) {
        m_multiviewWin->removeEventFilter(m_eventFilter);
        m_eventFilter->deleteLater();
        m_eventFilter = nullptr;
    }


  ── In setLayout(), update event filter transparency: ───────────────────────

    // When switching TO Default mode, make sure event filter is active.
    // When switching FROM Default mode, the overlay handles events directly.
    if (m_eventFilter) {
        // Event filter is always installed; overlay WA_TransparentForMouseEvents
        // controls whether overlay or underlying window sees mouse events.
    }
*/
