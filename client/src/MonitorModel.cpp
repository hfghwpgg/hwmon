#include "MonitorModel.hpp"

#include "Types.hpp"
#include "UiStyle.hpp"

#include <QCollator>
#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QSvgRenderer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

constexpr quintptr kSectionMask = 0xFFFF;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

const QString &mimeType() {
  static const QString type = QStringLiteral("application/x-hwmon-item");
  return type;
}

quintptr makeSectionId(int deviceIndex) {
  return quintptr(deviceIndex + 1) << 16;
}

quintptr makeSensorId(int deviceIndex, int sectionIndex) {
  return (quintptr(deviceIndex + 1) << 16) | quintptr(sectionIndex + 1);
}

int deviceIndexFromId(quintptr id) {
  return int(id >> 16) - 1;
}

int sectionIndexFromId(quintptr id) {
  return int(id & kSectionMask) - 1;
}

qint64 nowMs() {
  return QDateTime::currentMSecsSinceEpoch();
}

QSet<QString> toSet(const QStringList &list) {
  return QSet<QString>(list.cbegin(), list.cend());
}

int visibleSensors(const SectionData &section) {
  return static_cast<int>(std::count_if(section.sensors.cbegin(), section.sensors.cend(),
                                        [](const SensorData &s) { return !s.hidden; }));
}

bool anyHidden(const SectionData &section) {
  return std::any_of(section.sensors.cbegin(), section.sensors.cend(),
                     [](const SensorData &s) { return s.hidden; });
}

QString formatBytes(double bytes) {
  static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  int unit = 0;
  while (bytes >= 1024.0 && unit < 4) {
    bytes /= 1024.0;
    ++unit;
  }
  return QString::number(bytes, 'f', unit == 0 ? 0 : 2) + QLatin1Char(' ') +
         QLatin1String(units[unit]);
}

QString formatValue(int type, double value) {
  if (!std::isfinite(value)) {
    return QStringLiteral("—");
  }
  if (type == kMemorySensorType || type == kThroughputSensorType) {
    return formatBytes(value) + sensorTypeInfo(type).unit.toString();
  }
  if (type < 0 || type >= kUnknownSensorType) {
    return QString::number(value, 'g', 4);
  }
  const SensorTypeInfo &info = sensorTypeInfo(type);
  return QString::number(value, 'f', info.precision) + info.unit.toString();
}

// Value/flash state for one row (sensor or a section's primary sensor).
struct Readings {
  bool valid = false;
  int type = kUnknownSensorType;
  double current = 0.0;
  double min = 0.0;
  double max = 0.0;
  double avg = kNaN;
  bool flashCurrent = false;
  bool flashMin = false;
  bool flashMax = false;

  double forColumn(int column) const {
    switch (column) {
    case MonitorModel::CurrentColumn:
      return current;
    case MonitorModel::MinColumn:
      return min;
    case MonitorModel::MaxColumn:
      return max;
    case MonitorModel::AvgColumn:
      return avg;
    default:
      return kNaN;
    }
  }

  bool flashForColumn(int column) const {
    switch (column) {
    case MonitorModel::CurrentColumn:
      return flashCurrent;
    case MonitorModel::MinColumn:
      return flashMin;
    case MonitorModel::MaxColumn:
      return flashMax;
    default:
      return false;
    }
  }
};

Readings readingsFor(const SensorData &sensor, qint64 now) {
  Readings r;
  r.valid = true;
  r.type = sensor.type;
  r.current = sensor.value;
  r.min = sensor.min;
  r.max = sensor.max;
  r.avg = sensor.times > 0 ? sensor.sum / static_cast<double>(sensor.times) : kNaN;
  r.flashCurrent = sensor.flashCurrentUntil > now;
  r.flashMin = sensor.flashMinUntil > now;
  r.flashMax = sensor.flashMaxUntil > now;
  return r;
}

Readings primaryReadings(const SectionData &section, qint64 now) {
  for (const SensorData &sensor : section.sensors) {
    if (sensor.primary && !sensor.hidden) {
      return readingsFor(sensor, now);
    }
  }
  return {};
}

bool isHotTemperature(int type, double value) {
  return type == 0 && std::isfinite(value) && value >= kHotTemperatureC;
}

QString labelledName(const QString &iconPath, const QString &label, const QString &name) {
  return iconPath.isEmpty() ? QStringLiteral("[%1]  %2").arg(label, name) : name;
}

QIcon tintedSvgIcon(const QString &path, bool dark) {
  if (path.isEmpty()) {
    return {};
  }

  static QHash<QPair<QString, bool>, QIcon> cache;
  const auto key = qMakePair(path, dark);
  if (auto it = cache.constFind(key); it != cache.cend()) {
    return it.value();
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }

  QSvgRenderer renderer(file.readAll());
  if (!renderer.isValid()) {
    return {};
  }

  const QColor color = accentColor(dark);
  QIcon icon;
  for (const int size : {16, 32, 48}) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    icon.addPixmap(pixmap);
  }
  cache.insert(key, icon);
  return icon;
}

void sortSensorsByName(QVector<SensorData> &sensors) {
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  std::sort(sensors.begin(), sensors.end(), [&](const SensorData &left, const SensorData &right) {
    const int compared = collator.compare(left.name, right.name);
    return compared != 0 ? compared < 0 : left.key < right.key;
  });
}

// Reorders items so that those listed in `order` (by key) come first, in that
// order; the rest keep their relative order at the end.
template <typename T> QVector<T> reorderByKey(QVector<T> items, const QStringList &order) {
  QHash<QString, int> indexByKey;
  indexByKey.reserve(items.size());
  for (int i = 0; i < items.size(); ++i) {
    indexByKey.insert(items[i].key, i);
  }

  QVector<T> ordered;
  ordered.reserve(items.size());
  QVector<bool> used(items.size(), false);
  for (const QString &key : order) {
    const auto it = indexByKey.constFind(key);
    if (it == indexByKey.cend() || used[it.value()]) {
      continue;
    }
    ordered.push_back(std::move(items[it.value()]));
    used[it.value()] = true;
  }
  for (int i = 0; i < items.size(); ++i) {
    if (!used[i]) {
      ordered.push_back(std::move(items[i]));
    }
  }
  return ordered;
}

// Carries UI-only state (hidden flag, flash timers) from the previous snapshot.
void carryState(const SensorData &old, SensorData &next, qint64 flashUntil) {
  next.hidden = old.hidden;
  next.flashCurrentUntil = next.value != old.value ? flashUntil : old.flashCurrentUntil;
  next.flashMinUntil = next.min != old.min ? flashUntil : old.flashMinUntil;
  next.flashMaxUntil = next.max != old.max ? flashUntil : old.flashMaxUntil;
}

SensorData parseSensor(const QString &deviceKey, QHash<QString, int> &sensorCounts,
                       const QJsonObject &sensorObject) {
  SensorData sensor;
  sensor.name = sensorObject.value(QLatin1String("name")).toString();
  sensor.type = sensorObject.value(QLatin1String("type")).toInt(kUnknownSensorType);
  const QString identity = QString::number(sensor.type) + QLatin1Char('|') + sensor.name;
  const int occurrence = sensorCounts[identity]++;
  sensor.key = QStringLiteral("%1/%2:%3#%4")
                   .arg(deviceKey)
                   .arg(sensor.type)
                   .arg(sensor.name)
                   .arg(occurrence);

  const QJsonObject readings = sensorObject.value(QLatin1String("readings")).toObject();
  sensor.value = readings.value(QLatin1String("value")).toDouble();
  sensor.min = readings.value(QLatin1String("min_value")).toDouble();
  sensor.max = readings.value(QLatin1String("max_value")).toDouble();
  sensor.sum = readings.value(QLatin1String("sum")).toDouble();
  sensor.times = readings.value(QLatin1String("times")).toVariant().toLongLong();
  const QJsonValue primary = sensorObject.value(QLatin1String("isPrimary"));
  sensor.primary = primary.toBool() || primary.toInt() != 0;
  return sensor;
}

QVector<SensorData> parseSensorArray(const QString &deviceKey, QHash<QString, int> &sensorCounts,
                                     const QJsonArray &sensorArray) {
  QVector<SensorData> sensors;
  sensors.reserve(sensorArray.size());
  for (const QJsonValue &sensorValue : sensorArray) {
    if (sensorValue.isObject()) {
      sensors.push_back(parseSensor(deviceKey, sensorCounts, sensorValue.toObject()));
    }
  }
  return sensors;
}

} // namespace

MonitorModel::MonitorModel(QObject *parent) :
    QAbstractItemModel(parent) {
  loadSettings();
}

QModelIndex MonitorModel::index(int row, int column, const QModelIndex &parent) const {
  if (row < 0 || column < 0 || column >= ColumnCount) {
    return {};
  }
  if (!parent.isValid()) {
    return row < m_devices.size() ? createIndex(row, column, quintptr(0)) : QModelIndex();
  }
  if (isDevice(parent)) {
    return row < visibleSectionCount(parent.row())
               ? createIndex(row, column, makeSectionId(parent.row()))
               : QModelIndex();
  }
  if (isSection(parent)) {
    const int deviceIndex = deviceIndexFromId(parent.internalId());
    const int sectionIndex = visibleToSection(deviceIndex, parent.row());
    if (sectionIndex < 0 || row >= visibleSensorCount(deviceIndex, sectionIndex)) {
      return {};
    }
    return createIndex(row, column, makeSensorId(deviceIndex, sectionIndex));
  }
  return {};
}

QModelIndex MonitorModel::parent(const QModelIndex &child) const {
  if (!child.isValid() || isDevice(child)) {
    return {};
  }
  const int deviceIndex = deviceIndexFromId(child.internalId());
  if (isSection(child)) {
    return createIndex(deviceIndex, 0, quintptr(0));
  }
  const int visible = sectionToVisible(deviceIndex, sectionIndexFromId(child.internalId()));
  return visible < 0 ? QModelIndex() : createIndex(visible, 0, makeSectionId(deviceIndex));
}

int MonitorModel::rowCount(const QModelIndex &parent) const {
  if (!parent.isValid()) {
    return m_devices.size();
  }
  if (isDevice(parent)) {
    return visibleSectionCount(parent.row());
  }
  if (isSection(parent)) {
    const int deviceIndex = deviceIndexFromId(parent.internalId());
    return visibleSensorCount(deviceIndex, visibleToSection(deviceIndex, parent.row()));
  }
  return 0;
}

int MonitorModel::columnCount(const QModelIndex &) const {
  return ColumnCount;
}

QVariant MonitorModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  const DeviceData *device = deviceFromIndex(index);
  const SectionData *section = isSection(index) ? sectionFromIndex(index) : nullptr;
  const SensorData *sensor = isSensor(index) ? sensorFromIndex(index) : nullptr;
  if (device == nullptr || (isSection(index) && section == nullptr) ||
      (isSensor(index) && sensor == nullptr)) {
    return {};
  }

  const bool deviceRow = isDevice(index);
  const int column = index.column();
  const ThemeColors &theme = themeColors(m_darkTheme);
  const auto readings = [&]() -> Readings {
    if (sensor != nullptr) {
      return readingsFor(*sensor, nowMs());
    }
    return section != nullptr ? primaryReadings(*section, nowMs()) : Readings{};
  };

  switch (role) {
  case Qt::DisplayRole: {
    if (column == NameColumn) {
      if (deviceRow) {
        return labelledName(deviceIconPath(device->type), deviceTypeLabel(device->type),
                            device->name);
      }
      if (section != nullptr) {
        return section->name;
      }
      return labelledName(sensorIconPath(sensor->type), sensorTypeLabel(sensor->type),
                          sensor->name);
    }
    if (deviceRow) {
      return {};
    }
    const Readings r = readings();
    if (!r.valid || column < CurrentColumn || column > AvgColumn) {
      return {};
    }
    return formatValue(r.type, r.forColumn(column));
  }

  case Qt::DecorationRole: {
    if (column != NameColumn) {
      return {};
    }
    const QString path = deviceRow            ? deviceIconPath(device->type)
                         : section != nullptr ? sensorIconPath(section->type)
                                              : sensorIconPath(sensor->type);
    return tintedSvgIcon(path, m_darkTheme);
  }

  case Qt::FontRole: {
    QFont font;
    if (deviceRow || section != nullptr) {
      font.setBold(true);
      return sizedFont(font, deviceRow ? kFontDeviceName : kFontSectionName);
    }
    if (column != NameColumn) {
      font.setFamilies({QStringLiteral("monospace"), QStringLiteral("Noto Sans Mono")});
      return sizedFont(font, kFontSensorReading);
    }
    return sizedFont(font, kFontSensorName);
  }

  case Qt::TextAlignmentRole:
    if (column != NameColumn) {
      return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
    }
    return {};

  case Qt::ForegroundRole: {
    if (column == NameColumn) {
      return theme.accent;
    }
    const Readings r = readings();
    if (r.valid && isHotTemperature(r.type, r.forColumn(column))) {
      return theme.warning;
    }
    return theme.text;
  }

  case Qt::BackgroundRole: {
    if (deviceRow) {
      return theme.deviceRow;
    }
    const Readings r = readings();
    if (r.valid && r.flashForColumn(column)) {
      return theme.flash;
    }
    return section != nullptr ? QVariant(theme.sectionRow) : QVariant();
  }

  default:
    return {};
  }
}

QVariant MonitorModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return {};
  }
  switch (section) {
  case NameColumn:
    return QStringLiteral("Sensor");
  case CurrentColumn:
    return QStringLiteral("Current");
  case MinColumn:
    return QStringLiteral("Minimum");
  case MaxColumn:
    return QStringLiteral("Maximum");
  case AvgColumn:
    return QStringLiteral("Average");
  default:
    return {};
  }
}

Qt::ItemFlags MonitorModel::flags(const QModelIndex &index) const {
  if (!index.isValid()) {
    return Qt::ItemIsDropEnabled;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

Qt::DropActions MonitorModel::supportedDropActions() const {
  return Qt::MoveAction;
}

QStringList MonitorModel::mimeTypes() const {
  return {mimeType()};
}

QMimeData *MonitorModel::mimeData(const QModelIndexList &indexes) const {
  if (indexes.isEmpty()) {
    return nullptr;
  }
  const QModelIndex index = indexes.front().siblingAtColumn(0);
  const DeviceData *device = deviceFromIndex(index);
  if (device == nullptr) {
    return nullptr;
  }

  QByteArray payload;
  if (isDevice(index)) {
    payload = "D\n" + device->key.toUtf8();
  } else if (isSection(index)) {
    payload = "G\n" + device->key.toUtf8() + '\n' + sectionFromIndex(index)->key.toUtf8();
  } else {
    payload = "S\n" + device->key.toUtf8() + '\n' + sectionFromIndex(index)->key.toUtf8() + '\n' +
              sensorFromIndex(index)->key.toUtf8();
  }
  auto *mime = new QMimeData;
  mime->setData(mimeType(), payload);
  return mime;
}

bool MonitorModel::canDropMimeData(const QMimeData *data, Qt::DropAction, int, int,
                                   const QModelIndex &parent) const {
  if (data == nullptr || !data->hasFormat(mimeType())) {
    return false;
  }
  const QList<QByteArray> parts = data->data(mimeType()).split('\n');
  const QByteArray kind = parts.value(0);
  if (kind == "D") {
    return true;
  }
  // deviceFromIndex / sectionFromIndex resolve any node level via the internal id.
  const DeviceData *device = deviceFromIndex(parent);
  if (device == nullptr || device->key != QString::fromUtf8(parts.value(1))) {
    return false;
  }
  if (kind == "G" && parts.size() >= 3) {
    return true;
  }
  if (kind == "S" && parts.size() >= 4) {
    const SectionData *section = sectionFromIndex(parent);
    return section != nullptr && section->key == QString::fromUtf8(parts.at(2));
  }
  return false;
}

bool MonitorModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int,
                                const QModelIndex &parent) {
  if (action != Qt::MoveAction || !canDropMimeData(data, action, row, 0, parent)) {
    return false;
  }

  const QList<QByteArray> parts = data->data(mimeType()).split('\n');
  const QByteArray kind = parts.value(0);
  const QString deviceKey = QString::fromUtf8(parts.value(1));

  if (kind == "D") {
    int to = row;
    if (parent.isValid()) {
      to = isDevice(parent) ? parent.row() : deviceIndexFromId(parent.internalId());
    }
    if (to < 0) {
      to = m_devices.size();
    }
    return moveDevice(findDeviceIndex(deviceKey), to);
  }

  const int deviceIndex = findDeviceIndex(deviceKey);
  if (deviceIndex < 0) {
    return false;
  }
  const QString sectionKey = QString::fromUtf8(parts.value(2));

  if (kind == "G") {
    const int visible = visibleSectionCount(deviceIndex);
    const int fromVisible =
        sectionToVisible(deviceIndex, findSectionIndex(deviceIndex, sectionKey));
    int toVisible = row;
    if (isSection(parent)) {
      toVisible = parent.row();
    } else if (isSensor(parent)) {
      toVisible = sectionToVisible(deviceIndex, sectionIndexFromId(parent.internalId()));
    }
    if (toVisible < 0) {
      toVisible = visible;
    }
    return moveSection(deviceIndex, fromVisible, toVisible);
  }

  const int sectionIndex = findSectionIndex(deviceIndex, sectionKey);
  if (sectionIndex < 0) {
    return false;
  }
  const QString sensorKey = QString::fromUtf8(parts.value(3));
  const int visible = visibleSensorCount(deviceIndex, sectionIndex);
  int fromVisible = -1;
  int seen = 0;
  for (const SensorData &sensor : m_devices[deviceIndex].sections[sectionIndex].sensors) {
    if (sensor.hidden) {
      continue;
    }
    if (sensor.key == sensorKey) {
      fromVisible = seen;
      break;
    }
    ++seen;
  }
  int toVisible = row;
  if (isSection(parent)) {
    toVisible = qMax(0, row); // dropping onto the section header puts it first
  } else if (isSensor(parent)) {
    toVisible = parent.row();
  }
  if (toVisible < 0) {
    toVisible = visible;
  }
  return moveSensor(deviceIndex, sectionIndex, fromVisible, toVisible);
}

bool MonitorModel::applySnapshot(const QJsonDocument &document, qint64 *timestampOut) {
  if (!document.isArray()) {
    return false;
  }

  qint64 timestamp = 0;
  QVector<DeviceData> incoming = parseDevices(document.array(), &timestamp);
  if (timestampOut != nullptr) {
    *timestampOut = timestamp;
  }

  QHash<QString, int> existingIndex;
  existingIndex.reserve(m_devices.size());
  for (int i = 0; i < m_devices.size(); ++i) {
    existingIndex.insert(m_devices[i].key, i);
  }

  const qint64 flashUntil = nowMs() + m_flashDurationMs;
  for (DeviceData &device : incoming) {
    const auto existing = existingIndex.constFind(device.key);
    if (existing == existingIndex.cend()) {
      // First time we see this device: apply persisted preferences.
      device.expanded = !m_collapsedKeys.contains(device.key);
      const QStringList sensorOrder = m_sensorOrder.value(device.key);
      for (SectionData &section : device.sections) {
        for (SensorData &sensor : section.sensors) {
          sensor.hidden = m_hiddenKeys.contains(sensor.key);
        }
        if (!sensorOrder.isEmpty()) {
          section.sensors = reorderByKey(std::move(section.sensors), sensorOrder);
        }
      }
      if (auto order = m_sectionOrder.constFind(device.key); order != m_sectionOrder.cend()) {
        device.sections = reorderSections(std::move(device.sections), order.value());
      }
      continue;
    }

    // Known device: carry over UI state and keep the user's current ordering.
    const DeviceData &old = m_devices[existing.value()];
    device.expanded = old.expanded;
    QHash<QString, const SectionData *> oldSections;
    QHash<QString, const SensorData *> oldSensors;
    QStringList oldSectionOrder;
    oldSections.reserve(old.sections.size());
    oldSectionOrder.reserve(old.sections.size());
    for (const SectionData &section : old.sections) {
      oldSections.insert(section.key, &section);
      oldSectionOrder.push_back(section.key);
      for (const SensorData &sensor : section.sensors) {
        oldSensors.insert(sensor.key, &sensor);
      }
    }
    for (SectionData &section : device.sections) {
      QStringList previousOrder;
      if (const SectionData *previous = oldSections.value(section.key, nullptr)) {
        section.expanded = previous->expanded;
        previousOrder.reserve(previous->sensors.size());
        for (const SensorData &sensor : previous->sensors) {
          previousOrder.push_back(sensor.key);
        }
      }
      for (SensorData &sensor : section.sensors) {
        if (const SensorData *previous = oldSensors.value(sensor.key, nullptr)) {
          carryState(*previous, sensor, flashUntil);
        } else {
          sensor.hidden = m_hiddenKeys.contains(sensor.key);
        }
      }
      section.sensors = reorderByKey(std::move(section.sensors), previousOrder);
    }
    device.sections = reorderSections(std::move(device.sections), oldSectionOrder);
  }

  if (!m_initialized) {
    if (!m_deviceOrder.isEmpty()) {
      incoming = reorderByKey(std::move(incoming), m_deviceOrder);
    }
    m_initialized = true;
  } else if (!m_devices.isEmpty()) {
    QStringList currentOrder;
    currentOrder.reserve(m_devices.size());
    for (const DeviceData &device : m_devices) {
      currentOrder.push_back(device.key);
    }
    incoming = reorderByKey(std::move(incoming), currentOrder);
  }

  const bool reset = !structureEquals(incoming);
  if (reset) {
    beginResetModel();
  }
  m_devices = std::move(incoming);
  if (reset) {
    endResetModel();
  } else {
    notifyValues();
  }
  return true;
}

void MonitorModel::resetLocalReadings() {
  for (DeviceData &device : m_devices) {
    for (SectionData &section : device.sections) {
      for (SensorData &sensor : section.sensors) {
        sensor.min = sensor.value;
        sensor.max = sensor.value;
        sensor.sum = 0.0;
        sensor.times = 0;
        sensor.flashCurrentUntil = 0;
        sensor.flashMinUntil = 0;
        sensor.flashMaxUntil = 0;
      }
    }
  }
  notifyValues();
}

void MonitorModel::hideSensor(const QModelIndex &index) {
  SensorData *sensor = sensorFromIndex(index);
  if (sensor == nullptr || sensor->hidden) {
    return;
  }
  const int deviceIndex = deviceIndexFromId(index.internalId());
  const int sectionIndex = sectionIndexFromId(index.internalId());
  const int sectionVisible = sectionToVisible(deviceIndex, sectionIndex);
  const QModelIndex sectionParent = parent(index);
  const QModelIndex deviceParent = sectionParent.parent();
  const bool lastVisible = visibleSensorCount(deviceIndex, sectionIndex) == 1;

  beginRemoveRows(sectionParent, index.row(), index.row());
  sensor->hidden = true;
  m_hiddenKeys.insert(sensor->key);
  endRemoveRows();

  if (lastVisible && sectionVisible >= 0) {
    beginRemoveRows(deviceParent, sectionVisible, sectionVisible);
    endRemoveRows();
  }
  saveSettings();
}

void MonitorModel::showHiddenSensors(const QModelIndex &index) {
  DeviceData *device = deviceFromIndex(index);
  if (device == nullptr) {
    return;
  }
  bool changed = false;
  auto unhide = [&](SectionData &section) {
    for (SensorData &sensor : section.sensors) {
      if (sensor.hidden) {
        sensor.hidden = false;
        m_hiddenKeys.remove(sensor.key);
        changed = true;
      }
    }
  };
  if (isSection(index)) {
    if (SectionData *section = sectionFromIndex(index)) {
      unhide(*section);
    }
  } else {
    for (SectionData &section : device->sections) {
      unhide(section);
    }
  }
  if (!changed) {
    return;
  }
  beginResetModel();
  endResetModel();
  saveSettings();
}

bool MonitorModel::hasHiddenSensors(const QModelIndex &index) const {
  if (isSection(index)) {
    const SectionData *section = sectionFromIndex(index);
    return section != nullptr && anyHidden(*section);
  }
  const DeviceData *device = deviceFromIndex(index);
  return device != nullptr &&
         std::any_of(device->sections.cbegin(), device->sections.cend(), anyHidden);
}

bool MonitorModel::isDevice(const QModelIndex &index) const {
  return index.isValid() && index.internalId() == 0;
}

bool MonitorModel::isSection(const QModelIndex &index) const {
  return index.isValid() && index.internalId() != 0 && (index.internalId() & kSectionMask) == 0;
}

bool MonitorModel::isSensor(const QModelIndex &index) const {
  return index.isValid() && (index.internalId() & kSectionMask) != 0;
}

void MonitorModel::setNodeExpanded(const QModelIndex &index, bool expanded) {
  QString key;
  if (isDevice(index)) {
    DeviceData *device = deviceFromIndex(index);
    if (device == nullptr) {
      return;
    }
    device->expanded = expanded;
    key = device->key;
  } else if (isSection(index)) {
    SectionData *section = sectionFromIndex(index);
    if (section == nullptr) {
      return;
    }
    section->expanded = expanded;
    key = section->key;
  } else {
    return;
  }
  if (expanded) {
    m_collapsedKeys.remove(key);
  } else {
    m_collapsedKeys.insert(key);
  }
  saveSettings();
}

bool MonitorModel::isNodeExpanded(const QModelIndex &index) const {
  if (isDevice(index)) {
    const DeviceData *device = deviceFromIndex(index);
    return device != nullptr && device->expanded;
  }
  if (isSection(index)) {
    const SectionData *section = sectionFromIndex(index);
    return section != nullptr && section->expanded;
  }
  return false;
}

void MonitorModel::setDarkTheme(bool dark) {
  if (m_darkTheme == dark) {
    return;
  }
  m_darkTheme = dark;
  notifyValues();
}

void MonitorModel::setFlashDurationMs(int ms) {
  m_flashDurationMs = qMax(50, ms);
}

void MonitorModel::resetSensorOrder() {
  beginResetModel();
  for (DeviceData &device : m_devices) {
    for (SectionData &section : device.sections) {
      sortSensorsByName(section.sensors);
    }
  }
  endResetModel();
  saveSettings();
}

int MonitorModel::findDeviceIndex(const QString &key) const {
  for (int i = 0; i < m_devices.size(); ++i) {
    if (m_devices[i].key == key) {
      return i;
    }
  }
  return -1;
}

int MonitorModel::findSectionIndex(int deviceIndex, const QString &key) const {
  if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
    return -1;
  }
  const auto &sections = m_devices[deviceIndex].sections;
  for (int i = 0; i < sections.size(); ++i) {
    if (sections[i].key == key) {
      return i;
    }
  }
  return -1;
}

int MonitorModel::visibleSectionCount(int deviceIndex) const {
  if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
    return 0;
  }
  const auto &sections = m_devices[deviceIndex].sections;
  return static_cast<int>(
      std::count_if(sections.cbegin(), sections.cend(),
                    [](const SectionData &s) { return visibleSensors(s) > 0; }));
}

int MonitorModel::visibleToSection(int deviceIndex, int visibleRow) const {
  if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
    return -1;
  }
  int seen = 0;
  const auto &sections = m_devices[deviceIndex].sections;
  for (int i = 0; i < sections.size(); ++i) {
    if (visibleSensors(sections[i]) == 0) {
      continue;
    }
    if (seen == visibleRow) {
      return i;
    }
    ++seen;
  }
  return -1;
}

int MonitorModel::sectionToVisible(int deviceIndex, int sectionIndex) const {
  if (visibleSensorCount(deviceIndex, sectionIndex) == 0) {
    return -1;
  }
  const auto &sections = m_devices[deviceIndex].sections;
  int visible = 0;
  for (int i = 0; i < sectionIndex; ++i) {
    if (visibleSensors(sections[i]) > 0) {
      ++visible;
    }
  }
  return visible;
}

int MonitorModel::visibleSensorCount(int deviceIndex, int sectionIndex) const {
  if (deviceIndex < 0 || deviceIndex >= m_devices.size() || sectionIndex < 0 ||
      sectionIndex >= m_devices[deviceIndex].sections.size()) {
    return 0;
  }
  return visibleSensors(m_devices[deviceIndex].sections[sectionIndex]);
}

int MonitorModel::visibleToSensor(int deviceIndex, int sectionIndex, int visibleRow) const {
  int seen = 0;
  const auto &sensors = m_devices[deviceIndex].sections[sectionIndex].sensors;
  for (int i = 0; i < sensors.size(); ++i) {
    if (sensors[i].hidden) {
      continue;
    }
    if (seen == visibleRow) {
      return i;
    }
    ++seen;
  }
  return -1;
}

const DeviceData *MonitorModel::deviceFromIndex(const QModelIndex &index) const {
  if (!index.isValid()) {
    return nullptr;
  }
  const int deviceIndex = isDevice(index) ? index.row() : deviceIndexFromId(index.internalId());
  if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
    return nullptr;
  }
  return &m_devices[deviceIndex];
}

DeviceData *MonitorModel::deviceFromIndex(const QModelIndex &index) {
  return const_cast<DeviceData *>(std::as_const(*this).deviceFromIndex(index));
}

const SectionData *MonitorModel::sectionFromIndex(const QModelIndex &index) const {
  if (!index.isValid() || isDevice(index)) {
    return nullptr;
  }
  const int deviceIndex = deviceIndexFromId(index.internalId());
  const int sectionIndex = isSection(index) ? visibleToSection(deviceIndex, index.row())
                                            : sectionIndexFromId(index.internalId());
  if (deviceIndex < 0 || deviceIndex >= m_devices.size() || sectionIndex < 0 ||
      sectionIndex >= m_devices[deviceIndex].sections.size()) {
    return nullptr;
  }
  return &m_devices[deviceIndex].sections[sectionIndex];
}

SectionData *MonitorModel::sectionFromIndex(const QModelIndex &index) {
  return const_cast<SectionData *>(std::as_const(*this).sectionFromIndex(index));
}

const SensorData *MonitorModel::sensorFromIndex(const QModelIndex &index) const {
  if (!isSensor(index)) {
    return nullptr;
  }
  const int deviceIndex = deviceIndexFromId(index.internalId());
  const int sectionIndex = sectionIndexFromId(index.internalId());
  if (visibleSensorCount(deviceIndex, sectionIndex) == 0) {
    return nullptr;
  }
  const int sensorIndex = visibleToSensor(deviceIndex, sectionIndex, index.row());
  if (sensorIndex < 0) {
    return nullptr;
  }
  return &m_devices[deviceIndex].sections[sectionIndex].sensors[sensorIndex];
}

SensorData *MonitorModel::sensorFromIndex(const QModelIndex &index) {
  return const_cast<SensorData *>(std::as_const(*this).sensorFromIndex(index));
}

void MonitorModel::loadSettings() {
  QSettings settings;
  m_collapsedKeys = toSet(settings.value(QStringLiteral("collapsed")).toStringList());
  m_hiddenKeys = toSet(settings.value(QStringLiteral("hidden")).toStringList());
  m_deviceOrder = settings.value(QStringLiteral("deviceOrder")).toStringList();

  const auto readOrders = [&settings](const QString &group, QHash<QString, QStringList> &out) {
    const int count = settings.beginReadArray(group);
    for (int i = 0; i < count; ++i) {
      settings.setArrayIndex(i);
      const QString key = settings.value(QStringLiteral("device")).toString();
      if (!key.isEmpty()) {
        out.insert(key, settings.value(QStringLiteral("order")).toStringList());
      }
    }
    settings.endArray();
  };
  readOrders(QStringLiteral("sensorOrders"), m_sensorOrder);
  readOrders(QStringLiteral("sectionOrders"), m_sectionOrder);
}

void MonitorModel::saveSettings() const {
  QSettings settings;
  QStringList collapsed(m_collapsedKeys.cbegin(), m_collapsedKeys.cend());
  collapsed.sort();
  QStringList hidden(m_hiddenKeys.cbegin(), m_hiddenKeys.cend());
  hidden.sort();

  QStringList deviceOrder;
  deviceOrder.reserve(m_devices.size());
  for (const DeviceData &device : m_devices) {
    deviceOrder.push_back(device.key);
  }

  settings.setValue(QStringLiteral("collapsed"), collapsed);
  settings.setValue(QStringLiteral("hidden"), hidden);
  settings.setValue(QStringLiteral("deviceOrder"), deviceOrder);

  const auto writeOrders = [&](const QString &group, auto keysOf) {
    settings.beginWriteArray(group, m_devices.size());
    for (int i = 0; i < m_devices.size(); ++i) {
      settings.setArrayIndex(i);
      settings.setValue(QStringLiteral("device"), m_devices[i].key);
      settings.setValue(QStringLiteral("order"), keysOf(m_devices[i]));
    }
    settings.endArray();
  };
  writeOrders(QStringLiteral("sensorOrders"), [](const DeviceData &device) {
    QStringList order;
    for (const SectionData &section : device.sections) {
      for (const SensorData &sensor : section.sensors) {
        order.push_back(sensor.key);
      }
    }
    return order;
  });
  writeOrders(QStringLiteral("sectionOrders"), [](const DeviceData &device) {
    QStringList order;
    order.reserve(device.sections.size());
    for (const SectionData &section : device.sections) {
      order.push_back(section.key);
    }
    return order;
  });
}

void MonitorModel::notifyValues() {
  if (m_devices.isEmpty()) {
    return;
  }
  emit dataChanged(index(0, 0), index(m_devices.size() - 1, ColumnCount - 1));
  for (int deviceIndex = 0; deviceIndex < m_devices.size(); ++deviceIndex) {
    const QModelIndex device = index(deviceIndex, 0);
    const int sections = visibleSectionCount(deviceIndex);
    if (sections == 0) {
      continue;
    }
    emit dataChanged(index(0, 0, device), index(sections - 1, ColumnCount - 1, device));
    for (int sectionVisible = 0; sectionVisible < sections; ++sectionVisible) {
      const QModelIndex section = index(sectionVisible, 0, device);
      const int sensors =
          visibleSensorCount(deviceIndex, visibleToSection(deviceIndex, sectionVisible));
      if (sensors > 0) {
        emit dataChanged(index(0, 0, section), index(sensors - 1, ColumnCount - 1, section));
      }
    }
  }
}

bool MonitorModel::structureEquals(const QVector<DeviceData> &other) const {
  if (m_devices.size() != other.size()) {
    return false;
  }
  for (int i = 0; i < m_devices.size(); ++i) {
    const DeviceData &a = m_devices[i];
    const DeviceData &b = other[i];
    if (a.key != b.key || a.sections.size() != b.sections.size()) {
      return false;
    }
    for (int s = 0; s < a.sections.size(); ++s) {
      const SectionData &sa = a.sections[s];
      const SectionData &sb = b.sections[s];
      if (sa.key != sb.key || sa.name != sb.name || sa.custom != sb.custom ||
          sa.sensors.size() != sb.sensors.size()) {
        return false;
      }
      for (int j = 0; j < sa.sensors.size(); ++j) {
        if (sa.sensors[j].key != sb.sensors[j].key ||
            sa.sensors[j].hidden != sb.sensors[j].hidden) {
          return false;
        }
      }
    }
  }
  return true;
}

QVector<DeviceData> MonitorModel::parseDevices(const QJsonArray &root, qint64 *timestampOut) const {
  QVector<DeviceData> devices;
  QHash<QString, int> deviceCounts;

  for (const QJsonValue &entry : root) {
    if (!entry.isObject()) {
      continue;
    }

    const QJsonObject object = entry.toObject();
    if (object.contains(QLatin1String("timestamp")) && !object.contains(QLatin1String("name")) &&
        !object.contains(QLatin1String("sensors"))) {
      if (timestampOut != nullptr) {
        *timestampOut = object.value(QLatin1String("timestamp")).toVariant().toLongLong();
      }
      continue;
    }

    DeviceData device;
    device.name = object.value(QLatin1String("name")).toString();
    device.type = object.value(QLatin1String("type")).toInt(4);
    const QString identity = QString::number(device.type) + QLatin1Char('|') + device.name;
    const int occurrence = deviceCounts[identity]++;
    device.key = QStringLiteral("%1:%2#%3").arg(device.type).arg(device.name).arg(occurrence);

    QHash<QString, int> sensorCounts;
    const QJsonValue sensorsValue = object.value(QLatin1String("sensors"));
    if (sensorsValue.isObject()) {
      device.sections = parseCustomSections(device.key, sensorCounts, sensorsValue.toObject());
    } else if (sensorsValue.isArray()) {
      device.sections = groupSensors(
          device.key, parseSensorArray(device.key, sensorCounts, sensorsValue.toArray()));
    }
    devices.push_back(std::move(device));
  }

  return devices;
}

SectionData MonitorModel::makeSection(const QString &key, const QString &name, int type,
                                      bool custom, QVector<SensorData> sensors) const {
  SectionData section;
  section.key = key;
  section.name = name;
  section.type = type;
  section.custom = custom;
  section.expanded = !m_collapsedKeys.contains(key);
  section.sensors = std::move(sensors);
  sortSensorsByName(section.sensors);
  return section;
}

QVector<SectionData> MonitorModel::groupSensors(const QString &deviceKey,
                                                QVector<SensorData> sensors) const {
  QList<int> order;
  QHash<int, QVector<SensorData>> buckets;
  for (SensorData &sensor : sensors) {
    if (!buckets.contains(sensor.type)) {
      order.push_back(sensor.type);
    }
    buckets[sensor.type].push_back(std::move(sensor));
  }

  QVector<SectionData> sections;
  sections.reserve(order.size());
  for (int type : order) {
    sections.push_back(makeSection(deviceKey + QStringLiteral("/sec/") + QString::number(type),
                                   sensorSectionName(type), type, false, std::move(buckets[type])));
  }
  return sections;
}

QVector<SectionData> MonitorModel::parseCustomSections(const QString &deviceKey,
                                                       QHash<QString, int> &sensorCounts,
                                                       const QJsonObject &sectionsObject) const {
  QVector<SectionData> sections;
  sections.reserve(sectionsObject.size());
  for (auto it = sectionsObject.begin(); it != sectionsObject.end(); ++it) {
    if (!it.value().isArray()) {
      continue;
    }
    QVector<SensorData> sensors = parseSensorArray(deviceKey, sensorCounts, it.value().toArray());
    if (sensors.isEmpty()) {
      continue;
    }
    const int type = sensors.front().type;
    sections.push_back(makeSection(deviceKey + QStringLiteral("/sec/") + it.key(), it.key(), type,
                                   true, std::move(sensors)));
  }
  return sections;
}

QVector<SectionData> MonitorModel::reorderSections(QVector<SectionData> sections,
                                                   const QStringList &order) const {
  // Section order entries may be a key, a display name or (legacy) a numeric type.
  const auto matches = [](const SectionData &section, const QString &item) {
    if (section.key == item || section.name == item) {
      return true;
    }
    bool ok = false;
    const int type = item.toInt(&ok);
    return ok && !section.custom && section.type == type;
  };

  QVector<SectionData> ordered;
  QVector<bool> used(sections.size(), false);
  ordered.reserve(sections.size());
  for (const QString &item : order) {
    for (int i = 0; i < sections.size(); ++i) {
      if (!used[i] && matches(sections[i], item)) {
        ordered.push_back(std::move(sections[i]));
        used[i] = true;
        break;
      }
    }
  }
  for (int i = 0; i < sections.size(); ++i) {
    if (!used[i]) {
      ordered.push_back(std::move(sections[i]));
    }
  }
  return ordered;
}

bool MonitorModel::moveDevice(int from, int to) {
  if (from < 0 || from >= m_devices.size()) {
    return false;
  }
  to = qMin(to, static_cast<int>(m_devices.size()));
  if (to < 0 || to == from || to == from + 1) {
    return false;
  }
  if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), to)) {
    return false;
  }
  DeviceData device = m_devices.takeAt(from);
  if (from < to) {
    --to;
  }
  m_devices.insert(to, std::move(device));
  endMoveRows();
  saveSettings();
  return true;
}

bool MonitorModel::moveSection(int deviceIndex, int fromVisible, int toVisible) {
  if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
    return false;
  }
  const int visible = visibleSectionCount(deviceIndex);
  if (fromVisible < 0 || fromVisible >= visible) {
    return false;
  }
  toVisible = qMin(toVisible, visible);
  if (toVisible < 0 || toVisible == fromVisible || toVisible == fromVisible + 1) {
    return false;
  }
  const QModelIndex parent = index(deviceIndex, 0);
  if (!beginMoveRows(parent, fromVisible, fromVisible, parent, toVisible)) {
    return false;
  }
  auto &sections = m_devices[deviceIndex].sections;
  const int fromSection = visibleToSection(deviceIndex, fromVisible);
  int toSection = toVisible >= visible ? sections.size() : visibleToSection(deviceIndex, toVisible);
  SectionData section = sections.takeAt(fromSection);
  if (fromSection < toSection) {
    --toSection;
  }
  toSection = qBound(0, toSection, static_cast<int>(sections.size()));
  sections.insert(toSection, std::move(section));
  endMoveRows();
  saveSettings();
  return true;
}

bool MonitorModel::moveSensor(int deviceIndex, int sectionIndex, int fromVisible, int toVisible) {
  const int visible = visibleSensorCount(deviceIndex, sectionIndex);
  if (fromVisible < 0 || fromVisible >= visible) {
    return false;
  }
  toVisible = qMin(toVisible, visible);
  if (toVisible < 0 || toVisible == fromVisible || toVisible == fromVisible + 1) {
    return false;
  }
  const int sectionVisible = sectionToVisible(deviceIndex, sectionIndex);
  const QModelIndex parent = index(sectionVisible, 0, index(deviceIndex, 0));
  if (!beginMoveRows(parent, fromVisible, fromVisible, parent, toVisible)) {
    return false;
  }
  auto &sensors = m_devices[deviceIndex].sections[sectionIndex].sensors;
  const int fromSensor = visibleToSensor(deviceIndex, sectionIndex, fromVisible);
  int toSensor =
      toVisible >= visible ? sensors.size() : visibleToSensor(deviceIndex, sectionIndex, toVisible);
  SensorData sensor = sensors.takeAt(fromSensor);
  if (fromSensor < toSensor) {
    --toSensor;
  }
  toSensor = qBound(0, toSensor, static_cast<int>(sensors.size()));
  sensors.insert(toSensor, std::move(sensor));
  endMoveRows();
  saveSettings();
  return true;
}
