#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  SceneOrderModel
//
//  Owns the plugin's independent scene ordering.  The canonical OBS scene list
//  is never modified; only this model is mutated for Multiview display order.
// ─────────────────────────────────────────────────────────────────────────────

#include "multiview-plus.hpp"
#include <algorithm>

class SceneOrderModel {
public:
    // ── Rebuild from OBS scene list ──────────────────────────────────────────
    //
    //  Call whenever the scene collection changes.  New scenes are appended;
    //  scenes no longer present in OBS are pruned.  The relative order of
    //  already-known scenes is preserved.
    //
    void syncWithOBS()
    {
        // Snapshot current OBS scene list (in OBS order)
        char         **obsNames = nullptr;
        size_t         obsCount = 0;
        obs_scene_t   *scene    = nullptr;

        struct obs_frontend_source_list list = {};
        obs_frontend_get_scenes(&list);

        std::vector<std::string> currentOBS;
        currentOBS.reserve(list.sources.num);
        for (size_t i = 0; i < list.sources.num; i++) {
            obs_source_t *src = list.sources.array[i];
            if (src)
                currentOBS.emplace_back(obs_source_get_name(src));
        }
        obs_frontend_source_list_free(&list);

        // Prune removed scenes from our list
        m_order.erase(
            std::remove_if(m_order.begin(), m_order.end(),
                [&](const SceneEntry &e) {
                    return std::find(currentOBS.begin(), currentOBS.end(),
                                     e.name) == currentOBS.end();
                }),
            m_order.end());

        // Append new scenes (not yet tracked)
        for (const auto &name : currentOBS) {
            bool already = std::any_of(m_order.begin(), m_order.end(),
                                       [&](const SceneEntry &e) {
                                           return e.name == name;
                                       });
            if (!already)
                m_order.push_back({name, true});
        }
    }

    // ── Accessors ────────────────────────────────────────────────────────────

    const std::vector<SceneEntry> &entries() const { return m_order; }

    // Index of scene by name (-1 if not found)
    int indexOf(const std::string &name) const
    {
        for (int i = 0; i < (int)m_order.size(); i++)
            if (m_order[i].name == name)
                return i;
        return -1;
    }

    // ── Mutation ─────────────────────────────────────────────────────────────

    // Move scene one position toward the front (lower index → displayed first)
    bool moveForward(const std::string &name)
    {
        int idx = indexOf(name);
        if (idx <= 0)
            return false;
        std::swap(m_order[idx], m_order[idx - 1]);
        return true;
    }

    // Move scene one position toward the back (higher index → displayed last)
    bool moveBackward(const std::string &name)
    {
        int idx = indexOf(name);
        if (idx < 0 || idx >= (int)m_order.size() - 1)
            return false;
        std::swap(m_order[idx], m_order[idx + 1]);
        return true;
    }

    // ── Serialization ────────────────────────────────────────────────────────

    // Returns comma-separated scene names reflecting current order
    std::string serialize() const
    {
        std::string out;
        for (size_t i = 0; i < m_order.size(); i++) {
            if (i) out += ',';
            out += m_order[i].name;
        }
        return out;
    }

    // Restore order from a comma-separated string produced by serialize().
    // Unknown names are ignored; known OBS scenes not present are appended.
    void deserialize(const std::string &csv)
    {
        std::vector<std::string> saved;
        std::string token;
        for (char c : csv) {
            if (c == ',') {
                if (!token.empty()) saved.push_back(token);
                token.clear();
            } else {
                token += c;
            }
        }
        if (!token.empty()) saved.push_back(token);

        // Build new order from saved list, preserving only existing entries
        std::vector<SceneEntry> reordered;
        reordered.reserve(m_order.size());

        for (const auto &savedName : saved) {
            int idx = indexOf(savedName);
            if (idx >= 0)
                reordered.push_back(m_order[idx]);
        }

        // Append any scenes not in the saved list
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

    void clear() { m_order.clear(); }

private:
    std::vector<SceneEntry> m_order;
};
