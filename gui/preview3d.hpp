#ifndef FLER_PREVIEW3D_HPP
#define FLER_PREVIEW3D_HPP

#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QPointF>
#include <QStandardPaths>
#include <QTimer>
#include <QVector3D>
#include <QWidget>

#include <algorithm>
#include <cmath>

#include "config.hpp"
#include "glb.hpp"
#include "theme.hpp"
#include "widgets.hpp"

/* ESP preview: the real Counter-Strike agents rendered in software, with the
   overlay drawn on top exactly like the live cheat draws it. The meshes come
   from the local CS2 install (see tools/fetch_models.sh); when they are not
   around the widget falls back to drawn stand-ins. */
class EspPreview : public QWidget, public Paintable {
public:
  explicit EspPreview(QWidget *parent = nullptr) : QWidget(parent) {
    setObjectName("preview");
    setMinimumHeight(380);

    previews().push_back(this);

    loadModels();

    spin_ = new QTimer(this);
    spin_->setInterval(100);
    connect(spin_, &QTimer::timeout, this, [this] {
      if (!isVisible() || !models_loaded_) return;
      phase_ += 0.045;
      update();
    });
    spin_->start();
  }

  ~EspPreview() override {
    auto &list = previews();
    list.erase(std::remove(list.begin(), list.end(), this), list.end());
  }

  void refresh() override { update(); }

  static QVector<QWidget *> &previews() {
    static QVector<QWidget *> list;
    return list;
  }

  static void refresh_all() {
    for (QWidget *w : previews()) w->update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = rect();
    drawBackdrop(p, area);

    const double base_y = area.bottom() - area.height() * 0.12;
    const double model_h = area.height() * 0.64;

    if (models_loaded_) {
      drawAgent(p, ct_, QPointF(area.left() + area.width() * 0.29, base_y), model_h, false);
      drawAgent(p, t_, QPointF(area.left() + area.width() * 0.71, base_y), model_h, true);
    } else {
      drawStandIn(p, QPointF(area.left() + area.width() * 0.29, base_y), model_h, false);
      drawStandIn(p, QPointF(area.left() + area.width() * 0.71, base_y), model_h, true);
    }
  }

private:
  /* ---------------- assets ---------------- */

  static QString findAsset(const QString &name) {
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/assets/" + name,
        QDir::currentPath() + "/assets/" + name,
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + name,
    };
    for (const QString &path : candidates) {
      if (QFileInfo::exists(path)) return path;
    }
    return QString();
  }

  void loadModels() {
    const QString ct_path = findAsset("ct.glb");
    const QString t_path = findAsset("t.glb");
    if (ct_path.isEmpty() || t_path.isEmpty()) return;

    models_loaded_ = Glb::load(ct_path, ct_) && Glb::load(t_path, t_);
    if (!models_loaded_) return;

    decimate(ct_, 4200);
    decimate(t_, 4200);
  }

  /* the preview only shows the agents at a couple hundred pixels, so the
     small triangles are pure cost — drop them once, at load time */
  static void decimate(Model3D &model, int keep) {
    struct Candidate {
      float area;
      int part;
      int index;
    };
    QVector<Candidate> candidates;

    for (int part = 0; part < model.parts.size(); ++part) {
      const Mesh3D &mesh = model.parts[part];
      for (int i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t ia = mesh.indices[i];
        const uint32_t ib = mesh.indices[i + 1];
        const uint32_t ic = mesh.indices[i + 2];
        if (ia >= (uint32_t)mesh.positions.size() || ib >= (uint32_t)mesh.positions.size() ||
            ic >= (uint32_t)mesh.positions.size()) {
          continue;
        }
        const QVector3D e1 = mesh.positions[ib] - mesh.positions[ia];
        const QVector3D e2 = mesh.positions[ic] - mesh.positions[ia];
        candidates.push_back({QVector3D::crossProduct(e1, e2).length(), part, i});
      }
    }

    if (candidates.size() <= keep) return;

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.area > b.area; });
    candidates.resize(keep);

    QVector<QVector<uint32_t>> kept(model.parts.size());
    for (const Candidate &c : candidates) {
      const Mesh3D &mesh = model.parts[c.part];
      kept[c.part].push_back(mesh.indices[c.index]);
      kept[c.part].push_back(mesh.indices[c.index + 1]);
      kept[c.part].push_back(mesh.indices[c.index + 2]);
    }

    for (int part = 0; part < model.parts.size(); ++part) {
      model.parts[part].indices = kept[part];
    }
  }

  /* ---------------- scene ---------------- */

  void drawBackdrop(QPainter &p, const QRectF &area) const {
    const double horizon = area.top() + area.height() * 0.64;

    QLinearGradient sky(0, area.top(), 0, horizon);
    sky.setColorAt(0.0, QColor(0x17, 0x1D, 0x27));
    sky.setColorAt(1.0, QColor(0x0D, 0x11, 0x18));
    p.fillRect(QRectF(area.left(), area.top(), area.width(), horizon - area.top()), sky);

    QLinearGradient floor(0, horizon, 0, area.bottom());
    floor.setColorAt(0.0, QColor(0x0C, 0x0F, 0x15));
    floor.setColorAt(1.0, QColor(0x06, 0x08, 0x0B));
    p.fillRect(QRectF(area.left(), horizon, area.width(), area.bottom() - horizon), floor);

    p.setPen(QPen(QColor(0x1C, 0x23, 0x2E), 1));
    const QPointF vanish(area.center().x(), horizon);
    for (int i = -9; i <= 9; ++i) {
      p.drawLine(vanish, QPointF(area.center().x() + i * area.width() * 0.16, area.bottom()));
    }
    for (int i = 1; i <= 6; ++i) {
      const double t = i / 6.0;
      const double y = horizon + (area.bottom() - horizon) * t * t;
      p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }
    p.setPen(QPen(QColor(0x27, 0x30, 0x3D), 1));
    p.drawLine(QPointF(area.left(), horizon), QPointF(area.right(), horizon));
  }

  /* ---------------- 3D agent ---------------- */

  struct Target {
    QRectF box;
    QPointF head;
    QPointF head_top;
    QVector<QPointF> bones; /* pairs */
    double health = 100.0;
    QString name;
    QString weapon;
  };

  /* horizontal extremes of the model inside a height band */
  struct Band {
    bool has = false;
    float min_x = 0.0f;
    float max_x = 0.0f;
    QVector3D sum;
    int count = 0;

    float edge(bool right) const { return right ? max_x : min_x; }
  };

  static QVector3D rotateY(const QVector3D &p, double degrees) {
    const double rad = degrees * M_PI / 180.0;
    const double c = std::cos(rad);
    const double s = std::sin(rad);
    return QVector3D(float(p.x() * c + p.z() * s), p.y(), float(-p.x() * s + p.z() * c));
  }

  static QVector3D tilt(const QVector3D &p, double degrees) {
    const double rad = degrees * M_PI / 180.0;
    const double c = std::cos(rad);
    const double s = std::sin(rad);
    return QVector3D(p.x(), float(p.y() * c - p.z() * s), float(p.y() * s + p.z() * c));
  }

  void drawAgent(QPainter &p, const Model3D &model, const QPointF &anchor, double target_height,
                 bool terrorist) {
    if (!model.valid) return;

    const double dist = 4.0;
    const double height = model.height();
    if (height <= 0.0f) return;

    const double focal = target_height * dist / height;
    const double mid_y = (model.min_y + model.max_y) / 2.0;

    /* anchors are picked in the rotated frame so they follow the model */
    Band head, shoulder, hip, knee, foot;

    QVector<QVector3D> rotated;
    rotated.reserve(64);

    for (const Mesh3D &part : model.parts) {
      for (const QVector3D &v : part.positions) {
        const QVector3D r = rotateY(v, angle());
        rotated.push_back(r);

        const float t = (r.y() - model.min_y) / height;
        auto feed = [&](Band &band) {
          if (!band.has) {
            band.has = true;
            band.min_x = band.max_x = r.x();
          }
          band.min_x = qMin(band.min_x, r.x());
          band.max_x = qMax(band.max_x, r.x());
          band.sum += r;
          ++band.count;
        };

        if (t > 0.86) {
          feed(head);
        } else if (t > 0.74 && t < 0.84) {
          feed(shoulder);
        } else if (t > 0.46 && t < 0.56) {
          feed(hip);
        } else if (t > 0.22 && t < 0.30) {
          feed(knee);
        } else if (t < 0.04) {
          feed(foot);
        }
      }
    }

    const QRectF area = rect();
    const double cx = anchor.x();
    const double cy = anchor.y() - target_height * 0.5;

    auto project = [&](const QVector3D &world, float *depth = nullptr) {
      QVector3D v = world;
      v.setY(v.y() - mid_y);
      v = tilt(v, -6.0);
      const float d = float(dist) - v.z();
      if (depth) *depth = d;
      if (d <= 0.05f) return QPointF();
      return QPointF(cx + focal * v.x() / d, cy - focal * v.y() / d);
    };

    struct Tri {
      QPointF a, b, c;
      float depth;
      QColor color;
    };
    QVector<Tri> triangles;

    const QVector3D light = QVector3D(-0.45f, 0.72f, 0.52f).normalized();
    const QVector<QColor> palette = terrorist
        ? QVector<QColor>{QColor(0xA9, 0x8C, 0x60), QColor(0x8C, 0x74, 0x4E),
                          QColor(0x7E, 0x62, 0x3E), QColor(0xB8, 0x9C, 0x70)}
        : QVector<QColor>{QColor(0x5C, 0x70, 0x8C), QColor(0x42, 0x51, 0x68),
                          QColor(0x33, 0x3E, 0x50), QColor(0x6B, 0x82, 0xA2)};

    for (int part_index = 0; part_index < model.parts.size(); ++part_index) {
      const Mesh3D &part = model.parts[part_index];
      const QColor base = palette[part_index % palette.size()];

      for (int i = 0; i + 2 < part.indices.size(); i += 3) {
        const uint32_t ia = part.indices[i];
        const uint32_t ib = part.indices[i + 1];
        const uint32_t ic = part.indices[i + 2];
        if (ia >= (uint32_t)part.positions.size() || ib >= (uint32_t)part.positions.size() ||
            ic >= (uint32_t)part.positions.size()) {
          continue;
        }

        const QVector3D wa = rotateY(part.positions[ia], angle());
        const QVector3D wb = rotateY(part.positions[ib], angle());
        const QVector3D wc = rotateY(part.positions[ic], angle());

        QVector3D normal = QVector3D::crossProduct(wb - wa, wc - wa);
        if (normal.isNull()) continue;
        normal.normalize();

        float da = 0, db = 0, dc = 0;
        const QPointF a = project(wa, &da);
        const QPointF b = project(wb, &db);
        const QPointF c = project(wc, &dc);
        if (a.isNull() || b.isNull() || c.isNull()) continue;

        /* skip the sub pixel triangles, they cost more than they show */
        const double area_px =
            qAbs((b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y())) * 0.5;
        if (area_px < 1.2) continue;

        const float lambert = qMax(0.0f, QVector3D::dotProduct(normal, light));
        const float shade = 0.46f + 0.72f * lambert;

        Tri tri;
        tri.a = a;
        tri.b = b;
        tri.c = c;
        tri.depth = (da + db + dc) / 3.0f;
        tri.color = QColor::fromRgbF(qBound(0.0, base.redF() * shade, 1.0),
                                     qBound(0.0, base.greenF() * shade, 1.0),
                                     qBound(0.0, base.blueF() * shade, 1.0));
        triangles.push_back(tri);
      }
    }

    std::sort(triangles.begin(), triangles.end(),
              [](const Tri &x, const Tri &y) { return x.depth > y.depth; });

    for (const Tri &tri : triangles) {
      p.setPen(Qt::NoPen);
      p.setBrush(tri.color);
      p.drawPolygon(QPolygonF() << tri.a << tri.b << tri.c);
    }

    /* ---- anchors for the overlay ---- */
    Target target;
    target.health = terrorist ? 62.0 : 100.0;
    target.name = terrorist ? "Dragomir" : "Phoenix";
    target.weapon = terrorist ? "AK-47" : "M4A1-S";

    if (head.has && head.count) {
      const QVector3D center = head.sum / float(head.count);
      target.head = project(center + QVector3D(0, height * 0.012f, 0));
      target.head_top = project(center + QVector3D(0, height * 0.055f, 0));
    }

    double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
    for (const QVector3D &r : rotated) {
      const QPointF s = project(r);
      if (s.isNull()) continue;
      min_x = qMin(min_x, s.x());
      max_x = qMax(max_x, s.x());
      min_y = qMin(min_y, s.y());
      max_y = qMax(max_y, s.y());
    }
    if (min_x > max_x) return;

    target.box = QRectF(QPointF(min_x, min_y), QPointF(max_x, max_y)).adjusted(-4, -4, 4, 4);

    QVector<QPointF> bones;
    auto push = [&](const QPointF &a, const QPointF &b) {
      if (!a.isNull() && !b.isNull()) {
        bones.push_back(a);
        bones.push_back(b);
      }
    };

    if (head.has && shoulder.has && hip.has && knee.has && foot.has) {
      const float unit = float(height);
      const QVector3D head_center = head.sum / float(head.count);

      /* the widest point of a band is the arm, the joint sits inside it */
      auto band_point = [&](const Band &band, bool right, float y) {
        const float axis = band.sum.x() / float(band.count);
        const float edge = band.edge(right);
        const float joint = axis + (edge - axis) * 0.58f;
        return project(QVector3D(joint, y, 0.0f));
      };

      push(target.head, project(QVector3D(head_center.x(), unit * 0.86f, head_center.z())));
      push(band_point(shoulder, false, unit * 0.80f), band_point(shoulder, true, unit * 0.80f));
      push(band_point(shoulder, false, unit * 0.80f), band_point(hip, false, unit * 0.52f));
      push(band_point(shoulder, true, unit * 0.80f), band_point(hip, true, unit * 0.52f));
      push(band_point(hip, false, unit * 0.52f), band_point(knee, false, unit * 0.27f));
      push(band_point(hip, true, unit * 0.52f), band_point(knee, true, unit * 0.27f));
      push(band_point(knee, false, unit * 0.27f), band_point(foot, false, unit * 0.02f));
      push(band_point(knee, true, unit * 0.27f), band_point(foot, true, unit * 0.02f));
    }
    target.bones = bones;

    drawTarget(p, target, terrorist);
  }

  /* ---------------- overlay ---------------- */

  void drawTarget(QPainter &p, const Target &target, bool terrorist) {
    const QRectF &box = target.box;

    if (config.esp.snap_lines) {
      p.setPen(QPen(QColor(config.esp.snap_lines_color[0], config.esp.snap_lines_color[1],
                           config.esp.snap_lines_color[2]),
                    1.2));
      p.drawLine(QPointF(rect().center().x(), rect().bottom()), target.head);
    }

    if (config.esp.box) {
      p.setPen(QPen(QColor(config.esp.box_color[0], config.esp.box_color[1],
                           config.esp.box_color[2]),
                    1.6));
      p.setBrush(Qt::NoBrush);
      p.drawRect(box);
    }

    if (config.esp.skeleton) {
      p.setPen(QPen(QColor(config.esp.skeleton_color[0], config.esp.skeleton_color[1],
                           config.esp.skeleton_color[2]),
                    1.4));
      for (int i = 0; i + 1 < target.bones.size(); i += 2) {
        p.drawLine(target.bones[i], target.bones[i + 1]);
      }
    }

    if (config.esp.head_dot) {
      const QColor c(config.esp.head_dot_color[0], config.esp.head_dot_color[1],
                     config.esp.head_dot_color[2]);
      const double head_r = qMax(3.0, QLineF(target.head, target.head_top).length() * 0.42);

      p.setPen(QPen(c, 1.6));
      p.setBrush(config.esp.head_dot_fill ? c : Qt::NoBrush);
      p.drawEllipse(target.head, head_r * 0.42, head_r * 0.42);

      if (config.esp.cat_ears) {
        p.setBrush(Qt::NoBrush);
        QPainterPath earL, earR;
        earL.moveTo(target.head.x() - head_r * 0.9, target.head.y() - head_r * 0.5);
        earL.lineTo(target.head.x() - head_r * 0.2, target.head.y() - head_r * 2.0);
        earL.lineTo(target.head.x() + head_r * 0.3, target.head.y() - head_r * 0.9);
        earL.closeSubpath();
        earR.moveTo(target.head.x() + head_r * 0.9, target.head.y() - head_r * 0.5);
        earR.lineTo(target.head.x() + head_r * 0.2, target.head.y() - head_r * 2.0);
        earR.lineTo(target.head.x() - head_r * 0.3, target.head.y() - head_r * 0.9);
        earR.closeSubpath();
        p.drawPath(earL);
        p.drawPath(earR);
      }
    }

    if (config.esp.health_bar) {
      const double bar_w = 4.0;
      const double bar_x = box.left() - 9.0;
      const double fill_h = box.height() * (target.health / 100.0);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0x14, 0x18, 0x1F));
      p.drawRect(QRectF(bar_x, box.top(), bar_w, box.height()));

      QColor bar_color(0xFF, 0x00, 0x32);
      if (target.health > 100.0) {
        bar_color = QColor(0x00, 0xFF, 0xF0);
      } else if (target.health >= 90.0) {
        bar_color = QColor(0x00, 0xFF, 0x32);
      } else if (target.health > 60.0) {
        bar_color = QColor(0xFF, 0xFF, 0x00);
      } else if (target.health > 35.0) {
        bar_color = QColor(0xFF, 0xAA, 0x00);
      }
      p.setBrush(bar_color);
      p.drawRect(QRectF(bar_x, box.bottom() - fill_h, bar_w, fill_h));
    }

    if (config.esp.health_text) {
      drawText(p, QPointF(box.left() - 14.0, box.center().y()),
               QString::number((int)target.health), QColor(0xFF, 0xFF, 0xFF),
               Qt::AlignRight | Qt::AlignVCenter);
    }

    if (config.esp.name) {
      drawText(p, QPointF(box.center().x(), box.top() - 6.0), target.name, QColor(0xFF, 0xFF, 0xFF));
    }

    if (config.esp.weapon_name) {
      drawText(p, QPointF(box.center().x(), box.bottom() + 14.0), target.weapon,
               QColor(0xFF, 0xC8, 0x50));
    }
  }

  static void drawText(QPainter &p, const QPointF &pos, const QString &text, const QColor &c,
                       int flags = Qt::AlignCenter) {
    QFont f = p.font();
    f.setPixelSize(11);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);

    const QRectF bounds(pos.x() - 120, pos.y() - 11, 240, 22);
    p.setPen(QColor(0, 0, 0, 160));
    p.drawText(bounds.translated(1, 1), flags, text);
    p.setPen(c);
    p.drawText(bounds, flags, text);
  }

  /* ---------------- fallback stand-ins ---------------- */

  void drawStandIn(QPainter &p, const QPointF &feet, double height, bool terrorist) {
    struct Joints {
      QPointF head, neck, shoulderL, shoulderR, elbowL, elbowR, handL, handR;
      QPointF hipL, hipR, kneeL, kneeR, footL, footR;
    };

    const double s = height / 120.0;
    auto P = [&](double x, double y) { return QPointF(feet.x() + x * s, feet.y() + y * s); };
    Joints j;
    j.head = P(0, -110);
    j.neck = P(0, -100);
    j.shoulderL = P(-11, -96);
    j.shoulderR = P(11, -96);
    j.elbowL = P(-15, -76);
    j.elbowR = P(15, -76);
    j.handL = P(-16, -58);
    j.handR = P(16, -58);
    j.hipL = P(-6, -60);
    j.hipR = P(6, -60);
    j.kneeL = P(-7, -30);
    j.kneeR = P(7, -30);
    j.footL = P(-8, -1);
    j.footR = P(8, -1);

    const QColor shirt = terrorist ? QColor(0xB3, 0x9A, 0x72) : QColor(0x3D, 0x5A, 0x8A);
    const QColor pants = terrorist ? QColor(0x6B, 0x57, 0x40) : QColor(0x2B, 0x36, 0x48);
    const QColor vest = terrorist ? QColor(0x6E, 0x5A, 0x3E) : QColor(0x22, 0x30, 0x47);
    const QColor skin(0xC8, 0xA0, 0x7C);
    const QColor gear = terrorist ? QColor(0x2E, 0x2A, 0x24) : QColor(0x26, 0x37, 0x4E);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 90));
    p.drawEllipse(QRectF(feet.x() - 22 * s, feet.y() - 5 * s, 44 * s, 10 * s));

    if (terrorist) {
      p.setBrush(QColor(0x7A, 0x4B, 0x33));
      p.drawRoundedRect(QRectF(j.shoulderL.x() - 7 * s, j.shoulderL.y() + 2 * s, 13 * s, 25 * s),
                        3 * s, 3 * s);
    }

    p.setPen(QPen(pants, 7 * s, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(j.hipL, j.kneeL);
    p.drawLine(j.kneeL, j.footL);
    p.drawLine(j.hipR, j.kneeR);
    p.drawLine(j.kneeR, j.footR);

    QPainterPath torso;
    torso.moveTo(j.shoulderL);
    torso.lineTo(j.shoulderR);
    torso.lineTo(QPointF(j.hipR.x() + s, j.hipR.y() + 3 * s));
    torso.lineTo(QPointF(j.hipL.x() - s, j.hipL.y() + 3 * s));
    torso.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(shirt);
    p.drawPath(torso);

    p.setBrush(vest);
    p.drawRoundedRect(
        QRectF(j.shoulderL.x() + 2.5 * s, j.shoulderL.y() + 4 * s,
               (j.shoulderR.x() - j.shoulderL.x()) - 5 * s, 22 * s),
        2 * s, 2 * s);

    p.setPen(QPen(shirt, 5.5 * s, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(j.shoulderL, j.elbowL);
    p.drawLine(j.shoulderR, j.elbowR);
    p.setPen(QPen(skin, 4.5 * s, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(j.elbowL, j.handL);
    p.drawLine(j.elbowR, j.handR);

    p.setPen(Qt::NoPen);
    p.setBrush(skin);
    p.drawEllipse(j.head, 8.5 * s, 9 * s);

    if (!terrorist) {
      p.setBrush(gear);
      p.drawPie(QRectF(j.head.x() - 9.5 * s, j.head.y() - 11 * s, 19 * s, 22 * s), 0, 180 * 16);
    } else {
      p.setBrush(gear);
      p.drawEllipse(j.head, 8.5 * s, 9 * s);
    }

    Target target;
    target.health = terrorist ? 62.0 : 100.0;
    target.name = terrorist ? "Dragomir" : "Phoenix";
    target.weapon = terrorist ? "AK-47" : "M4A1-S";
    target.head = j.head;
    target.head_top = P(0, -122);

    const QList<QPointF> points = {j.head, j.neck, j.shoulderL, j.shoulderR, j.elbowL,
                                   j.elbowR, j.handL, j.handR, j.hipL, j.hipR, j.kneeL,
                                   j.kneeR, j.footL, j.footR};
    double min_x = j.head.x(), max_x = j.head.x(), min_y = j.head.y(), max_y = j.head.y();
    for (const QPointF &pt : points) {
      min_x = qMin(min_x, pt.x());
      max_x = qMax(max_x, pt.x());
      min_y = qMin(min_y, pt.y());
      max_y = qMax(max_y, pt.y());
    }
    target.box = QRectF(QPointF(min_x, min_y), QPointF(max_x, max_y)).adjusted(-8 * s, -13 * s,
                                                                             8 * s, 4 * s);

    QVector<QPointF> bones;
    auto push = [&](const QPointF &a, const QPointF &b) {
      bones.push_back(a);
      bones.push_back(b);
    };
    push(j.head, j.neck);
    push(j.neck, j.shoulderL);
    push(j.neck, j.shoulderR);
    push(j.shoulderL, j.hipL);
    push(j.shoulderR, j.hipR);
    push(j.hipL, j.kneeL);
    push(j.hipR, j.kneeR);
    push(j.kneeL, j.footL);
    push(j.kneeR, j.footR);
    target.bones = bones;

    drawTarget(p, target, terrorist);
  }

  Model3D ct_;
  Model3D t_;
  bool models_loaded_ = false;
  double phase_ = 0.0;
  QTimer *spin_ = nullptr;

  /* the agents are modelled facing away from the camera, so the base angle
     turns them around; the idle sway keeps a bit of life in the preview */
  double angle() const { return 198.0 + 13.0 * std::sin(phase_); }
};

#endif
