#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  MultiviewEventFilter
//
//  Installed on the OBS Multiview QWidget when the overlay is in Default mode.
//  Catches right-click / QEvent::ContextMenu BEFORE OBS processes it, so our
//  custom menu appears.  If the user dismisses without selecting, we do NOT
//  consume the event, allowing OBS's own context menu to fire (optional
//  behavior controlled by m_consumeAll).
// ─────────────────────────────────────────────────────────────────────────────

#include "multiview-plus.hpp"
#include <QEvent>
#include <QContextMenuEvent>
#include <QMouseEvent>

class MultiviewEventFilter : public QObject {
    Q_OBJECT

public:
    explicit MultiviewEventFilter(QObject *parent = nullptr)
        : QObject(parent) {}

signals:
    // Emitted when a right-click context menu should be shown.
    // globalPos: screen coordinates for menu placement.
    // localPos:  widget-local coordinates (for scene hit-testing).
    void contextMenuRequested(const QPoint &globalPos, const QPoint &localPos);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::ContextMenu) {
            auto *ce = static_cast<QContextMenuEvent *>(event);
            emit contextMenuRequested(ce->globalPos(), ce->pos());
            // Consume the event so OBS's own context menu doesn't open.
            // Comment this line out to allow OBS's menu as a fallback.
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};
