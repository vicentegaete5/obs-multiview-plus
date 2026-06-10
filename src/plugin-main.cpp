// ─────────────────────────────────────────────────────────────────────────────
//  obs-multiview-plus — plugin entry point
// ─────────────────────────────────────────────────────────────────────────────

#include "multiview-plus.hpp"
#include "multiview-plus-manager.hpp"

// Global manager instance (owned here, destroyed on unload)
static MultiviewPlusManager *g_manager = nullptr;

// ─────────────────────────────────────────────────────────────────────────────

bool obs_module_load(void)
{
    blog(LOG_INFO, "[obs-multiview-plus] Loading plugin v%s", PLUGIN_VERSION);

    g_manager = new MultiviewPlusManager();
    g_manager->initialize();

    blog(LOG_INFO, "[obs-multiview-plus] Plugin loaded successfully");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[obs-multiview-plus] Unloading plugin");

    delete g_manager;
    g_manager = nullptr;
}

// Required for localization helper OBS_MODULE_USE_DEFAULT_LOCALE
MODULE_EXPORT const char *obs_module_description(void)
{
    return "Extends OBS Multiview with custom layout modes (Column/Row) "
           "and independent scene ordering.";
}

MODULE_EXPORT const char *obs_module_name(void)
{
    return "OBS Multiview Plus";
}
