#include "settings_manager.h"

#include <fstream>
#include <utility>

namespace cc::app {
    SettingsManager::SettingsManager(std::filesystem::path  config_file):
        m_ConfigFile(std::move(config_file))
    {
        load();
    }

    SettingsManager::~SettingsManager() {
        try {
            save();
        } catch (...) {
            // Swallow exceptions in destructor to prevent std::terminate
        }
    }

    bool SettingsManager::load() {
        if (std::filesystem::exists(m_ConfigFile)) {
            std::ifstream cfg(m_ConfigFile);
            cfg >> m_Settings;

            if (cfg.fail()) {
                m_Settings = Settings {};
                return false;
            }

            return true;
        }

        return false;
    }

    void SettingsManager::save() const {
        std::ofstream cfg(m_ConfigFile);
        cfg << m_Settings;
    }

    Settings& SettingsManager::get() {
        return m_Settings;
    }

    const Settings& SettingsManager::get() const {
        return m_Settings;
    }

    void SettingsManager::reset_to_default() {
        m_Settings = Settings {};
    }

    void SettingsManager::set_selected_color(const Color3& color) {
        m_Settings.m_ForegroundColor = color;
    }

    Color3 SettingsManager::get_selected_color() const {
        return m_Settings.m_ForegroundColor;
    }
}
