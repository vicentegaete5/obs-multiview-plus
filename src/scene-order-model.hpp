#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  SceneOrderModel
//
//  Owns the plugin's independent scene ordering AND per-scene visibility.
//  The canonical OBS scene list is never modified.
//
//  Visibility covers ALL multiview slots including the Preview, Program and
//  blank scene slots that OBS renders natively — these are tracked with the
//  reserved names  __preview__  and  __program__  so they can be hidden too.
// ─────────────────────────────────────────────────────────────────────────────

#include "multiview-plus.hpp"
#include <algorithm>

// Reserved slot names for the non-scene cells OBS always shows in Multiview
static constexpr const char *SLOT_PREVIEW = "__preview__";
static constexpr const char *SLOT_PROGRAM = "__program__";

class SceneOrderModel {
public:
    // ── Rebuild from OBS scene list ──────────────────────────────────────────
    void syncWithOBS()
    {
        struct obs_frontend_source_list list = {};
        obs_frontend_get_scenes(&list);

        std::vector<std::string> currentOBS;
        currentOBS.reserve(list.sources.num + 2);

        // Always include the two fixed OBS multiview slots first
        currentOBS.emplace_back(SLOT_PREVIEW);
        currentOBS.emplace_back(SLOT_PROGRAM);

        for (size_t i = 0; i < list.sources.num; i++) {
            obs_source_t *src = list.sources.array[i];
            if (src)
                currentOBS.emplace_back(obs_source_get_name(src));
        }
        obs_frontend_source_list_free(&list);

        // Prune entries no longer in OBS
        m_order.erase(
            std::remove_if(m_order.begin(), m_order.end(),
                [&](const SceneEntry &e) {
                    return std::find(currentOBS.begin(), currentOBS.end(),
                                     e.name) == currentOBS.end();
                }),
            m_order.end());

        // Append new entries (preserve existing visible state)
        for (const auto &name : currentOBS) {
            bool already = std::any_of(m_order.begin(), m_order.end(),
                                       [&](const SceneEntry &e) {
                                           return e.name == name;
                                       });
            if (!already)
                m_order.push_back({name, true /*visible by default*/});
        }
    }

    // ── Accessors ────────────────────────────────────────────────────────────

    // All entries regardless of visibility (used for ordering operations)
    const std::vector<SceneEntry> &entries() const { return m_order; }

    // Only visible entries (used by the overlay renderer)
    std::vector<SceneEntry> visibleEntries() const
    {
        std::vector<SceneEntry> out;
        out.reserve(m_order.size());
        for (const auto &e : m_order)
            if (e.visible)
                out.push_back(e);
        return out;
    }

    bool anyHidden() const
    {
        return std::any_of(m_order.begin(), m_order.end(),
                           [](const SceneEntry &e) { return !e.visible; });
    }

    int indexOf(const std::string &name) const
    {
        for (int i = 0; i < (int)m_order.size(); i++)
            if (m_order[i].name == name)
                return i;
        return -1;
    }

    bool isVisible(const std::string &name) const
    {
        int idx = indexOf(name);
        return idx >= 0 ? m_order[idx].visible : true;
    }

    // ── Mutation ─────────────────────────────────────────────────────────────

    bool moveForward(const std::string &name)
    {
        int idx = indexOf(name);
        if (idx <= 0) return false;
        std::swap(m_order[idx], m_order[idx - 1]);
        return true;
    }

    bool moveBackward(const std::string &name)
    {
        int idx = indexOf(name);
        if (idx < 0 || idx >= (int)m_order.size() - 1) return false;
        std::swap(m_order[idx], m_order[idx + 1]);
        return true;
    }

    // Hide a specific scene slot from Multiview
    bool hide(const std::string &name)
    {
        int idx = indexOf(name);
        if (idx < 0 || !m_order[idx].visible) return false;
        m_order[idx].visible = false;
        return true;
    }

    // Make every scene slot visible again
    void showAll()
    {
        for (auto &e : m_order)
            e.visible = true;
    }

    // ── Serialization ────────────────────────────────────────────────────────

    // Order: comma-separated names (all entries, visible and hidden)
    std::string serializeOrder() const
    {
        std::string out;
        for (size_t i = 0; i < m_order.size(); i++) {
            if (i) out += ',';
            out += escapeName(m_order[i].name);
        }
        return out;
    }

    // Hidden set: comma-separated names of hidden entries only
    std::string serializeHidden() const
    {
        std::string out;
        bool first = true;
        for (const auto &e : m_order) {
            if (!e.visible) {
                if (!first) out += ',';
                out += escapeName(e.name);
                first = false;
            }
        }
        return out;
    }

    void deserializeOrder(const std::string &csv)
    {
        auto saved = splitCSV(csv);

        std::vector<SceneEntry> reordered;
        reordered.reserve(m_order.size());

        for (const auto &savedName : saved) {
            int idx = indexOf(savedName);
            if (idx >= 0)
                reordered.push_back(m_order[idx]);
        }
        for (const auto &entry : m_order) {
            bool already = std::any_of(reordered.begin(), reordered.end(),
                                       [&](const SceneEntry &e) {
                                           return e.name == entry.name;
                                       });
            if (!already)
                reordered.push_back(entry);
        }
        m_order = std::move(reordered);
    }

    void deserializeHidden(const std::string &csv)
    {
        // First make everything visible, then re-hide what was saved
        for (auto &e : m_order) e.visible = true;
        for (const auto &name : splitCSV(csv)) {
            int idx = indexOf(name);
            if (idx >= 0)
                m_order[idx].visible = false;
        }
    }

    // Legacy: single-field deserialize (order only, no visibility — upgrade path)
    void deserialize(const std::string &csv) { deserializeOrder(csv); }

    void clear() { m_order.clear(); }

private:
    std::vector<SceneEntry> m_order;

    // Escape commas in names as \, so CSV round-trips safely
    static std::string escapeName(const std::string &s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == ',')  out += "\\,";
            else if (c == '\\') out += "\\\\";
            else out += c;
        }
        return out;
    }

    static std::vector<std::string> splitCSV(const std::string &csv)
    {
        std::vector<std::string> result;
        std::string token;
        for (size_t i = 0; i < csv.size(); i++) {
            if (csv[i] == '\\' && i + 1 < csv.size()) {
                token += csv[++i]; // escaped character
            } else if (csv[i] == ',') {
                if (!token.empty()) result.push_back(token);
                token.clear();
            } else {
                token += csv[i];
            }
        }
        if (!token.empty()) result.push_back(token);
        return result;
    }
};
