#ifndef FLER_GLB_HPP
#define FLER_GLB_HPP

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>
#include <QVector3D>

/* Minimal binary glTF reader — enough for the agent meshes VRF exports. */
struct Mesh3D {
  QVector<QVector3D> positions;
  QVector<QVector3D> normals;
  QVector<uint32_t> indices;
};

struct Model3D {
  QVector<Mesh3D> parts;

  float min_y = 0.0f;
  float max_y = 0.0f;
  bool valid = false;

  float height() const { return max_y - min_y; }
};

namespace Glb {

  inline uint32_t read_u32(const char *p) {
    uint32_t v = 0;
    memcpy(&v, p, 4);
    return v;
  }

  inline bool load(const QString &path, Model3D &model) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QByteArray raw = file.readAll();
    if (raw.size() < 20) return false;

    const char *data = raw.constData();
    if (read_u32(data) != 0x46546C67) return false; /* 'glTF' */

    const int json_type = 0x4E4F534A; /* JSON */
    const int bin_type = 0x004E4942;  /* BIN  */

    QByteArray json_chunk;
    QByteArray bin_chunk;

    int offset = 12;
    while (offset + 8 <= raw.size()) {
      const uint32_t length = read_u32(data + offset);
      const uint32_t type = read_u32(data + offset + 4);
      if (offset + 8 + (int)length > raw.size()) break;

      if (type == (uint32_t)json_type) {
        json_chunk = raw.mid(offset + 8, length);
      } else if (type == (uint32_t)bin_type) {
        bin_chunk = raw.mid(offset + 8, length);
      }
      offset += 8 + length;
    }

    if (json_chunk.isEmpty() || bin_chunk.isEmpty()) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(json_chunk);
    if (!doc.isObject()) return false;
    const QJsonObject root = doc.object();

    const QJsonArray views = root["bufferViews"].toArray();
    const QJsonArray accessors = root["accessors"].toArray();
    const QJsonArray meshes = root["meshes"].toArray();
    if (meshes.isEmpty()) return false;

    auto view_base = [&](int index) -> const char * {
      if (index < 0 || index >= views.size()) return nullptr;
      const QJsonObject view = views[index].toObject();
      return bin_chunk.constData() + view["byteOffset"].toInt(0);
    };

    auto view_stride = [&](int index) -> int {
      if (index < 0 || index >= views.size()) return 0;
      return views[index].toObject()["byteStride"].toInt(0);
    };

    auto read_vec3 = [&](int accessor_index, QVector<QVector3D> &out) -> bool {
      if (accessor_index < 0 || accessor_index >= accessors.size()) return false;
      const QJsonObject acc = accessors[accessor_index].toObject();
      if (acc["componentType"].toInt() != 5126) return false; /* float */

      const char *base = view_base(acc["bufferView"].toInt(-1));
      if (!base) return false;

      const int stride = view_stride(acc["bufferView"].toInt(-1));
      const int elem = stride ? stride : 12;
      const int start = acc["byteOffset"].toInt(0);
      const int count = acc["count"].toInt(0);

      out.resize(count);
      for (int i = 0; i < count; ++i) {
        float v[3];
        memcpy(v, base + start + i * elem, 12);
        out[i] = QVector3D(v[0], v[1], v[2]);
      }
      return true;
    };

    auto read_indices = [&](int accessor_index, QVector<uint32_t> &out) -> bool {
      if (accessor_index < 0 || accessor_index >= accessors.size()) return false;
      const QJsonObject acc = accessors[accessor_index].toObject();
      const int component = acc["componentType"].toInt();

      const char *base = view_base(acc["bufferView"].toInt(-1));
      if (!base) return false;

      const int stride = view_stride(acc["bufferView"].toInt(-1));
      const int elem = stride ? stride : (component == 5125 ? 4 : 2);
      const int start = acc["byteOffset"].toInt(0);
      const int count = acc["count"].toInt(0);

      out.resize(count);
      for (int i = 0; i < count; ++i) {
        const char *p = base + start + i * elem;
        if (component == 5125) {
          uint32_t v = 0;
          memcpy(&v, p, 4);
          out[i] = v;
        } else {
          uint16_t v = 0;
          memcpy(&v, p, 2);
          out[i] = v;
        }
      }
      return true;
    };

    /* pick the third person body when the model ships several meshes */
    int mesh_index = 0;
    for (int i = 0; i < meshes.size(); ++i) {
      const QString name = meshes[i].toObject()["name"].toString();
      if (name.contains("thirdperson_body")) {
        mesh_index = i;
        break;
      }
    }

    const QJsonObject mesh = meshes[mesh_index].toObject();
    const QJsonArray primitives = mesh["primitives"].toArray();

    for (const QJsonValue &value : primitives) {
      const QJsonObject prim = value.toObject();
      const QJsonObject attributes = prim["attributes"].toObject();

      Mesh3D part;
      if (!read_vec3(attributes["POSITION"].toInt(-1), part.positions)) continue;
      read_vec3(attributes["NORMAL"].toInt(-1), part.normals);
      read_indices(prim["indices"].toInt(-1), part.indices);
      if (part.positions.isEmpty()) continue;

      model.parts.push_back(part);
    }

    if (model.parts.isEmpty()) return false;

    model.min_y = model.max_y = model.parts.first().positions.first().y();
    for (const Mesh3D &part : model.parts) {
      for (const QVector3D &p : part.positions) {
        model.min_y = qMin(model.min_y, p.y());
        model.max_y = qMax(model.max_y, p.y());
      }
    }
    model.valid = true;
    return true;
  }
}

#endif
