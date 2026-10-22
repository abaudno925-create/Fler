#ifndef FLER_WIDGETS_HPP
#define FLER_WIDGETS_HPP

#include <QAbstractButton>
#include <QColorDialog>
#include <QFocusEvent>
#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWindow>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QVariantAnimation>
#include <QWidget>

#include <functional>
#include <vector>

#include <algorithm>

#include "theme.hpp"

/* Widgets that paint themselves register here so a theme change can repaint them. */
class Paintable {
public:
  Paintable() { paintables().push_back(this); }

  virtual ~Paintable() {
    std::vector<Paintable *> &v = paintables();
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
  }

  virtual void refresh() = 0;

  static std::vector<Paintable *> &paintables() {
    static std::vector<Paintable *> list;
    return list;
  }
};

/* Line icons drawn with QPainter — no image assets to ship. */
inline void draw_glyph(QPainter &p, const QRectF &r, int id, const QColor &c) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);
  QPen pen(c, 1.6);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);

  const double w = r.width();
  const double h = r.height();

  if (id == 0) { /* ESP — corner brackets */
    const double k = w * 0.34;
    p.drawLine(r.left(), r.top(), r.left() + k, r.top());
    p.drawLine(r.left(), r.top(), r.left(), r.top() + k);
    p.drawLine(r.right(), r.top(), r.right() - k, r.top());
    p.drawLine(r.right(), r.top(), r.right(), r.top() + k);
    p.drawLine(r.left(), r.bottom(), r.left() + k, r.bottom());
    p.drawLine(r.left(), r.bottom(), r.left(), r.bottom() - k);
    p.drawLine(r.right(), r.bottom(), r.right() - k, r.bottom());
    p.drawLine(r.right(), r.bottom(), r.right(), r.bottom() - k);
  } else if (id == 1) { /* Aimbot — target */
    p.drawEllipse(r.center(), w * 0.34, w * 0.34);
    p.drawEllipse(r.center(), w * 0.12, w * 0.12);
    p.drawLine(r.center().x(), r.top(), r.center().x(), r.center().y() - w * 0.34);
    p.drawLine(r.center().x(), r.bottom(), r.center().x(), r.center().y() + w * 0.34);
  } else if (id == 2) { /* Visuals — eye */
    QPainterPath eye;
    eye.moveTo(r.left(), r.center().y());
    eye.quadTo(r.center().x(), r.top() - h * 0.08, r.right(), r.center().y());
    eye.quadTo(r.center().x(), r.bottom() + h * 0.08, r.left(), r.center().y());
    p.drawPath(eye);
    p.drawEllipse(r.center(), w * 0.15, w * 0.15);
  } else if (id == 4) { /* minimize */
    p.drawLine(r.left() + w * 0.2, r.bottom() - h * 0.18, r.right() - w * 0.2,
               r.bottom() - h * 0.18);
  } else if (id == 5) { /* hide / collapse */
    p.drawLine(r.center().x() - w * 0.28, r.top() + h * 0.34, r.center().x(),
               r.top() + h * 0.62);
    p.drawLine(r.center().x(), r.top() + h * 0.62, r.center().x() + w * 0.28,
               r.top() + h * 0.34);
  } else if (id == 6) { /* close */
    p.drawLine(r.left(), r.top(), r.right(), r.bottom());
    p.drawLine(r.right(), r.top(), r.left(), r.bottom());
  } else { /* Theme — mixer sliders */
    const double xs[3] = {0.25, 0.55, 0.38};
    for (int i = 0; i < 3; ++i) {
      const double y = r.top() + h * (0.2 + 0.3 * i);
      p.drawLine(r.left() + w * 0.08, y, r.right() - w * 0.08, y);
      p.drawEllipse(QRectF(r.left() + w * xs[i] - 2.2, y - 2.2, 4.4, 4.4));
    }
  }

  p.restore();
}

/* Animated iOS-style switch. */
class Toggle : public QAbstractButton, public Paintable {
public:
  explicit Toggle(bool checked = false, QWidget *parent = nullptr) : QAbstractButton(parent) {
    setCheckable(true);
    setChecked(checked);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(46, 25);

    anim_ = new QVariantAnimation(this);
    anim_->setDuration(160);
    pos_ = checked ? 1.0 : 0.0;
    anim_->setStartValue(pos_);
    anim_->setEndValue(pos_);
    connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
      pos_ = v.toDouble();
      update();
    });
    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
      anim_->stop();
      anim_->setStartValue(pos_);
      anim_->setEndValue(on ? 1.0 : 0.0);
      anim_->start();
    });
  }

  void refresh() override { update(); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor a = Style::accent();
    const QColor off(0x24, 0x2A, 0x34);
    const QColor track(
        static_cast<int>(off.red() + (a.red() - off.red()) * pos_),
        static_cast<int>(off.green() + (a.green() - off.green()) * pos_),
        static_cast<int>(off.blue() + (a.blue() - off.blue()) * pos_));

    if (pos_ > 0.01) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(a.red(), a.green(), a.blue(), static_cast<int>(70 * pos_)));
      p.drawEllipse(QRectF(-4, -4, width() + 8, height() + 8));
    }

    QPainterPath path;
    path.addRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), height() / 2.0,
                        height() / 2.0);
    p.fillPath(path, track);

    const double knob = 19.0;
    const double x = 3.0 + pos_ * (width() - knob - 6.0);
    const QColor kc(
        static_cast<int>(0x7C + (0xFF - 0x7C) * pos_),
        static_cast<int>(0x87 + (0xFF - 0x87) * pos_),
        static_cast<int>(0x9B + (0xFF - 0x9B) * pos_));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60));
    p.drawEllipse(QRectF(x, 4.0, knob, knob));
    p.setBrush(kc);
    p.drawEllipse(QRectF(x, 3.0, knob, knob));
  }

private:
  QVariantAnimation *anim_ = nullptr;
  double pos_ = 0.0;
};

/* Round color chip that opens the picker. */
class ColorSwatch : public QPushButton, public Paintable {
public:
  ColorSwatch(int *rgb, QWidget *parent = nullptr) : QPushButton(parent), rgb_(rgb) {
    setFixedSize(26, 26);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setToolTip("Click to pick a color");
  }

  void setOnChange(std::function<void()> cb) { on_change_ = std::move(cb); }

  void refresh() override { update(); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(rgb_[0], rgb_[1], rgb_[2]));
    p.setPen(QPen(QColor(255, 255, 255, 46), 1));
    p.drawEllipse(QRectF(1, 1, width() - 2, height() - 2));
  }

  void mouseReleaseEvent(QMouseEvent *e) override {
    if (!rect().contains(e->position().toPoint())) return;

    QColorDialog dialog(QColor(rgb_[0], rgb_[1], rgb_[2]), this);
    dialog.setWindowFlags(Qt::Dialog);
    dialog.setOption(QColorDialog::DontUseNativeDialog, true);
    dialog.setWindowTitle("Fler — pick a color");
    dialog.setStyleSheet(Style::qss());

    if (dialog.exec() == QDialog::Accepted) {
      const QColor c = dialog.currentColor();
      rgb_[0] = c.red();
      rgb_[1] = c.green();
      rgb_[2] = c.blue();
      update();
      if (on_change_) on_change_();
    }
  }

private:
  int *rgb_ = nullptr;
  std::function<void()> on_change_;
};

/* One-shot accent preset. */
class ColorChip : public QPushButton, public Paintable {
public:
  ColorChip(const QColor &c, QWidget *parent = nullptr) : QPushButton(parent), color_(c) {
    setFixedSize(28, 28);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
  }

  void setOnClick(std::function<void()> cb) { on_click_ = std::move(cb); }

  void refresh() override { update(); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(color_);
    p.setPen(QPen(QColor(255, 255, 255, 50), 1));
    p.drawEllipse(QRectF(2, 2, width() - 4, height() - 4));
  }

  void mouseReleaseEvent(QMouseEvent *e) override {
    if (!rect().contains(e->position().toPoint())) return;
    if (on_click_) on_click_();
  }

private:
  QColor color_;
  std::function<void()> on_click_;
};

/* Single key capture, matching config.aim.key (a raw X11 keysym char). */
class KeyBind : public QPushButton {
public:
  KeyBind(char *key, QWidget *parent = nullptr) : QPushButton(parent), key_(key) {
    setObjectName("key");
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setMinimumWidth(92);
    refresh();
  }

  void setOnChange(std::function<void()> cb) { on_change_ = std::move(cb); }

  static QString label(char k) {
    if (k >= 'a' && k <= 'z') return QString(QChar(k)).toUpper();
    if (k >= 'A' && k <= 'Z') return QString(QChar(k));
    if (k == ' ') return "SPACE";
    if (k == '\t') return "TAB";
    if (k == '\r' || k == '\n') return "ENTER";
    if (k == '\b') return "BKSP";
    return QString("KEY %1").arg(QChar(k).unicode());
  }

  void refresh() { setText(capturing_ ? "press a key" : label(*key_)); }

protected:
  void mousePressEvent(QMouseEvent *e) override {
    QPushButton::mousePressEvent(e);
    capturing_ = true;
    refresh();
  }

  void keyPressEvent(QKeyEvent *e) override {
    if (!capturing_) {
      QPushButton::keyPressEvent(e);
      return;
    }

    const int k = e->key();
    if (k == Qt::Key_Escape) {
      capturing_ = false;
      refresh();
      e->accept();
      return;
    }
    if (k == Qt::Key_Control || k == Qt::Key_Shift || k == Qt::Key_Alt || k == Qt::Key_Meta) {
      e->accept();
      return;
    }

    char c = 0;
    if (k >= 0x20 && k <= 0x7E) {
      c = (char)k;
      if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    } else if (k == Qt::Key_Space) {
      c = ' ';
    } else if (k == Qt::Key_Tab) {
      c = '\t';
    } else if (k == Qt::Key_Return || k == Qt::Key_Enter) {
      c = '\r';
    }

    if (c) {
      *key_ = c;
      capturing_ = false;
      refresh();
      if (on_change_) on_change_();
    }
    e->accept();
  }

  void focusOutEvent(QFocusEvent *e) override {
    capturing_ = false;
    refresh();
    QPushButton::focusOutEvent(e);
  }

private:
  char *key_ = nullptr;
  bool capturing_ = false;
  std::function<void()> on_change_;
};

/* Sidebar entry: glyph + label, painted by hand so the accent bar lines up. */
class NavItem : public QPushButton, public Paintable {
public:
  NavItem(int glyph, const QString &text, QWidget *parent = nullptr)
      : QPushButton(text, parent), glyph_(glyph) {
    setObjectName("nav");
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setMinimumHeight(42);
    setMouseTracking(true);
  }

  void refresh() override { update(); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool active = isChecked();
    const QColor a = Style::accent();
    const QColor fg = active ? a : QColor(0x7C, 0x87, 0x9B);

    if (active || hovered_) {
      QPainterPath path;
      path.addRoundedRect(QRectF(0, 0, width(), height()), 10, 10);
      p.fillPath(path, active ? QColor(a.red(), a.green(), a.blue(), 38)
                              : QColor(0xFF, 0xFF, 0xFF, 6));
    }

    if (active) {
      p.setPen(Qt::NoPen);
      p.setBrush(a);
      p.drawRoundedRect(QRectF(0, height() / 2.0 - 10, 3, 20), 1.5, 1.5);
    }

    draw_glyph(p, QRectF(15, (height() - 18) / 2.0, 18, 18), glyph_, fg);

    QFont f = font();
    f.setWeight(active ? QFont::DemiBold : QFont::Medium);
    p.setFont(f);
    p.setPen(fg);
    p.drawText(QRectF(45, 0, width() - 60, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
  }

  void enterEvent(QEnterEvent *e) override {
    hovered_ = true;
    update();
    QPushButton::enterEvent(e);
  }

  void leaveEvent(QEvent *e) override {
    hovered_ = false;
    update();
    QPushButton::leaveEvent(e);
  }

private:
  int glyph_ = 0;
  bool hovered_ = false;
};

/* Header button painted by hand so the glyph matches the rest of the menu. */
class IconButton : public QPushButton, public Paintable {
public:
  IconButton(int glyph, const QString &tip, QWidget *parent = nullptr)
      : QPushButton(parent), glyph_(glyph) {
    setToolTip(tip);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(30, 28);
    setMouseTracking(true);
  }

  void setDanger(bool danger) { danger_ = danger; }

  void refresh() override { update(); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool danger_hover = danger_ && hovered_;
    if (hovered_) {
      QPainterPath path;
      path.addRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), 8, 8);
      p.fillPath(path, danger_hover ? QColor(0xE0, 0x31, 0x46) : QColor(0x1B, 0x22, 0x30));
    }

    const QColor fg = danger_hover ? QColor(0xFF, 0xFF, 0xFF)
                                   : (hovered_ ? QColor(0xE9, 0xEE, 0xF6)
                                               : QColor(0x7C, 0x87, 0x9B));
    draw_glyph(p, QRectF(7, 6, width() - 14, height() - 12), glyph_, fg);
  }

  void enterEvent(QEnterEvent *e) override {
    hovered_ = true;
    update();
    QPushButton::enterEvent(e);
  }

  void leaveEvent(QEvent *e) override {
    hovered_ = false;
    update();
    QPushButton::leaveEvent(e);
  }

private:
  int glyph_ = 0;
  bool danger_ = false;
  bool hovered_ = false;
};

/* Captures an X11 keysym — the menu hotkey is not a plain character. */
class KeySymBind : public QPushButton {
public:
  KeySymBind(int *keysym, QWidget *parent = nullptr) : QPushButton(parent), keysym_(keysym) {
    setObjectName("key");
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setMinimumWidth(92);
    refresh();
  }

  void setOnChange(std::function<void()> cb) { on_change_ = std::move(cb); }

  static int from_qt(int qt_key) {
    switch (qt_key) {
      case Qt::Key_End: return 0xFF57;
      case Qt::Key_Home: return 0xFF50;
      case Qt::Key_Insert: return 0xFF63;
      case Qt::Key_Delete: return 0xFFFF;
      case Qt::Key_PageUp: return 0xFF55;
      case Qt::Key_PageDown: return 0xFF56;
      case Qt::Key_Pause: return 0xFF13;
      case Qt::Key_Print: return 0xFF61;
      case Qt::Key_ScrollLock: return 0xFF14;
      case Qt::Key_Space: return 0x20;
      case Qt::Key_Tab: return 0xFF09;
      case Qt::Key_Backspace: return 0xFF08;
      case Qt::Key_Return:
      case Qt::Key_Enter: return 0xFF0D;
      case Qt::Key_Escape: return 0xFF1B;
      default: break;
    }
    if (qt_key >= 0x20 && qt_key <= 0x7E) return qt_key;
    return 0;
  }

  static QString name(int keysym) {
    switch (keysym) {
      case 0xFF57: return "END";
      case 0xFF50: return "HOME";
      case 0xFF63: return "INS";
      case 0xFFFF: return "DEL";
      case 0xFF55: return "PGUP";
      case 0xFF56: return "PGDN";
      case 0xFF13: return "PAUSE";
      case 0xFF61: return "PRINT";
      case 0xFF14: return "SCRLK";
      case 0x20: return "SPACE";
      case 0xFF09: return "TAB";
      case 0xFF08: return "BKSP";
      case 0xFF0D: return "ENTER";
      case 0xFF1B: return "ESC";
      default: break;
    }
    if (keysym >= 'a' && keysym <= 'z') return QString(QChar(keysym - 32));
    if (keysym >= 'A' && keysym <= 'Z') return QString(QChar(keysym));
    if (keysym >= 0x20 && keysym <= 0x7E) return QString(QChar(keysym));
    return QString("KEY %1").arg(keysym, 0, 16).toUpper();
  }

  void refresh() { setText(capturing_ ? "press a key" : name(*keysym_)); }

protected:
  void mousePressEvent(QMouseEvent *e) override {
    QPushButton::mousePressEvent(e);
    capturing_ = true;
    refresh();
  }

  void keyPressEvent(QKeyEvent *e) override {
    if (!capturing_) {
      QPushButton::keyPressEvent(e);
      return;
    }

    const int qt_key = e->key();
    if (qt_key == Qt::Key_Escape) {
      capturing_ = false;
      refresh();
      e->accept();
      return;
    }
    if (qt_key == Qt::Key_Control || qt_key == Qt::Key_Shift || qt_key == Qt::Key_Alt ||
        qt_key == Qt::Key_Meta) {
      e->accept();
      return;
    }

    const int keysym = from_qt(qt_key);
    if (keysym) {
      *keysym_ = keysym;
      capturing_ = false;
      refresh();
      if (on_change_) on_change_();
    }
    e->accept();
  }

  void focusOutEvent(QFocusEvent *e) override {
    capturing_ = false;
    refresh();
    QPushButton::focusOutEvent(e);
  }

private:
  int *keysym_ = nullptr;
  bool capturing_ = false;
  std::function<void()> on_change_;
};

/* Brand mark: gradient badge with a cut-out F. */
class Logo : public QWidget, public Paintable {
public:
  explicit Logo(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedSize(36, 36);
  }

  void refresh() override { update(); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor a = Style::accent();
    QLinearGradient g(0, 0, width(), height());
    g.setColorAt(0.0, a.lighter(118));
    g.setColorAt(1.0, a.darker(118));

    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width(), height()), 11, 11);
    p.fillPath(path, g);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xFF, 0xFF, 0xFF, 235));
    p.drawRoundedRect(QRectF(11, 9, 4, 18), 1.5, 1.5);
    p.drawRoundedRect(QRectF(11, 9, 13, 4), 1.5, 1.5);
    p.drawRoundedRect(QRectF(11, 16, 10, 4), 1.5, 1.5);
  }
};

/* The same mark as a pixmap — used for the tray icon. */
inline QPixmap fler_icon(int size) {
  QPixmap pm(size, size);
  pm.fill(Qt::transparent);

  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QColor a = Style::accent();
  QLinearGradient g(0, 0, size, size);
  g.setColorAt(0.0, a.lighter(118));
  g.setColorAt(1.0, a.darker(118));

  QPainterPath path;
  path.addRoundedRect(QRectF(0, 0, size, size), size * 0.3, size * 0.3);
  p.fillPath(path, g);

  const double u = size / 36.0;
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0xFF, 0xFF, 0xFF, 235));
  p.drawRoundedRect(QRectF(11 * u, 9 * u, 4 * u, 18 * u), 1.5 * u, 1.5 * u);
  p.drawRoundedRect(QRectF(11 * u, 9 * u, 13 * u, 4 * u), 1.5 * u, 1.5 * u);
  p.drawRoundedRect(QRectF(11 * u, 16 * u, 10 * u, 4 * u), 1.5 * u, 1.5 * u);
  return pm;
}

#endif
