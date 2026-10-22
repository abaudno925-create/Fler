#ifndef FLER_ESP_OVERLAY_HPP
#define FLER_ESP_OVERLAY_HPP

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QTimer>
#include <QWidget>

#include "../hacks/player_info.hpp"
#include "../math.hpp"
#include "config.hpp"

/* The cheat draws with Qt instead of a raw X11 window: Xwayland has no ARGB
   visual, so an X11 overlay is always an opaque black rectangle. A wayland
   surface gets real transparency and still sits above the game, which is an
   X11 client. */
class EspOverlay : public QWidget {
public:
  EspOverlay(QWidget *parent = nullptr) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowTitle("Fler overlay");

    timer_ = new QTimer(this);
    timer_->setInterval(16);
    connect(timer_, &QTimer::timeout, this, [this] { update(); });
  }

  void start() { timer_->start(); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    drawPlayers(p);
    drawCrosshair(p);
    drawFps(p);
  }

private:
  static QColor rgb(const int *c) { return QColor(c[0], c[1], c[2]); }

  static QColor health_color(int health) {
    if (health > 100) return QColor(0x00, 0xFF, 0xF0);
    if (health >= 90) return QColor(0x00, 0xFF, 0x32);
    if (health > 60) return QColor(0xFF, 0xFF, 0x00);
    if (health > 35) return QColor(0xFF, 0xAA, 0x00);
    return QColor(0xFF, 0x00, 0x32);
  }

  void drawPlayers(QPainter &p) {
    if (!config.esp.master) return;

    const PlayerInfo::Player &local = PlayerInfo::get_local_player();

    for (int i = 0; i < 512; ++i) {
      const PlayerInfo::Player &player = PlayerInfo::l_players[i];
      if (player.index == local.index || player.index < 0) continue;
      if (player.health <= 0) continue;
      if (config.esp.ignore_team && player.team == local.team) continue;
      if (config.esp.spotted && !player.spotted) continue;

      const float distance = distance_3d(local.location, player.location);
      if (distance < 1.0f) continue;

      const Vector3 feet = {player.bone_matrix[29].x, player.bone_matrix[29].y,
                            player.bone_matrix[29].z};
      const Vector3 top_point = {feet.x, feet.y, feet.z + player.height + 9.0f};

      float bottom[2], top[2];
      if (!world_to_screen(0, feet, bottom, true)) continue;
      if (!world_to_screen(0, top_point, top, true)) continue;

      const float health_offset = (13000.0f / distance) / local.fov_multiplier;
      const float box_offset = (12000.0f / distance) / local.fov_multiplier;

      const float left = bottom[0] - box_offset;
      const float right = bottom[0] + box_offset;

      /* the box */
      if (config.esp.box) {
        p.setPen(QPen(QColor(0, 0, 0), 4));
        p.drawLine(QPointF(left, top[1]), QPointF(left, bottom[1]));
        p.drawLine(QPointF(right, top[1]), QPointF(right, bottom[1]));
        p.drawLine(QPointF(left, top[1]), QPointF(right, top[1]));
        p.drawLine(QPointF(left, bottom[1]), QPointF(right, bottom[1]));

        p.setPen(QPen(rgb(config.esp.box_color), 2));
        p.drawLine(QPointF(left, top[1]), QPointF(left, bottom[1]));
        p.drawLine(QPointF(right, top[1]), QPointF(right, bottom[1]));
        p.drawLine(QPointF(left, top[1]), QPointF(right, top[1]));
        p.drawLine(QPointF(left, bottom[1]), QPointF(right, bottom[1]));
      }

      /* the health bar */
      const float bar_x = bottom[0] - health_offset - 5.0f;
      if (config.esp.health_bar) {
        const float full = top[1] - bottom[1];
        const float ydelta = full * (1.0f - (player.health / 100.0f));
        const QColor color = health_color(player.health);

        p.setPen(QPen(QColor(0, 0, 0), 4));
        p.drawLine(QPointF(bar_x, top[1] - 2), QPointF(bar_x, bottom[1] + 2));

        p.setPen(QPen(color, 2));
        p.drawLine(QPointF(bar_x, bottom[1]), QPointF(bar_x, top[1] - ydelta));
      }

      /* the health text */
      if (config.esp.health_text) {
        drawText(p, QString::number(player.health), QPointF(bar_x - 24.0f, bottom[1]),
                 health_color(player.health), Qt::AlignRight | Qt::AlignVCenter);
      }

      /* the name */
      if (config.esp.name && !player.name.empty()) {
        drawText(p, player.name.c_str(), QPointF(bottom[0], top[1] - 5.0f), QColor(255, 255, 255));
      }

      /* the weapon */
      if (config.esp.weapon_name && player.weapon_name.size() > 7) {
        drawText(p, player.weapon_name.c_str() + 7, QPointF(bottom[0], bottom[1] + 11.0f),
                 QColor(255, 255, 255));
      }

      /* the snap lines */
      if (config.esp.snap_lines) {
        p.setPen(QPen(rgb(config.esp.snap_lines_color), 1));
        p.drawLine(QPointF(width() / 2.0, height()), QPointF(bottom[0], bottom[1]));
      }

      /* the skeleton */
      if (config.esp.skeleton) {
        p.setPen(QPen(rgb(config.esp.skeleton_color), 1));
        static const int bones[][2] = {{1, 4}, {5, 4}, {5, 6}, {1, 22}, {23, 22}, {23, 24},
                                       {1, 25}, {26, 25}, {26, 27}, {5, 8}, {9, 8}, {9, 10},
                                       {5, 13}, {14, 13}, {14, 15}};
        for (const auto &pair : bones) {
          float a[2], b[2];
          if (!world_to_screen(0, player.bone_matrix[pair[0]], a, true)) continue;
          if (!world_to_screen(0, player.bone_matrix[pair[1]], b, true)) continue;
          p.drawLine(QPointF(a[0], a[1]), QPointF(b[0], b[1]));
        }
      }

      /* the head dot */
      if (config.esp.head_dot) {
        float head[2];
        if (!world_to_screen(0, player.bone_matrix[6], head, true)) continue;

        const float size = (7500.0f / distance) / local.fov_multiplier;
        const float ear = (6500.0f / distance) / local.fov_multiplier;
        const QColor color = rgb(config.esp.head_dot_color);

        p.setPen(QPen(color, 1.6));
        if (config.esp.head_dot_fill) {
          p.setBrush(color);
        } else {
          p.setBrush(Qt::NoBrush);
        }
        p.drawEllipse(QPointF(head[0], head[1]), size / 2.0f, size / 2.0f);
        p.setBrush(Qt::NoBrush);

        if (config.esp.cat_ears) {
          p.setPen(QPen(color, 1.6));
          p.drawLine(QPointF(head[0] + size / 2.0f - 1, head[1]),
                     QPointF(head[0] + size / 2.0f - 1, head[1] - ear));
          p.drawLine(QPointF(head[0], head[1] - size / 2.0f),
                     QPointF(head[0] + size / 2.0f - 1, head[1] - ear));
          p.drawLine(QPointF(head[0] - size / 2.0f, head[1]),
                     QPointF(head[0] - size / 2.0f, head[1] - ear));
          p.drawLine(QPointF(head[0], head[1] - size / 2.0f),
                     QPointF(head[0] - size / 2.0f, head[1] - ear));
        }
      }
    }
  }

  void drawCrosshair(QPainter &p) {
    if (!config.visuals.crosshair) return;

    const PlayerInfo::Player &local = PlayerInfo::get_local_player();
    if (config.visuals.sniper_only) {
      const std::string &weapon = local.weapon_name;
      if (weapon.find("awp") == std::string::npos && weapon.find("g3sg1") == std::string::npos &&
          weapon.find("scout") == std::string::npos) {
        return;
      }
    }

    const QColor color = rgb(config.visuals.crosshair_color);
    const float cx = width() / 2.0f;
    const float cy = height() / 2.0f;
    const float gap = 4.0f;
    const float len = 8.0f;

    p.setPen(QPen(QColor(0, 0, 0), 3));
    p.drawLine(QPointF(cx - gap - len, cy), QPointF(cx - gap, cy));
    p.drawLine(QPointF(cx + gap, cy), QPointF(cx + gap + len, cy));
    p.drawLine(QPointF(cx, cy - gap - len), QPointF(cx, cy - gap));
    p.drawLine(QPointF(cx, cy + gap), QPointF(cx, cy + gap + len));

    p.setPen(QPen(color, 1.4));
    p.drawLine(QPointF(cx - gap - len, cy), QPointF(cx - gap, cy));
    p.drawLine(QPointF(cx + gap, cy), QPointF(cx + gap + len, cy));
    p.drawLine(QPointF(cx, cy - gap - len), QPointF(cx, cy - gap));
    p.drawLine(QPointF(cx, cy + gap), QPointF(cx, cy + gap + len));
  }

  void drawFps(QPainter &p) {
    if (!config.visuals.fps) return;
    drawText(p, QString("FPS: %1").arg(fps_), QPointF(8, 18), QColor(255, 255, 255),
             Qt::AlignLeft | Qt::AlignVCenter, 11);
  }

  void drawText(QPainter &p, const QString &text, const QPointF &at, const QColor &color,
                int flags = Qt::AlignCenter, int pixel_size = 12) {
    QFont font = p.font();
    font.setPixelSize(pixel_size);
    font.setWeight(QFont::DemiBold);
    p.setFont(font);

    const QRectF bounds(at.x() - 120, at.y() - 9, 240, 18);
    p.setPen(QColor(0, 0, 0, 170));
    p.drawText(bounds.translated(1, 1), flags, text);
    p.setPen(color);
    p.drawText(bounds, flags, text);
  }

  QTimer *timer_ = nullptr;
  int fps_ = 0;
  uint64_t frames_ = 0;
  uint64_t last_fps_ = 0;
};

#endif
