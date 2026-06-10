#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  MultiviewOverlay
//
//  A transparent QWidget placed over the OBS Multiview window.
//
//  Rendering strategy
//  ──────────────────
//  In Default mode we are entirely transparent — OBS renders normally and we
//  only inject the context menu.
//
//  In Column / Row mode we intercept paintEvent to draw scene previews
//  ourselves using obs_source_get_frame() → draw them via Qt, and overlay
//  labels, selection highlights, etc.  Mouse events are forwarded to the
//  underlying Multiview window so click-to-preview / click-to-program still
//  works.
//
//  OBS Multiview API limitation workaround
//  ────────────────────────────────────────
//  OBS 32 does not expose a public C API to repaint the Multiview with a
//  custom scene order.  Instead we:
//    1. Render our own scene grid on top via QPainter + obs_source_video_render.
//    2. In Default mode, remain invisible so OBS renders normally.
//    3. Inject a right-click context menu before OBS processes it.
// ─────────────────────────────────────────────────────────────────────────────

#include "multiview-plus.hpp"
#include "scene-order-model.hpp"

#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QLabel>
#include <QRect>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QApplication>
#include <QScreen>
#include <QImage>

// Thin wrapper so we can capture paintEvent on the real Multiview window.
// We parent ourselves to the Multiview window and set WA_TransparentForMouseEvents
// only when in Default mode.

class MultiviewOverlay : public QWidget {
    Q_OBJECT

public:
    explicit MultiviewOverlay(QWidget *multiviewParent,
                               SceneOrderModel *model,
                               QObject *manager,
                               QWidget *parentWidget = nullptr)
        : QWidget(parentWidget ? parentWidget : multiviewParent)
        , m_model(model)
        , m_manager(manager)
        , m_layout(MultiviewLayout::Default)
    {
        // Cover the whole parent
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setMouseTracking(true);

        // Start in default mode → transparent passthrough
        applyLayoutMode(MultiviewLayout::Default);

        if (multiviewParent) {
            multiviewParent->installEventFilter(this);
            resize(multiviewParent->size());
        }
    }

    void applyLayoutMode(MultiviewLayout mode)
    {
        m_layout = mode;

        if (mode == MultiviewLayout::Default) {
            // Fully transparent, let OBS render
            setAttribute(Qt::WA_TransparentForMouseEvents, true);
            hide();
        } else {
            setAttribute(Qt::WA_TransparentForMouseEvents, false);
            show();
            update();
        }
    }

    MultiviewLayout currentLayout() const { return m_layout; }

    // Called by manager when scene collection or order changes
    void refresh() { update(); }

    // Hit-test: which scene name is under pixel position p (in our coords)?
    std::string sceneAtPoint(const QPoint &p) const
    {
        const auto &entries = m_model->entries();
        for (int i = 0; i < (int)m_cellRects.size() && i < (int)entries.size(); i++) {
            if (m_cellRects[i].contains(p))
                return entries[i].name;
        }
        return {};
    }

signals:
    void contextMenuRequested(const QPoint &globalPos, const std::string &sceneName);
    void sceneClicked(const std::string &sceneName, bool alt);
    void sceneDoubleClicked(const std::string &sceneName);

protected:
    // ── Paint ────────────────────────────────────────────────────────────────

    void paintEvent(QPaintEvent *) override
    {
        if (m_layout == MultiviewLayout::Default)
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Dark background
        p.fillRect(rect(), QColor(20, 20, 20));

        const auto &entries = m_model->entries();
        if (entries.empty()) return;

        m_cellRects.clear();
        m_cellRects.reserve(entries.size());

        int count = (int)entries.size();
        QRect area = rect().adjusted(4, 4, -4, -4);

        for (int i = 0; i < count; i++) {
            QRect cell = cellRect(i, count, area);
            m_cellRects.push_back(cell);
            drawSceneCell(p, cell, entries[i], i);
        }
    }

    // ── Mouse ────────────────────────────────────────────────────────────────

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            std::string scene = sceneAtPoint(e->pos());
            if (!scene.empty()) {
                bool alt = (e->modifiers() & Qt::AltModifier) != 0;
                emit sceneClicked(scene, alt);
            }
        }
        QWidget::mousePressEvent(e);
    }

    void mouseDoubleClickEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            std::string scene = sceneAtPoint(e->pos());
            if (!scene.empty())
                emit sceneDoubleClicked(scene);
        }
        QWidget::mouseDoubleClickEvent(e);
    }

    void contextMenuEvent(QContextMenuEvent *e) override
    {
        std::string scene = sceneAtPoint(e->pos());
        emit contextMenuRequested(e->globalPos(), scene);
        e->accept();
    }

    // ── Resize tracking ──────────────────────────────────────────────────────

    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        Q_UNUSED(obj)
        if (ev->type() == QEvent::Resize) {
            auto *re = static_cast<QResizeEvent *>(ev);
            resize(re->size());
            update();
        }
        return false;
    }

private:
    // ── Layout helpers ───────────────────────────────────────────────────────

    QRect cellRect(int index, int total, const QRect &area) const
    {
        if (m_layout == MultiviewLayout::Column) {
            // Vertical stack
            int cellH = area.height() / total;
            int y     = area.top() + index * cellH;
            return {area.left(), y, area.width(), cellH};
        } else {
            // Horizontal row
            int cellW = area.width() / total;
            int x     = area.left() + index * cellW;
            return {x, area.top(), cellW, area.height()};
        }
    }

    // ── Scene cell drawing ───────────────────────────────────────────────────

    void drawSceneCell(QPainter &p, const QRect &cell,
                       const SceneEntry &entry, int /*index*/)
    {
        // Border
        bool isProgram = (m_programScene == entry.name);
        bool isPreview = (m_previewScene == entry.name);

        QColor border(60, 60, 60);
        if (isProgram)      border = QColor(220, 50, 50);
        else if (isPreview) border = QColor(50, 180, 50);

        p.setPen(QPen(border, 2));
        p.setBrush(QColor(30, 30, 30));
        p.drawRect(cell.adjusted(1, 1, -1, -1));

        // Scene video frame (best-effort: grab OBS source frame)
        QRect innerCell = cell.adjusted(2, 2, -2, -18); // leave room for label
        drawSourcePreview(p, innerCell, entry.name);

        // Label bar
        QRect labelRect(cell.left(), cell.bottom() - 18,
                        cell.width(), 18);
        p.fillRect(labelRect, QColor(0, 0, 0, 180));

        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPixelSize(11);
        f.setBold(isProgram || isPreview);
        p.setFont(f);
        p.drawText(labelRect.adjusted(4, 0, -4, 0),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromStdString(entry.name));

        // PGM/PRV badge
        if (isProgram || isPreview) {
            QString badge = isProgram ? "PGM" : "PRV";
            QColor  bc    = isProgram ? QColor(220, 50, 50) : QColor(50, 180, 50);
            QRect   br(labelRect.right() - 40, labelRect.top() + 2, 36, 14);
            p.fillRect(br, bc);
            p.setPen(Qt::white);
            QFont bf = f;
            bf.setPixelSize(9);
            bf.setBold(true);
            p.setFont(bf);
            p.drawText(br, Qt::AlignCenter, badge);
        }
    }

    // Render the OBS source into a QRect using obs_source_get_frame (best-effort).
    // Falls back to a dark placeholder when the frame isn't ready.
    void drawSourcePreview(QPainter &p, const QRect &dst,
                           const std::string &sceneName)
    {
        obs_source_t *src = obs_get_source_by_name(sceneName.c_str());
        if (!src) {
            p.fillRect(dst, QColor(40, 40, 40));
            return;
        }

        // Try to get a screenshot-quality frame via obs_source_get_frame
        struct obs_source_frame *frame = obs_source_get_frame(src);
        if (frame) {
            // Convert to QImage (handle common formats)
            QImage img = frameToQImage(frame);
            obs_source_release_frame(src, frame);
            if (!img.isNull()) {
                p.drawImage(dst, img);
                obs_source_release(src);
                return;
            }
        }

        // Fallback: solid dark with source dimensions label
        p.fillRect(dst, QColor(35, 35, 40));
        p.setPen(QColor(80, 80, 80));
        uint32_t w = obs_source_get_width(src);
        uint32_t h = obs_source_get_height(src);
        if (w && h) {
            p.drawText(dst, Qt::AlignCenter,
                       QString("%1×%2").arg(w).arg(h));
        }
        obs_source_release(src);
    }

    // Convert obs_source_frame* to QImage (NV12 / I420 / BGRA / BGR3)
    static QImage frameToQImage(struct obs_source_frame *frame)
    {
        if (!frame || !frame->data[0]) return {};

        uint32_t w = frame->width;
        uint32_t h = frame->height;

        // Only handle BGRA for now (most common format OBS provides after
        // format negotiation on Windows).  Extend as needed.
        if (frame->format == VIDEO_FORMAT_BGRA) {
            return QImage(frame->data[0], w, h,
                          frame->linesize[0],
                          QImage::Format_ARGB32).copy();
        }
        if (frame->format == VIDEO_FORMAT_BGR3) {
            return QImage(frame->data[0], w, h,
                          frame->linesize[0],
                          QImage::Format_BGR888).copy();
        }
        // For other formats return null (fallback will show resolution text)
        return {};
    }

public:
    // Updated by the manager when OBS program/preview changes
    std::string m_programScene;
    std::string m_previewScene;

private:
    SceneOrderModel        *m_model;
    QObject                *m_manager;
    MultiviewLayout         m_layout;
    mutable std::vector<QRect> m_cellRects;
};
