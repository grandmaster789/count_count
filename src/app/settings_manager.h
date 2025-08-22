#ifndef CC_APP_SETTINGS_MANAGER_H
#define CC_APP_SETTINGS_MANAGER_H

#include <filesystem>

#include "types/settings.h"

namespace cc::app {
    class SettingsManager {
    public:
        explicit SettingsManager(const std::filesystem::path& config_file = "count_count.cfg");
        ~SettingsManager();

        SettingsManager            (const SettingsManager&)     = delete;
        SettingsManager& operator= (const SettingsManager&)     = delete;
        SettingsManager            (SettingsManager&&) noexcept = delete;
        SettingsManager& operator= (SettingsManager&&) noexcept = delete;

        bool load();
        void save() const;

              Settings& get();
        const Settings& get() const;

        void reset_to_default();

        void       set_selected_color(const cv::Scalar& color);
        cv::Scalar get_selected_color() const;

    private:
        Settings              m_Settings;
        std::filesystem::path m_ConfigFile = "count_count.cfg";
    };
}

#endif
