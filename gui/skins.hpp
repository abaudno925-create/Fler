#ifndef FLER_SKINS_HPP
#define FLER_SKINS_HPP

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QString>
#include <QVBoxLayout>
#include <QVector>

#include <functional>

#include "config.hpp"
#include "theme.hpp"

struct Paint {
  int id = 0;
  QString name;
  QString display;
  QString rarity;
  int rarity_value = 0;
  float wear = 0.0f;
};

struct SkinItem {
  int def_index = 0;
  QString name;
  QString display;
  QString kind;
  QVector<int> paints;
};

/* The econ database, extracted from the local CS2 install by
   tools/fetch_skins.py into assets/skins.json. */
class SkinDatabase {
public:
  bool load(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;
    const QJsonObject root = doc.object();

    const QJsonObject paint_map = root["paints"].toObject();
    for (auto it = paint_map.begin(); it != paint_map.end(); ++it) {
      const QJsonObject node = it.value().toObject();
      Paint paint;
      paint.id = it.key().toInt();
      paint.name = node["name"].toString();
      paint.display = node["display"].toString();
      paint.rarity = node["rarity"].toString("default");
      paint.rarity_value = node["rarity_value"].toInt(0);
      paint.wear = (float)node["wear"].toDouble(0.0);
      paints_.push_back(paint);
    }

    const QJsonObject item_map = root["items"].toObject();
    for (auto it = item_map.begin(); it != item_map.end(); ++it) {
      const QJsonObject node = it.value().toObject();
      SkinItem item;
      item.def_index = it.key().toInt();
      item.name = node["name"].toString();
      item.display = node["display"].toString();
      item.kind = node["kind"].toString("weapon");
      for (const QJsonValue &value : node["paints"].toArray()) {
        item.paints.push_back(value.toInt());
      }
      if (item.kind == "weapon" || item.kind == "knife" || item.kind == "glove") {
        items_.push_back(item);
      }
    }

    return !items_.isEmpty();
  }

  const QVector<SkinItem> &items() const { return items_; }
  const QVector<Paint> &paints() const { return paints_; }

  const Paint *paint(int id) const {
    for (const Paint &paint : paints_) {
      if (paint.id == id) return &paint;
    }
    return nullptr;
  }

  const SkinItem *item(int def_index) const {
    for (const SkinItem &item : items_) {
      if (item.def_index == def_index) return &item;
    }
    return nullptr;
  }

  static QString find() {
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/assets/skins.json",
        QDir::currentPath() + "/assets/skins.json",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/skins.json",
    };
    for (const QString &path : candidates) {
      if (QFileInfo::exists(path)) return path;
    }
    return QString();
  }

  static QColor rarity_color(const QString &rarity) {
    if (rarity == "common") return QColor(0xB0, 0xC3, 0xD9);
    if (rarity == "uncommon") return QColor(0x5E, 0x98, 0xD9);
    if (rarity == "rare") return QColor(0x4A, 0x69, 0xA5);
    if (rarity == "mythical") return QColor(0x88, 0x47, 0xFF);
    if (rarity == "legendary") return QColor(0xD3, 0x2C, 0xE6);
    if (rarity == "ancient") return QColor(0xEB, 0x4B, 0x4B);
    if (rarity == "immortal") return QColor(0xE2, 0xB1, 0x00);
    return QColor(0x8A, 0x94, 0xA6);
  }

private:
  QVector<Paint> paints_;
  QVector<SkinItem> items_;
};

/* The skins page: pick a weapon, pick a finish, set wear and seed. */
class SkinPage : public QWidget {
public:
  explicit SkinPage(std::function<void()> on_change = nullptr, QWidget *parent = nullptr)
      : QWidget(parent), on_change_(std::move(on_change)) {
    setObjectName("skinPage");

    const QString path = SkinDatabase::find();
    loaded_ = !path.isEmpty() && database_.load(path);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 18, 26, 26);
    root->setSpacing(14);

    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    search_ = new QLineEdit;
    search_->setPlaceholderText("Search weapon or finish…");
    search_->setClearButtonEnabled(true);
    search_->setObjectName("search");
    header->addWidget(search_, 1);

    enabled_ = new QPushButton("Skin changer");
    enabled_->setObjectName("primary");
    enabled_->setCheckable(true);
    enabled_->setCursor(Qt::PointingHandCursor);
    header->addWidget(enabled_);
    root->addLayout(header);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(14);

    items_ = new QListWidget;
    items_->setObjectName("skinList");
    items_->setMinimumWidth(240);
    columns->addWidget(items_, 1);

    auto *right = new QVBoxLayout;
    right->setSpacing(12);

    paints_ = new QListWidget;
    paints_->setObjectName("skinList");
    right->addWidget(paints_, 1);

    auto *wearRow = new QHBoxLayout;
    wearRow->setSpacing(12);
    auto *wearLabel = new QLabel("Wear");
    wearLabel->setObjectName("rowLabel");
    wearRow->addWidget(wearLabel);
    wear_ = new QSlider(Qt::Horizontal);
    wear_->setRange(0, 100);
    wear_->setValue((int)(config.skin.wear * 100));
    wear_->setFixedWidth(200);
    wearRow->addWidget(wear_, 1);
    wearValue_ = new QLabel;
    wearValue_->setObjectName("rowValue");
    wearValue_->setMinimumWidth(38);
    wearValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    wearRow->addWidget(wearValue_);
    right->addLayout(wearRow);

    auto *seedRow = new QHBoxLayout;
    seedRow->setSpacing(12);
    auto *seedLabel = new QLabel("Seed");
    seedLabel->setObjectName("rowLabel");
    seedRow->addWidget(seedLabel);
    seed_ = new QSlider(Qt::Horizontal);
    seed_->setRange(0, 1000);
    seed_->setValue(config.skin.seed);
    seed_->setFixedWidth(200);
    seedRow->addWidget(seed_, 1);
    seedValue_ = new QLabel;
    seedValue_->setObjectName("rowValue");
    seedValue_->setMinimumWidth(38);
    seedValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    seedRow->addWidget(seedValue_);
    right->addLayout(seedRow);

    status_ = new QLabel;
    status_->setObjectName("rowValue");
    status_->setWordWrap(true);
    right->addWidget(status_);

    columns->addLayout(right, 1);
    root->addLayout(columns, 1);

    connect(search_, &QLineEdit::textChanged, this, [this](const QString &) { refresh(); });
    connect(enabled_, &QPushButton::toggled, this, [this](bool on) {
      config.skin.enabled = on;
      updateStatus();
      changed();
    });
    connect(items_, &QListWidget::currentRowChanged, this, [this](int) { selectItem(); });
    connect(paints_, &QListWidget::currentRowChanged, this, [this](int) { selectPaint(); });
    connect(wear_, &QSlider::valueChanged, this, [this](int value) {
      config.skin.wear = value / 100.0f;
      wearValue_->setText(QString::number(value) + "%");
      changed();
    });
    connect(seed_, &QSlider::valueChanged, this, [this](int value) {
      config.skin.seed = value;
      seedValue_->setText(QString::number(value));
      changed();
    });

    wearValue_->setText(QString::number(wear_->value()) + "%");
    seedValue_->setText(QString::number(seed_->value()));
    enabled_->setChecked(config.skin.enabled);

    fillItems();
    updateStatus();
  }

  bool loaded() const { return loaded_; }

private:
  void fillItems() {
    items_->clear();
    for (const SkinItem &item : database_.items()) {
      if (item.paints.isEmpty()) continue;
      QString label = item.display;
      if (item.kind == "knife") {
        label += "  ·  knife";
      } else if (item.kind == "glove") {
        label += "  ·  gloves";
      }
      auto *entry = new QListWidgetItem(label);
      entry->setForeground(QColor(0xC2, 0xCA, 0xD8));
      entry->setData(Qt::UserRole, item.def_index);
      entry->setToolTip(item.name);
      items_->addItem(entry);
    }

    /* land on the weapon the config already points at */
    for (int i = 0; i < items_->count(); ++i) {
      if (items_->item(i)->data(Qt::UserRole).toInt() == config.skin.item) {
        items_->setCurrentRow(i);
        break;
      }
    }
    if (!items_->currentItem() && items_->count() > 0) {
      items_->setCurrentRow(0);
    }
    refresh();
  }

  void refresh() {
    const QString needle = search_->text().trimmed();

    for (int i = 0; i < items_->count(); ++i) {
      QListWidgetItem *entry = items_->item(i);
      const SkinItem *item = database_.item(entry->data(Qt::UserRole).toInt());
      const bool match = item && (needle.isEmpty() ||
                                  item->display.contains(needle, Qt::CaseInsensitive) ||
                                  item->name.contains(needle, Qt::CaseInsensitive));
      entry->setHidden(!match);
    }

    const QListWidgetItem *current = items_->currentItem();
    const int def_index = current ? current->data(Qt::UserRole).toInt() : 0;
    const SkinItem *item = database_.item(def_index);
    if (!item) {
      paints_->clear();
      return;
    }

    paints_->clear();
    for (int paint_id : item->paints) {
      const Paint *paint = database_.paint(paint_id);
      if (!paint) continue;
      if (!needle.isEmpty() && !paint->display.contains(needle, Qt::CaseInsensitive) &&
          !paint->name.contains(needle, Qt::CaseInsensitive)) {
        continue;
      }
      auto *entry = new QListWidgetItem(paint->display);
      entry->setData(Qt::UserRole, paint->id);
      entry->setForeground(SkinDatabase::rarity_color(paint->rarity));
      entry->setToolTip(QString("%1 · %2").arg(paint->name).arg(paint->rarity));
      paints_->addItem(entry);
    }

    if (config.skin.paint) {
      for (int i = 0; i < paints_->count(); ++i) {
        if (paints_->item(i)->data(Qt::UserRole).toInt() == config.skin.paint) {
          paints_->setCurrentRow(i);
          break;
        }
      }
    }
  }

  void selectItem() {
    QListWidgetItem *entry = items_->currentItem();
    if (!entry) return;
    config.skin.item = entry->data(Qt::UserRole).toInt();
    config.skin.paint = 0;
    refresh();
    updateStatus();
    changed();
  }

  void selectPaint() {
    QListWidgetItem *entry = paints_->currentItem();
    if (!entry) return;
    config.skin.paint = entry->data(Qt::UserRole).toInt();
    const Paint *paint = database_.paint(config.skin.paint);
    if (paint && wear_->value() == 0 && paint->wear > 0.0f) {
      wear_->setValue((int)(paint->wear * 100));
    }
    updateStatus();
    changed();
  }

  void updateStatus() {
    if (!loaded_) {
      status_->setText("No skin database found — run tools/fetch_skins.py to extract it from "
                       "your CS2 install.");
      return;
    }
    if (!config.skin.enabled) {
      status_->setText("Skin changer is off.");
      return;
    }
    const Paint *paint = database_.paint(config.skin.paint);
    const SkinItem *item = database_.item(config.skin.item);
    if (!paint || !item) {
      status_->setText("Pick a weapon and a finish.");
      return;
    }
    status_->setText(QString("%1 · %2 — applied to the weapon you hold, wear %3%, seed %4")
                          .arg(item->display, paint->display)
                          .arg(wear_->value())
                          .arg(seed_->value()));
  }

  void changed() {
    if (on_change_) on_change_();
  }

  SkinDatabase database_;
  bool loaded_ = false;
  std::function<void()> on_change_;

  QLineEdit *search_ = nullptr;
  QPushButton *enabled_ = nullptr;
  QListWidget *items_ = nullptr;
  QListWidget *paints_ = nullptr;
  QSlider *wear_ = nullptr;
  QSlider *seed_ = nullptr;
  QLabel *wearValue_ = nullptr;
  QLabel *seedValue_ = nullptr;
  QLabel *status_ = nullptr;
};

#endif
