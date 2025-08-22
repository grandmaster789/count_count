#include "settings_manager.h"

#include <fstream>

namespace cc::app {
    SettingsManager::SettingsManager(const std::filesystem::path& config_file):
        m_ConfigFile(config_file)
    {
        load();
    }

    SettingsManager::~SettingsManager() {
        save();
    }

    bool SettingsManager::load() {
        if (std::filesystem::exists(m_ConfigFile)) {
            std::ifstream cfg(m_ConfigFile);

            cfg >> m_Settings;

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

    void SettingsManager::set_selected_color(const cv::Scalar& color) {
        m_Settings.m_ForegroundColor = color;
    }

    cv::Scalar SettingsManager::get_selected_color() const {
        return m_Settings.m_ForegroundColor;
    }
}