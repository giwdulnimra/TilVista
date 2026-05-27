#pragma once
#include <QJsonObject>
#include <QString>
#include <QVariant>

/// Reads tilvista_config.json once at startup.
/// All values are read-only from the GUI – changed only via the file.
///
/// Default values:
///   auto_pick_on_dir_select : true
///   auto_open_on_random     : false
///   fullscreen_slideshow    : true
class Config
{
public:
    static void    load();
    static QVariant get(const QString& key);

    // Typed convenience getters
    static bool getBool(const QString& key);

private:
    static QJsonObject s_data;
    static QJsonObject s_defaults;
};
