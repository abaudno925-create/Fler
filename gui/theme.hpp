#ifndef FLER_THEME_HPP
#define FLER_THEME_HPP

#include <QColor>
#include <QString>
#include <QWidget>

#include <algorithm>

#include "config.hpp"

/* Visual language of the Fler menu: dark surfaces, one accent color, soft edges. */
namespace Style {

  inline QColor accent() {
    return QColor(config.theme.accent[0], config.theme.accent[1], config.theme.accent[2]);
  }

  inline QColor accent_hot() {
    const QColor a = accent();
    return QColor::fromHsvF(a.hueF() < 0 ? 0.0 : a.hueF(), a.saturationF(),
                            std::min(1.0, a.valueF() + 0.14));
  }

  inline QString qss() {
    const QColor a = accent();

    QString sheet = QString::fromLatin1(R"CSS(
QWidget {
  font-family: "Inter", "SF Pro Display", "Segoe UI", "Noto Sans", "DejaVu Sans", sans-serif;
  font-size: 13px;
  color: #C9D1DE;
}
QWidget#root {
  background: #0A0C11;
}
QWidget#sidebar {
  background: #0E1117;
  border-right: 1px solid #1A1F28;
}
QWidget#content {
  background: #0A0C11;
}
QWidget#header {
  background: #0A0C11;
  border-bottom: 1px solid #161B23;
}
QLabel {
  background: transparent;
}
QLabel#brand {
  color: #FFFFFF;
  font-size: 21px;
  font-weight: 800;
  letter-spacing: 6px;
}
QLabel#brandSub {
  color: #59637A;
  font-size: 9px;
  font-weight: 600;
  letter-spacing: 2px;
}
QLabel#pageTitle {
  color: #F2F5FA;
  font-size: 19px;
  font-weight: 700;
}
QLabel#pageSub {
  color: #5F6A80;
  font-size: 12px;
}
QLabel#cardTitle {
  color: #6F7B91;
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 2px;
}
QLabel#rowLabel {
  color: #C2CAD8;
  font-size: 13px;
}
QLabel#rowValue {
  color: #5F6A80;
  font-size: 12px;
}
QLabel#footer {
  color: #414B5E;
  font-size: 11px;
}
QLabel#badge {
  color: @ACCENT@;
  background: @ACCENT_SOFT@;
  border: 1px solid @ACCENT_GLOW@;
  border-radius: 6px;
  padding: 2px 8px;
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 1px;
}
QFrame#card {
  background: #12161D;
  border: 1px solid #1B212B;
  border-radius: 14px;
}
QFrame#line {
  background: #191F28;
  border: none;
}
QPushButton#iconBtn {
  background: #141922;
  border: 1px solid #1F2632;
  border-radius: 8px;
  color: #7C879B;
  font-size: 13px;
  font-weight: 700;
  min-width: 30px;
  max-width: 30px;
  min-height: 28px;
  max-height: 28px;
}
QPushButton#iconBtn:hover {
  background: #1B2230;
  color: #E9EEF6;
}
QPushButton#iconBtn[cls="close"]:hover {
  background: #E03146;
  border-color: #E03146;
  color: #FFFFFF;
}
QPushButton#key {
  background: #151A22;
  border: 1px solid #222A36;
  border-radius: 8px;
  color: #C2CAD8;
  padding: 6px 10px;
  font-size: 12px;
  font-weight: 700;
}
QPushButton#key:hover {
  border-color: @ACCENT@;
  color: #FFFFFF;
}
QPushButton#key:focus {
  border-color: @ACCENT@;
}
QPushButton#primary {
  background: @ACCENT@;
  border: none;
  border-radius: 9px;
  color: #FFFFFF;
  padding: 9px 16px;
  font-weight: 700;
}
QPushButton#primary:hover {
  background: @ACCENT_H@;
}
QPushButton#ghost {
  background: #151A22;
  border: 1px solid #1F2632;
  border-radius: 9px;
  color: #93A0B5;
  padding: 9px 14px;
  font-weight: 600;
}
QPushButton#ghost:hover {
  background: #1B2230;
  color: #E9EEF6;
}
QSlider::groove:horizontal {
  height: 4px;
  background: #222833;
  border-radius: 2px;
}
QSlider::sub-page:horizontal {
  background: @ACCENT@;
  border-radius: 2px;
}
QSlider::handle:horizontal {
  background: #FFFFFF;
  border: 3px solid @ACCENT@;
  width: 14px;
  height: 14px;
  margin: -7px 0;
  border-radius: 8px;
}
QSlider::handle:horizontal:hover {
  border-color: @ACCENT_H@;
}
QSlider::groove:horizontal:disabled {
  background: #1A1F28;
}
QScrollArea {
  background: transparent;
  border: none;
}
QScrollArea > QWidget > QWidget {
  background: transparent;
}
QScrollBar:vertical {
  background: transparent;
  width: 10px;
  margin: 0;
}
QScrollBar::handle:vertical {
  background: #232A36;
  border-radius: 5px;
  min-height: 28px;
}
QScrollBar::handle:vertical:hover {
  background: #2E3746;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
  height: 0;
  width: 0;
}
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
  background: transparent;
}
QToolTip {
  background: #171D26;
  color: #C9D1DE;
  border: 1px solid #262E3B;
  padding: 5px 7px;
  border-radius: 6px;
}
QColorDialog QWidget {
  background: #12161D;
  color: #C9D1DE;
}
QColorDialog QPushButton {
  background: #1A202A;
  border: 1px solid #262E3B;
  border-radius: 6px;
  padding: 5px 14px;
}
QColorDialog QPushButton:hover {
  background: #222A36;
}
QColorDialog QLineEdit,
QColorDialog QSpinBox {
  background: #0E1218;
  border: 1px solid #262E3B;
  border-radius: 5px;
  padding: 3px;
}
QColorDialog QTabBar::tab {
  background: #161B24;
  color: #7C879B;
  padding: 6px 14px;
  border-top-left-radius: 7px;
  border-top-right-radius: 7px;
}
QColorDialog QTabBar::tab:selected {
  background: #12161D;
  color: #FFFFFF;
}
)CSS");

    const QString rgb = QString("%1,%2,%3").arg(a.red()).arg(a.green()).arg(a.blue());
    sheet.replace("@ACCENT@", a.name(QColor::HexRgb));
    sheet.replace("@ACCENT_H@", accent_hot().name(QColor::HexRgb));
    sheet.replace("@ACCENT_SOFT@", QString("rgba(%1,0.14)").arg(rgb));
    sheet.replace("@ACCENT_GLOW@", QString("rgba(%1,0.40)").arg(rgb));
    return sheet;
  }

  inline void apply(QWidget *root) {
    root->setStyleSheet(qss());
  }
}

#endif
