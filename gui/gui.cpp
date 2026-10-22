#include "gui.hpp"

#include "config.hpp"
#include "../engine.hpp"
#include "menu_key.hpp"
#include "esp_overlay.hpp"
#include "preview3d.hpp"
#include "settings.hpp"
#include "skins.hpp"
#include "theme.hpp"
#include "widgets.hpp"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSlider>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <functional>

namespace {

  const char *kVersion = "1.0.0";

  /* ---------------- layout helpers ---------------- */

  QWidget *makeRow(const QString &label, const QVector<QWidget *> &controls) {
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 7, 0, 7);
    h->setSpacing(12);

    auto *l = new QLabel(label);
    l->setObjectName("rowLabel");
    h->addWidget(l);
    h->addStretch(1);

    for (QWidget *c : controls) h->addWidget(c, 0, Qt::AlignRight | Qt::AlignVCenter);

    return w;
  }

  QWidget *makeRow(const QString &label, QWidget *control) {
    return makeRow(label, QVector<QWidget *>{control});
  }

  QFrame *makeSeparator() {
    auto *f = new QFrame;
    f->setObjectName("line");
    f->setFixedHeight(1);
    return f;
  }

  QWidget *makePage() {
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *inner = new QWidget;
    inner->setObjectName("page");
    auto *lay = new QVBoxLayout(inner);
    lay->setContentsMargins(26, 18, 26, 26);
    lay->setSpacing(16);

    scroll->setWidget(inner);
    return scroll;
  }

  QVBoxLayout *pageLayout(QWidget *page) {
    auto *scroll = qobject_cast<QScrollArea *>(page);
    return qobject_cast<QVBoxLayout *>(scroll->widget()->layout());
  }

  /* Rounded panel with a small caps heading. */
  class Card : public QFrame {
  public:
    explicit Card(const QString &title, QWidget *parent = nullptr) : QFrame(parent) {
      setObjectName("card");
      setFrameShape(QFrame::NoFrame);

      body_ = new QVBoxLayout(this);
      body_->setContentsMargins(18, 15, 18, 16);
      body_->setSpacing(0);

      if (!title.isEmpty()) {
        auto *t = new QLabel(title);
        t->setObjectName("cardTitle");
        body_->addWidget(t);
        body_->addSpacing(11);
      }
    }

    QVBoxLayout *body() const { return body_; }

    void addRow(QWidget *w) { body_->addWidget(w); }

    void addSeparator() { body_->addWidget(makeSeparator()); }

    void addText(const QString &text) {
      auto *l = new QLabel(text);
      l->setObjectName("rowValue");
      l->setWordWrap(true);
      body_->addWidget(l);
    }

  private:
    QVBoxLayout *body_ = nullptr;
  };

  /* Label + slider + live value. */
  class SliderRow : public QWidget {
  public:
    SliderRow(const QString &label, int minimum, int maximum, int value,
              std::function<void(int)> on_change, QWidget *parent = nullptr)
        : QWidget(parent) {
      auto *lay = new QHBoxLayout(this);
      lay->setContentsMargins(0, 6, 0, 6);
      lay->setSpacing(14);

      auto *l = new QLabel(label);
      l->setObjectName("rowLabel");
      lay->addWidget(l);
      lay->addStretch(1);

      slider_ = new QSlider(Qt::Horizontal);
      slider_->setRange(minimum, maximum);
      slider_->setValue(value);
      slider_->setFixedWidth(210);
      slider_->setTickPosition(QSlider::NoTicks);
      lay->addWidget(slider_);

      value_ = new QLabel(QString::number(value));
      value_->setObjectName("rowValue");
      value_->setMinimumWidth(34);
      value_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      lay->addWidget(value_);

      connect(slider_, &QSlider::valueChanged, this, [this, on_change](int v) {
        value_->setText(QString::number(v));
        if (on_change) on_change(v);
      });
    }

  private:
    QSlider *slider_ = nullptr;
    QLabel *value_ = nullptr;
  };

  /* Draggable title bar for the frameless window. */
  class Header : public QWidget {
  public:
    explicit Header(QWidget *parent = nullptr) : QWidget(parent) {
      setObjectName("header");
      setFixedHeight(82);

      auto *lay = new QHBoxLayout(this);
      lay->setContentsMargins(28, 0, 18, 0);
      lay->setSpacing(10);

      auto *texts = new QVBoxLayout;
      texts->setContentsMargins(0, 0, 0, 0);
      texts->setSpacing(3);

      title_ = new QLabel;
      title_->setObjectName("pageTitle");
      title_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
      sub_ = new QLabel;
      sub_->setObjectName("pageSub");
      sub_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
      texts->addWidget(title_);
      texts->addWidget(sub_);
      lay->addLayout(texts);
      lay->addStretch(1);

      hotkey_ = new QLabel;
      hotkey_->setObjectName("badge");
      hotkey_->setToolTip("Press this key to show or hide Fler");
      hotkey_->setAlignment(Qt::AlignCenter);
      lay->addWidget(hotkey_, 0, Qt::AlignVCenter);

      auto *minimize = new IconButton(4, "Minimize");
      auto *hide = new IconButton(5, "Hide menu");
      auto *close = new IconButton(6, "Close");
      close->setDanger(true);
      lay->addWidget(minimize);
      lay->addWidget(hide);
      lay->addWidget(close);

      connect(minimize, &QPushButton::clicked, this, [this] { window()->showMinimized(); });
      connect(hide, &QPushButton::clicked, this, [this] { window()->hide(); });
      connect(close, &QPushButton::clicked, this, [this] { window()->close(); });
    }

    void setPage(const QString &title, const QString &subtitle) {
      title_->setText(title);
      sub_->setText(subtitle);
    }

    void setHotkey(const QString &text) { hotkey_->setText(text); }

  protected:
    void mousePressEvent(QMouseEvent *e) override {
      if (e->button() != Qt::LeftButton) return;

      /* Compositor driven move — the only way to drag on Wayland. */
      QWindow *handle = window()->windowHandle();
      if (handle && handle->startSystemMove()) return;

      dragging_ = true;
      offset_ = e->globalPosition().toPoint() - window()->frameGeometry().topLeft();
    }

    void mouseMoveEvent(QMouseEvent *e) override {
      if (dragging_) window()->move(e->globalPosition().toPoint() - offset_);
    }

    void mouseReleaseEvent(QMouseEvent *e) override { dragging_ = false; }

  private:
    QLabel *title_ = nullptr;
    QLabel *sub_ = nullptr;
    QLabel *hotkey_ = nullptr;
    bool dragging_ = false;
    QPoint offset_;
  };

  /* Frameless window with rounded corners. */
  class FlerWindow : public QWidget {
  public:
    FlerWindow() {
      setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
      setAttribute(Qt::WA_TranslucentBackground, true);
    }

  protected:
    void resizeEvent(QResizeEvent *e) override {
      applyMask();
      QWidget::resizeEvent(e);
    }

    void showEvent(QShowEvent *e) override {
      applyMask();
      QWidget::showEvent(e);
    }

  private:
    void applyMask() {
      QPainterPath path;
      path.addRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), 14, 14);
      setMask(QRegion(path.toFillPolygon().toPolygon()));
    }
  };
}

void gui(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setStyle("Fusion");
  app.setApplicationName("Fler");

  Settings::load();

  FlerWindow window;
  window.setWindowTitle("Fler");
  window.setMinimumSize(900, 560);
  window.resize(1060, 690);

  auto *windowLayout = new QVBoxLayout(&window);
  windowLayout->setContentsMargins(0, 0, 0, 0);
  windowLayout->setSpacing(0);

  auto *root = new QWidget;
  root->setObjectName("root");
  windowLayout->addWidget(root);

  auto *rootLayout = new QVBoxLayout(root);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  auto *columns = new QHBoxLayout;
  columns->setContentsMargins(0, 0, 0, 0);
  columns->setSpacing(0);
  rootLayout->addLayout(columns);

  /* ---- sidebar ---- */
  auto *sidebar = new QWidget;
  sidebar->setObjectName("sidebar");
  sidebar->setFixedWidth(222);
  columns->addWidget(sidebar);

  auto *sideLayout = new QVBoxLayout(sidebar);
  sideLayout->setContentsMargins(16, 20, 16, 16);
  sideLayout->setSpacing(0);

  auto *brandRow = new QHBoxLayout;
  brandRow->setSpacing(11);
  brandRow->addWidget(new Logo);

  auto *brandText = new QVBoxLayout;
  brandText->setContentsMargins(0, 0, 0, 0);
  brandText->setSpacing(2);
  auto *brandLabel = new QLabel("FLER");
  brandLabel->setObjectName("brand");
  auto *brandSub = new QLabel("CS2 · LINUX");
  brandSub->setObjectName("brandSub");
  brandText->addWidget(brandLabel);
  brandText->addWidget(brandSub);
  brandRow->addLayout(brandText);
  brandRow->addStretch(1);
  sideLayout->addLayout(brandRow);
  sideLayout->addSpacing(28);

  /* ---- content ---- */
  auto *content = new QWidget;
  content->setObjectName("content");
  columns->addWidget(content, 1);

  auto *contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  auto *header = new Header;
  contentLayout->addWidget(header);

  auto *stack = new QStackedWidget;
  contentLayout->addWidget(stack, 1);

  /* ---- behaviour ---- */
  auto *saveTimer = new QTimer(root);
  saveTimer->setSingleShot(true);
  saveTimer->setInterval(400);
  QObject::connect(saveTimer, &QTimer::timeout, [] { Settings::save(); });

  auto scheduleSave = [saveTimer] { saveTimer->start(); };

  auto applyAccent = [root] {
    Style::apply(root);
    for (auto *widget : Paintable::paintables()) widget->refresh();
  };

  auto bindToggle = [root, scheduleSave](Toggle *toggle, bool *field) {
    toggle->setChecked(*field);
    QObject::connect(toggle, &QAbstractButton::toggled, root, [field, scheduleSave](bool on) {
      *field = on;
      EspPreview::refresh_all();
      scheduleSave();
    });
  };

  auto makeToggle = [bindToggle](bool *field) {
    auto *toggle = new Toggle(*field);
    bindToggle(toggle, field);
    return toggle;
  };

  auto makeSwatch = [scheduleSave](int *rgb) {
    auto *swatch = new ColorSwatch(rgb);
    swatch->setOnChange([scheduleSave] {
      EspPreview::refresh_all();
      scheduleSave();
    });
    return swatch;
  };

  /* ---- pages ---- */
  auto buildPages = std::make_shared<std::function<void()>>();

  auto buildEsp = [makeToggle, makeSwatch]() {
    auto *page = makePage();
    auto *lay = pageLayout(page);

    auto *previewCard = new Card("LIVE PREVIEW");
    previewCard->addRow(new EspPreview);
    previewCard->addText("Rendered with the exact options below — a counter-terrorist and a "
                         "terrorist.");
    lay->addWidget(previewCard);

    auto *players = new Card("PLAYERS");
    players->addRow(makeRow("Master", makeToggle(&config.esp.master)));
    players->addSeparator();
    players->addRow(makeRow("Ignore team", makeToggle(&config.esp.ignore_team)));
    players->addSeparator();
    players->addRow(makeRow("Spotted only", makeToggle(&config.esp.spotted)));
    lay->addWidget(players);

    auto *renderers = new Card("RENDERERS");
    renderers->addRow(makeRow("Box", {makeToggle(&config.esp.box),
                                       makeSwatch(config.esp.box_color)}));
    renderers->addSeparator();
    renderers->addRow(makeRow("Health bar", makeToggle(&config.esp.health_bar)));
    renderers->addSeparator();
    renderers->addRow(makeRow("Health text", makeToggle(&config.esp.health_text)));
    renderers->addSeparator();
    renderers->addRow(makeRow("Name", makeToggle(&config.esp.name)));
    renderers->addSeparator();
    renderers->addRow(makeRow("Weapon name", makeToggle(&config.esp.weapon_name)));
    renderers->addSeparator();
    renderers->addRow(makeRow("Skeleton", {makeToggle(&config.esp.skeleton),
                                           makeSwatch(config.esp.skeleton_color)}));
    renderers->addSeparator();
    renderers->addRow(makeRow("Head dot", {makeToggle(&config.esp.head_dot),
                                           makeSwatch(config.esp.head_dot_color)}));
    renderers->addSeparator();
    renderers->addRow(makeRow("Filled head dot", makeToggle(&config.esp.head_dot_fill)));
    renderers->addSeparator();
    renderers->addRow(makeRow("Cat ears", makeToggle(&config.esp.cat_ears)));
    renderers->addSeparator();
    renderers->addRow(makeRow("Snap lines", {makeToggle(&config.esp.snap_lines),
                                             makeSwatch(config.esp.snap_lines_color)}));
    lay->addWidget(renderers);

    lay->addStretch(1);
    return page;
  };

  auto buildAim = [makeToggle, makeSwatch, scheduleSave]() {
    auto *page = makePage();
    auto *lay = pageLayout(page);

    auto *targeting = new Card("TARGETING");
    targeting->addRow(makeRow("Master", makeToggle(&config.aim.master)));
    targeting->addSeparator();
    targeting->addRow(makeRow("Ignore team", makeToggle(&config.aim.ignore_team)));
    targeting->addSeparator();
    targeting->addRow(makeRow("Spotted only", makeToggle(&config.aim.spotted)));
    lay->addWidget(targeting);

    auto *aim = new Card("AIM");
    aim->addRow(new SliderRow("Field of view", 1, 180, (int)config.aim.fov,
                              [scheduleSave](int v) {
                                config.aim.fov = (float)v;
                                scheduleSave();
                              }));
    aim->addSeparator();
    aim->addRow(makeRow("Show FOV circle", makeToggle(&config.aim.show_fov)));
    aim->addSeparator();
    aim->addRow(makeRow("Snap lines", {makeToggle(&config.aim.snap_lines),
                                       makeSwatch(config.aim.snap_lines_color)}));
    aim->addSeparator();
    aim->addRow(makeRow("Recoil control", makeToggle(&config.aim.recoil)));
    aim->addSeparator();
    aim->addRow(makeRow("Auto shoot", makeToggle(&config.aim.auto_shoot)));
    lay->addWidget(aim);

    auto *hotkey = new Card("HOTKEY");
    auto *bind = new KeyBind(&config.aim.key);
    bind->setOnChange([scheduleSave] { scheduleSave(); });
    hotkey->addRow(makeRow("Aim key", bind));
    hotkey->addText("Hold this key to pull the aim onto the closest target inside the FOV.");
    lay->addWidget(hotkey);

    lay->addStretch(1);
    return page;
  };

  auto buildVisuals = [makeToggle, makeSwatch]() {
    auto *page = makePage();
    auto *lay = pageLayout(page);

    auto *crosshair = new Card("CROSSHAIR");
    crosshair->addRow(makeRow("Crosshair", {makeToggle(&config.visuals.crosshair),
                                             makeSwatch(config.visuals.crosshair_color)}));
    crosshair->addSeparator();
    crosshair->addRow(makeRow("Sniper only", makeToggle(&config.visuals.sniper_only)));
    crosshair->addSeparator();
    crosshair->addRow(makeRow("Visualize recoil", makeToggle(&config.visuals.visualize_recoil)));
    lay->addWidget(crosshair);

    auto *hud = new Card("HUD");
    hud->addRow(makeRow("FPS counter", makeToggle(&config.visuals.fps)));
    lay->addWidget(hud);

    lay->addStretch(1);
    return page;
  };

  auto buildTheme = [root, header, applyAccent, scheduleSave, buildPages]() {
    auto *page = makePage();
    auto *lay = pageLayout(page);

    auto *accentCard = new Card("ACCENT");
    auto *accentSwatch = new ColorSwatch(config.theme.accent);
    accentSwatch->setOnChange([applyAccent, scheduleSave] {
      applyAccent();
      scheduleSave();
    });
    accentCard->addRow(makeRow("Accent color", accentSwatch));

    auto *chips = new QWidget;
    auto *chipLayout = new QHBoxLayout(chips);
    chipLayout->setContentsMargins(0, 0, 0, 0);
    chipLayout->setSpacing(9);
    const QList<QColor> presets = {QColor(255, 45, 77), QColor(0, 229, 255),
                                   QColor(126, 87, 255),  QColor(0, 255, 122),
                                   QColor(255, 170, 0),   QColor(255, 255, 255)};
    for (const QColor &preset : presets) {
      auto *chip = new ColorChip(preset);
      chip->setOnClick([preset, applyAccent, scheduleSave] {
        config.theme.accent[0] = preset.red();
        config.theme.accent[1] = preset.green();
        config.theme.accent[2] = preset.blue();
        applyAccent();
        scheduleSave();
      });
      chipLayout->addWidget(chip);
    }
    chipLayout->addStretch(1);
    accentCard->addRow(makeRow("Presets", chips));
    lay->addWidget(accentCard);

    auto *menuCard = new Card("MENU");
    auto *keyBind = new KeySymBind(&config.menu.toggle_key);
    keyBind->setOnChange([header, scheduleSave] {
      MenuKey::start(config.menu.toggle_key);
      header->setHotkey(KeySymBind::name(config.menu.toggle_key));
      scheduleSave();
    });
    menuCard->addRow(makeRow("Toggle key", keyBind));

    auto *hideButton = new QPushButton("Hide menu");
    hideButton->setObjectName("ghost");
    hideButton->setCursor(Qt::PointingHandCursor);
    QObject::connect(hideButton, &QPushButton::clicked, root,
                     [root] { root->window()->hide(); });
    menuCard->addRow(makeRow("Visibility", hideButton));
    menuCard->addText("The toggle key shows and hides Fler while the game keeps the focus.");
    lay->addWidget(menuCard);

    auto *configCard = new Card("CONFIGURATION");
    auto *reset = new QPushButton("Reset to defaults");
    reset->setObjectName("ghost");
    reset->setCursor(Qt::PointingHandCursor);
    QObject::connect(reset, &QPushButton::clicked, root, [root, buildPages, applyAccent,
                                                           scheduleSave] {
      config = Config{};
      /* rebuild outside the click handler: it deletes the button that sent it */
      QTimer::singleShot(0, root, [buildPages, applyAccent, scheduleSave] {
        (*buildPages)();
        applyAccent();
        scheduleSave();
      });
    });
    configCard->addRow(makeRow("Defaults", reset));
    configCard->addText("Settings are stored in ~/.config/Fler/Fler.conf and restored on the "
                        "next launch.");
    lay->addWidget(configCard);

    auto *about = new Card("ABOUT");
    auto *badge = new QLabel(QString("v%1").arg(kVersion));
    badge->setObjectName("badge");
    about->addRow(makeRow("Version", badge));
    about->addSeparator();
    about->addText("Fler is an external X11 overlay for Counter-Strike 2 on Linux. Everything "
                   "is rendered outside the game process, so the game itself stays untouched.");
    lay->addWidget(about);

    lay->addStretch(1);
    return page;
  };

  struct NavDef {
    int glyph;
    const char *title;
    const char *subtitle;
  };

  const NavDef navDefs[5] = {
      {0, "ESP", "Who is drawn on screen"},
      {1, "AIMBOT", "Targeting and angles"},
      {2, "VISUALS", "Crosshair and HUD"},
      {7, "SKINS", "Weapon finishes from your game"},
      {3, "THEME", "Accent, hotkey and configuration"},
  };

  *buildPages = [&]() {
    while (stack->count() > 0) {
      QWidget *old = stack->widget(0);
      stack->removeWidget(old);
      delete old;
    }
    stack->addWidget(buildEsp());
    stack->addWidget(buildAim());
    stack->addWidget(buildVisuals());
    stack->addWidget(new SkinPage(scheduleSave));
    stack->addWidget(buildTheme());
  };

  (*buildPages)();

  /* ---- navigation ---- */
  auto *navGroup = new QButtonGroup;
  navGroup->setExclusive(true);

  for (int i = 0; i < 5; ++i) {
    auto *item = new NavItem(navDefs[i].glyph, navDefs[i].title, sidebar);
    item->setChecked(i == 0);
    sideLayout->addWidget(item);
    sideLayout->addSpacing(4);
    navGroup->addButton(item, i);

    const int index = i;
    QObject::connect(item, &QPushButton::clicked, root, [=] {
      stack->setCurrentIndex(index);
      header->setPage(navDefs[index].title, navDefs[index].subtitle);
    });
  }

  sideLayout->addStretch(1);

  auto *footerLine = makeSeparator();
  sideLayout->addWidget(footerLine);
  sideLayout->addSpacing(12);

  auto *version = new QLabel(QString("Fler v%1").arg(kVersion));
  version->setObjectName("footer");
  auto *platform = new QLabel("X11 overlay · Linux");
  platform->setObjectName("footer");
  sideLayout->addWidget(version);
  sideLayout->addWidget(platform);

  header->setPage(navDefs[0].title, navDefs[0].subtitle);

  /* ---- global hotkey, tray and show/hide ---- */
  MenuKey::start(config.menu.toggle_key);
  header->setHotkey(KeySymBind::name(config.menu.toggle_key));

  auto toggleMenu = [&window]() {
    if (window.isVisible()) {
      window.hide();
    } else {
      window.show();

  /* the cheat overlay: a translucent surface above the game */
  auto *overlay = new EspOverlay;
  overlay->setGeometry(0, 0, Engine::window_width, Engine::window_height);
  overlay->show();
  overlay->start();
      window.raise();
      window.activateWindow();
    }
  };

  auto *hotkeyTimer = new QTimer(root);
  hotkeyTimer->setInterval(40);
  QObject::connect(hotkeyTimer, &QTimer::timeout, [toggleMenu] {
    if (MenuKey::consume_toggle()) toggleMenu();
  });
  hotkeyTimer->start();

  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    auto *tray = new QSystemTrayIcon(&window);
    tray->setIcon(fler_icon(64));
    tray->setToolTip("Fler");

    auto *trayMenu = new QMenu(&window);
    QAction *toggleAction = trayMenu->addAction("Show / hide Fler");
    QObject::connect(toggleAction, &QAction::triggered, &window,
                     [toggleMenu] { toggleMenu(); });
    QAction *quitAction = trayMenu->addAction("Quit Fler");
    QObject::connect(quitAction, &QAction::triggered, &window,
                     [] { QApplication::quit(); });
    tray->setContextMenu(trayMenu);

    QObject::connect(tray, &QSystemTrayIcon::activated, &window,
                     [toggleMenu](QSystemTrayIcon::ActivationReason reason) {
                       if (reason == QSystemTrayIcon::Trigger ||
                           reason == QSystemTrayIcon::DoubleClick) {
                         toggleMenu();
                       }
                     });
    tray->show();
  }

  Style::apply(root);

  window.show();

  /* dev aid: FLER_SHOT=file.png FLER_PAGE=n renders the menu to a file */
  if (const char *shot = getenv("FLER_SHOT")) {
    if (const char *page = getenv("FLER_PAGE")) {
      const int index = atoi(page);
      if (index >= 0 && index < 5) {
        stack->setCurrentIndex(index);
        header->setPage(navDefs[index].title, navDefs[index].subtitle);
        for (int i = 0; i < 5; ++i) {
          auto *item = navGroup->button(i);
          if (item) item->setChecked(i == index);
        }
      }
    }
    QTimer::singleShot(1500, &window, [shot, &window] {
      window.grab().save(QString::fromLatin1(shot));
      QApplication::quit();
    });
  }

  QObject::connect(&app, &QApplication::aboutToQuit, [] { Settings::save(); });

  app.exec();

  Settings::save();
}
