#ifndef FLER_SETTINGS_HPP
#define FLER_SETTINGS_HPP

#include <QSettings>
#include <QString>

#include "config.hpp"

/* Persisted to ~/.config/Fler/Fler.conf — survives restarts of the overlay. */
namespace Settings {

  inline QString format_color(const int *rgb) {
    return QString("#%1%2%3")
        .arg(rgb[0], 2, 16, QChar('0'))
        .arg(rgb[1], 2, 16, QChar('0'))
        .arg(rgb[2], 2, 16, QChar('0'));
  }

  inline bool parse_color(const QString &value, int *rgb) {
    if (value.size() != 7 || value[0] != '#') return false;
    bool ok = false;
    const int r = value.mid(1, 2).toInt(&ok, 16);
    if (!ok) return false;
    const int g = value.mid(3, 2).toInt(&ok, 16);
    if (!ok) return false;
    const int b = value.mid(5, 2).toInt(&ok, 16);
    if (!ok) return false;
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
    return true;
  }

  inline void load_color(QSettings &s, const QString &key, int *rgb) {
    const QString value = s.value(key).toString();
    if (!value.isEmpty()) parse_color(value, rgb);
  }

  inline void save_color(QSettings &s, const QString &key, const int *rgb) {
    s.setValue(key, format_color(rgb));
  }

  inline void load() {
    QSettings s("Fler", "Fler");

    s.beginGroup("esp");
    config.esp.master = s.value("master", config.esp.master).toBool();
    config.esp.ignore_team = s.value("ignore_team", config.esp.ignore_team).toBool();
    config.esp.spotted = s.value("spotted", config.esp.spotted).toBool();
    config.esp.box = s.value("box", config.esp.box).toBool();
    config.esp.health_bar = s.value("health_bar", config.esp.health_bar).toBool();
    config.esp.health_text = s.value("health_text", config.esp.health_text).toBool();
    config.esp.name = s.value("name", config.esp.name).toBool();
    config.esp.weapon_name = s.value("weapon_name", config.esp.weapon_name).toBool();
    config.esp.skeleton = s.value("skeleton", config.esp.skeleton).toBool();
    config.esp.head_dot = s.value("head_dot", config.esp.head_dot).toBool();
    config.esp.head_dot_fill = s.value("head_dot_fill", config.esp.head_dot_fill).toBool();
    config.esp.cat_ears = s.value("cat_ears", config.esp.cat_ears).toBool();
    config.esp.snap_lines = s.value("snap_lines", config.esp.snap_lines).toBool();
    load_color(s, "box_color", config.esp.box_color);
    load_color(s, "skeleton_color", config.esp.skeleton_color);
    load_color(s, "head_dot_color", config.esp.head_dot_color);
    load_color(s, "snap_lines_color", config.esp.snap_lines_color);
    s.endGroup();

    s.beginGroup("visuals");
    config.visuals.crosshair = s.value("crosshair", config.visuals.crosshair).toBool();
    config.visuals.visualize_recoil = s.value("visualize_recoil", config.visuals.visualize_recoil).toBool();
    config.visuals.sniper_only = s.value("sniper_only", config.visuals.sniper_only).toBool();
    config.visuals.fps = s.value("fps", config.visuals.fps).toBool();
    load_color(s, "crosshair_color", config.visuals.crosshair_color);
    s.endGroup();

    s.beginGroup("aim");
    config.aim.master = s.value("master", config.aim.master).toBool();
    config.aim.ignore_team = s.value("ignore_team", config.aim.ignore_team).toBool();
    config.aim.spotted = s.value("spotted", config.aim.spotted).toBool();
    config.aim.fov = s.value("fov", config.aim.fov).toFloat();
    config.aim.show_fov = s.value("show_fov", config.aim.show_fov).toBool();
    config.aim.snap_lines = s.value("snap_lines", config.aim.snap_lines).toBool();
    config.aim.recoil = s.value("recoil", config.aim.recoil).toBool();
    config.aim.auto_shoot = s.value("auto_shoot", config.aim.auto_shoot).toBool();
    config.aim.key = (char)s.value("key", (int)config.aim.key).toInt();
    load_color(s, "snap_lines_color", config.aim.snap_lines_color);
    s.endGroup();

    s.beginGroup("theme");
    load_color(s, "accent", config.theme.accent);
    s.endGroup();

    s.beginGroup("menu");
    config.menu.toggle_key = s.value("toggle_key", config.menu.toggle_key).toInt();
    s.endGroup();
  }

  inline void save() {
    QSettings s("Fler", "Fler");

    s.beginGroup("esp");
    s.setValue("master", config.esp.master);
    s.setValue("ignore_team", config.esp.ignore_team);
    s.setValue("spotted", config.esp.spotted);
    s.setValue("box", config.esp.box);
    s.setValue("health_bar", config.esp.health_bar);
    s.setValue("health_text", config.esp.health_text);
    s.setValue("name", config.esp.name);
    s.setValue("weapon_name", config.esp.weapon_name);
    s.setValue("skeleton", config.esp.skeleton);
    s.setValue("head_dot", config.esp.head_dot);
    s.setValue("head_dot_fill", config.esp.head_dot_fill);
    s.setValue("cat_ears", config.esp.cat_ears);
    s.setValue("snap_lines", config.esp.snap_lines);
    save_color(s, "box_color", config.esp.box_color);
    save_color(s, "skeleton_color", config.esp.skeleton_color);
    save_color(s, "head_dot_color", config.esp.head_dot_color);
    save_color(s, "snap_lines_color", config.esp.snap_lines_color);
    s.endGroup();

    s.beginGroup("visuals");
    s.setValue("crosshair", config.visuals.crosshair);
    s.setValue("visualize_recoil", config.visuals.visualize_recoil);
    s.setValue("sniper_only", config.visuals.sniper_only);
    s.setValue("fps", config.visuals.fps);
    save_color(s, "crosshair_color", config.visuals.crosshair_color);
    s.endGroup();

    s.beginGroup("aim");
    s.setValue("master", config.aim.master);
    s.setValue("ignore_team", config.aim.ignore_team);
    s.setValue("spotted", config.aim.spotted);
    s.setValue("fov", config.aim.fov);
    s.setValue("show_fov", config.aim.show_fov);
    s.setValue("snap_lines", config.aim.snap_lines);
    s.setValue("recoil", config.aim.recoil);
    s.setValue("auto_shoot", config.aim.auto_shoot);
    s.setValue("key", (int)config.aim.key);
    save_color(s, "snap_lines_color", config.aim.snap_lines_color);
    s.endGroup();

    s.beginGroup("theme");
    save_color(s, "accent", config.theme.accent);
    s.endGroup();

    s.beginGroup("menu");
    s.setValue("toggle_key", config.menu.toggle_key);
    s.endGroup();

    s.sync();
  }
}

#endif
