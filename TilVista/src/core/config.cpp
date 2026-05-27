#include "config.h"
#include "pathutils.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>

QJsonObject Config::s_data;
QJsonObject Config::s_defaults = QJsonObject{
    {"auto_pick_on_dir_select", true },
    {"auto_open_on_random",     false},
    {"fullscreen_slideshow",    true },
};

void Config::load()
{
    QFile f(TV::scriptDir() + "/tilvista_config.json");
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject())
        s_data = doc.object();
}

QVariant Config::get(const QString& key)
{
    if (s_data.contains(key))
        return s_data.value(key).toVariant();
    if (s_defaults.contains(key))
        return s_defaults.value(key).toVariant();
    return QVariant();
}

bool Config::getBool(const QString& key)
{
    return get(key).toBool();
}
