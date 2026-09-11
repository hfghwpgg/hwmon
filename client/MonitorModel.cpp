#include "MonitorModel.hpp"

#include "Types.hpp"

#include <QCollator>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr auto kMimeType = "application/x-hwmon-item";
constexpr quintptr kSectionMask = 0xFFFF;

quintptr makeSectionId(int deviceIndex)
{
    return quintptr(deviceIndex + 1) << 16;
}

quintptr makeSensorId(int deviceIndex, int sectionIndex)
{
    return (quintptr(deviceIndex + 1) << 16) | quintptr(sectionIndex + 1);
}

int deviceIndexFromId(quintptr id)
{
    return int(id >> 16) - 1;
}

int sectionIndexFromId(quintptr id)
{
    return int(id & kSectionMask) - 1;
}

QString formatBytes(double bytes)
{
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    int unit = 0;
    while (bytes >= 1024.0 && unit < 4) {
        bytes /= 1024.0;
        ++unit;
    }
    const int precision = unit == 0 ? 0 : 2;
    return QString::number(bytes, 'f', precision) + QLatin1Char(' ') + QLatin1String(units[unit]);
}

QString formatValue(int type, double value)
{
    if (!std::isfinite(value)) {
        return QStringLiteral("—");
    }

    switch (type) {
    case 0:
        return QString::number(value, 'f', 1) + QStringLiteral(" °C");
    case 1:
        return QString::number(value, 'f', 0) + QStringLiteral(" RPM");
    case 2:
        return QString::number(value, 'f', 1) + QStringLiteral(" MHz");
    case 3:
        return QString::number(value, 'f', 1) + QStringLiteral(" W");
    case 4:
        return QString::number(value, 'f', 3) + QStringLiteral(" V");
    case 5:
        return QString::number(value, 'f', 3) + QStringLiteral(" A");
    case 6:
        return QString::number(value, 'f', 3) + QStringLiteral(" W");
    case 7:
        return QString::number(value, 'f', 1) + QStringLiteral(" %");
    case 8:
        return formatBytes(value);
    case 9:
        return formatBytes(value) + QStringLiteral("/s");
    default:
        return QString::number(value, 'g', 4);
    }
}

QString formatAverage(const SensorData &sensor)
{
    if (sensor.times <= 0) {
        return QStringLiteral("—");
    }
    return formatValue(sensor.type, sensor.sum / static_cast<double>(sensor.times));
}

QSet<QString> toSet(const QStringList &list)
{
    return QSet<QString>(list.cbegin(), list.cend());
}

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

struct SectionSummary {
    bool valid = false;
    int type = 10;
    double current = 0.0;
    double min = 0.0;
    double max = 0.0;
    double avg = 0.0;
    bool hasAvg = false;
    bool flashCurrent = false;
    bool flashMin = false;
    bool flashMax = false;
};

SectionSummary primaryForSection(const SectionData &section)
{
    SectionSummary summary;
    const qint64 now = nowMs();
    for (const SensorData &sensor : section.sensors) {
        if (!sensor.primary || sensor.hidden) {
            continue;
        }
        summary.valid = true;
        summary.type = sensor.type;
        summary.current = sensor.value;
        summary.min = sensor.min;
        summary.max = sensor.max;
        if (sensor.times > 0) {
            summary.avg = sensor.sum / static_cast<double>(sensor.times);
            summary.hasAvg = true;
        }
        summary.flashCurrent = sensor.flashCurrentUntil > now;
        summary.flashMin = sensor.flashMinUntil > now;
        summary.flashMax = sensor.flashMaxUntil > now;
        break;
    }
    return summary;
}

} // namespace

MonitorModel::MonitorModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    loadSettings();
}

QModelIndex MonitorModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column < 0 || column >= ColumnCount) {
        return {};
    }
    if (!parent.isValid()) {
        if (row >= m_devices.size()) {
            return {};
        }
        return createIndex(row, column, quintptr(0));
    }
    if (isDevice(parent)) {
        if (row >= visibleSectionCount(parent.row())) {
            return {};
        }
        return createIndex(row, column, makeSectionId(parent.row()));
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

QModelIndex MonitorModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || isDevice(child)) {
        return {};
    }
    if (isSection(child)) {
        return createIndex(deviceIndexFromId(child.internalId()), 0, quintptr(0));
    }
    const int deviceIndex = deviceIndexFromId(child.internalId());
    const int sectionIndex = sectionIndexFromId(child.internalId());
    const int visible = sectionToVisible(deviceIndex, sectionIndex);
    if (visible < 0) {
        return {};
    }
    return createIndex(visible, 0, makeSectionId(deviceIndex));
}

int MonitorModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_devices.size();
    }
    if (isDevice(parent)) {
        return visibleSectionCount(parent.row());
    }
    if (isSection(parent)) {
        const int deviceIndex = deviceIndexFromId(parent.internalId());
        const int sectionIndex = visibleToSection(deviceIndex, parent.row());
        return visibleSensorCount(deviceIndex, sectionIndex);
    }
    return 0;
}

int MonitorModel::columnCount(const QModelIndex &) const
{
    return ColumnCount;
}

QVariant MonitorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const DeviceData *device = deviceFromIndex(index);
    const SectionData *section = isSection(index) ? sectionFromIndex(index) : nullptr;
    const SensorData *sensor = isSensor(index) ? sensorFromIndex(index) : nullptr;
    if (device == nullptr) {
        return {};
    }
    if (isSection(index) && section == nullptr) {
        return {};
    }
    if (isSensor(index) && sensor == nullptr) {
        return {};
    }

    const SectionSummary summary = section != nullptr ? primaryForSection(*section) : SectionSummary{};
    const qint64 now = nowMs();

    if (role == Qt::DisplayRole) {
        if (index.column() == NameColumn) {
            if (isDevice(index)) {
                return QStringLiteral("[%1]  %2").arg(deviceTypeLabel(device->type), device->name);
            }
            if (isSection(index)) {
                if (summary.valid) {
                    return QStringLiteral("[%1]  %2").arg(sensorTypeLabel(summary.type), section->name);
                }
                return section->name;
            }
            return QStringLiteral("[%1]  %2").arg(sensorTypeLabel(sensor->type), sensor->name);
        }
        if (isDevice(index)) {
            return {};
        }
        if (isSection(index)) {
            if (!summary.valid) {
                return {};
            }
            switch (index.column()) {
            case CurrentColumn:
                return formatValue(summary.type, summary.current);
            case MinColumn:
                return formatValue(summary.type, summary.min);
            case MaxColumn:
                return formatValue(summary.type, summary.max);
            case AvgColumn:
                return summary.hasAvg ? formatValue(summary.type, summary.avg) : QStringLiteral("—");
            default:
                return {};
            }
        }
        switch (index.column()) {
        case CurrentColumn:
            return formatValue(sensor->type, sensor->value);
        case MinColumn:
            return formatValue(sensor->type, sensor->min);
        case MaxColumn:
            return formatValue(sensor->type, sensor->max);
        case AvgColumn:
            return formatAverage(*sensor);
        default:
            return {};
        }
    }

    if (role == Qt::FontRole) {
        QFont font;
        if (isDevice(index) || isSection(index)) {
            font.setBold(true);
        } else if (index.column() != NameColumn) {
            font.setFamilies({QStringLiteral("monospace"), QStringLiteral("Noto Sans Mono")});
        }
        return font;
    }

    if (role == Qt::TextAlignmentRole && index.column() != NameColumn) {
        return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole) {
        if (index.column() == NameColumn) {
            return m_darkTheme ? QColor("#6cb6ff") : QColor("#1565c0");
        }
        return m_darkTheme ? QColor("#ececec") : QColor("#1a1a1a");
    }

    if (role == Qt::BackgroundRole) {
        if (isDevice(index)) {
            return m_darkTheme ? QColor("#243140") : QColor("#d7e4f2");
        }
        if (isSection(index)) {
            const QColor base = m_darkTheme ? QColor("#1e2a36") : QColor("#e4ecf4");
            const QColor flash = m_darkTheme ? QColor(108, 182, 255, 26) : QColor(21, 101, 192, 13);
            if (summary.valid) {
                if (index.column() == CurrentColumn && summary.flashCurrent) {
                    return flash;
                }
                if (index.column() == MinColumn && summary.flashMin) {
                    return flash;
                }
                if (index.column() == MaxColumn && summary.flashMax) {
                    return flash;
                }
            }
            return base;
        }
        if (sensor != nullptr) {
            const QColor flash = m_darkTheme ? QColor(108, 182, 255, 26) : QColor(21, 101, 192, 13);
            if (index.column() == CurrentColumn && sensor->flashCurrentUntil > now) {
                return flash;
            }
            if (index.column() == MinColumn && sensor->flashMinUntil > now) {
                return flash;
            }
            if (index.column() == MaxColumn && sensor->flashMaxUntil > now) {
                return flash;
            }
        }
        return {};
    }

    return {};
}

QVariant MonitorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
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

Qt::ItemFlags MonitorModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsDropEnabled;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

Qt::DropActions MonitorModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList MonitorModel::mimeTypes() const
{
    return {QString::fromLatin1(kMimeType)};
}

QMimeData *MonitorModel::mimeData(const QModelIndexList &indexes) const
{
    if (indexes.isEmpty()) {
        return nullptr;
    }
    const QModelIndex index = indexes.front().siblingAtColumn(0);
    auto *mime = new QMimeData;
    if (isDevice(index)) {
        mime->setData(QString::fromLatin1(kMimeType),
                      QByteArray("D\n") + deviceFromIndex(index)->key.toUtf8());
    } else if (isSection(index)) {
        const DeviceData *device = deviceFromIndex(index);
        const SectionData *section = sectionFromIndex(index);
        mime->setData(QString::fromLatin1(kMimeType),
                      QByteArray("G\n") + device->key.toUtf8() + '\n' + section->key.toUtf8());
    } else {
        const DeviceData *device = deviceFromIndex(index);
        const SectionData *section = sectionFromIndex(index);
        const SensorData *sensor = sensorFromIndex(index);
        mime->setData(QString::fromLatin1(kMimeType),
                      QByteArray("S\n") + device->key.toUtf8() + '\n' + section->key.toUtf8() + '\n'
                          + sensor->key.toUtf8());
    }
    return mime;
}

bool MonitorModel::canDropMimeData(const QMimeData *data, Qt::DropAction, int, int,
                                   const QModelIndex &parent) const
{
    if (data == nullptr || !data->hasFormat(QString::fromLatin1(kMimeType))) {
        return false;
    }
    const QByteArray raw = data->data(QString::fromLatin1(kMimeType));
    if (raw.startsWith("D\n")) {
        return true;
    }
    const QList<QByteArray> parts = raw.split('\n');
    if (raw.startsWith("G\n") && parts.size() >= 3) {
        const QString deviceKey = QString::fromUtf8(parts.at(1));
        const DeviceData *device = nullptr;
        if (!parent.isValid()) {
            return false;
        }
        device = deviceFromIndex(isDevice(parent) ? parent : parent.parent().isValid() ? parent.parent() : parent);
        if (isSensor(parent)) {
            device = deviceFromIndex(parent.parent().parent());
        } else if (isSection(parent)) {
            device = deviceFromIndex(parent.parent());
        }
        return device != nullptr && device->key == deviceKey;
    }
    if (raw.startsWith("S\n") && parts.size() >= 4) {
        const QString deviceKey = QString::fromUtf8(parts.at(1));
        const QString sectionKey = QString::fromUtf8(parts.at(2));
        const SectionData *section = sectionFromIndex(isSensor(parent) ? parent.parent() : parent);
        const DeviceData *device = deviceFromIndex(parent);
        return device != nullptr && section != nullptr && device->key == deviceKey && section->key == sectionKey;
    }
    return false;
}

bool MonitorModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int,
                                const QModelIndex &parent)
{
    if (action != Qt::MoveAction || !canDropMimeData(data, action, row, 0, parent)) {
        return false;
    }

    const QList<QByteArray> parts = data->data(QString::fromLatin1(kMimeType)).split('\n');
    if (parts.isEmpty()) {
        return false;
    }

    if (parts.front() == "D") {
        const QString key = QString::fromUtf8(parts.value(1));
        int from = -1;
        for (int i = 0; i < m_devices.size(); ++i) {
            if (m_devices[i].key == key) {
                from = i;
                break;
            }
        }
        int to = row;
        if (parent.isValid()) {
            if (isDevice(parent)) {
                to = parent.row();
            } else if (isSection(parent)) {
                to = parent.parent().row();
            } else {
                to = parent.parent().parent().row();
            }
        }
        if (to < 0) {
            to = m_devices.size();
        }
        return moveDevice(from, to);
    }

    if (parts.front() == "G") {
        const QString deviceKey = QString::fromUtf8(parts.value(1));
        const QString sectionKey = QString::fromUtf8(parts.value(2));
        int deviceIndex = -1;
        for (int i = 0; i < m_devices.size(); ++i) {
            if (m_devices[i].key == deviceKey) {
                deviceIndex = i;
                break;
            }
        }
        if (deviceIndex < 0) {
            return false;
        }
        int fromVisible = -1;
        const int visible = visibleSectionCount(deviceIndex);
        for (int i = 0; i < visible; ++i) {
            const int sectionIndex = visibleToSection(deviceIndex, i);
            if (m_devices[deviceIndex].sections[sectionIndex].key == sectionKey) {
                fromVisible = i;
                break;
            }
        }
        int toVisible = row;
        if (isDevice(parent)) {
            toVisible = row < 0 ? visible : row;
        } else if (isSection(parent)) {
            toVisible = parent.row();
        } else if (isSensor(parent)) {
            toVisible = parent.parent().row();
        }
        if (toVisible < 0) {
            toVisible = visible;
        }
        return moveSection(deviceIndex, fromVisible, toVisible);
    }

    const QString deviceKey = QString::fromUtf8(parts.value(1));
    const QString sectionKey = QString::fromUtf8(parts.value(2));
    const QString sensorKey = QString::fromUtf8(parts.value(3));
    int deviceIndex = -1;
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].key == deviceKey) {
            deviceIndex = i;
            break;
        }
    }
    if (deviceIndex < 0) {
        return false;
    }
    int sectionIndex = -1;
    for (int i = 0; i < m_devices[deviceIndex].sections.size(); ++i) {
        if (m_devices[deviceIndex].sections[i].key == sectionKey) {
            sectionIndex = i;
            break;
        }
    }
    if (sectionIndex < 0) {
        return false;
    }

    int fromVisible = -1;
    const int visible = visibleSensorCount(deviceIndex, sectionIndex);
    for (int i = 0; i < visible; ++i) {
        const int sensorIndex = visibleToSensor(deviceIndex, sectionIndex, i);
        if (m_devices[deviceIndex].sections[sectionIndex].sensors[sensorIndex].key == sensorKey) {
            fromVisible = i;
            break;
        }
    }
    int toVisible = row;
    if (isSection(parent)) {
        toVisible = row < 0 ? 0 : row;
    } else if (isSensor(parent)) {
        toVisible = parent.row();
    }
    if (toVisible < 0) {
        toVisible = visible;
    }
    return moveSensor(deviceIndex, sectionIndex, fromVisible, toVisible);
}

bool MonitorModel::applySnapshot(const QJsonDocument &document, qint64 *timestampOut)
{
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

    QVector<DeviceData> merged;
    merged.reserve(incoming.size());
    for (DeviceData &device : incoming) {
        if (existingIndex.contains(device.key)) {
            const DeviceData &old = m_devices[existingIndex.value(device.key)];
            device.expanded = old.expanded;
            QHash<QString, const SectionData *> oldSections;
            QStringList oldSectionOrder;
            oldSections.reserve(old.sections.size());
            oldSectionOrder.reserve(old.sections.size());
            for (const SectionData &section : old.sections) {
                oldSections.insert(section.key, &section);
                oldSectionOrder.push_back(section.key);
            }
            QHash<QString, SensorData> mergedByKey;
            const QVector<SensorData> mergedFlat =
                mergeSensors(flattenSensors(old), flattenSensors(device));
            mergedByKey.reserve(mergedFlat.size());
            for (const SensorData &sensor : mergedFlat) {
                mergedByKey.insert(sensor.key, sensor);
            }
            for (SectionData &section : device.sections) {
                const SectionData *previous = oldSections.value(section.key, nullptr);
                if (previous != nullptr) {
                    section.expanded = previous->expanded;
                }
                QVector<SensorData> incomingSensors;
                incomingSensors.reserve(section.sensors.size());
                for (const SensorData &sensor : section.sensors) {
                    incomingSensors.push_back(mergedByKey.value(sensor.key, sensor));
                }
                QStringList previousOrder;
                if (previous != nullptr) {
                    previousOrder.reserve(previous->sensors.size());
                    for (const SensorData &sensor : previous->sensors) {
                        previousOrder.push_back(sensor.key);
                    }
                }
                section.sensors = reorderSensors(std::move(incomingSensors), previousOrder);
            }
            device.sections = reorderSections(std::move(device.sections), oldSectionOrder);
        } else {
            device.expanded = !m_collapsedKeys.contains(device.key);
            for (SectionData &section : device.sections) {
                for (SensorData &sensor : section.sensors) {
                    sensor.hidden = m_hiddenKeys.contains(sensor.key);
                }
            }
            if (auto sectionOrder = m_sectionOrder.constFind(device.key);
                sectionOrder != m_sectionOrder.cend()) {
                device.sections = reorderSections(std::move(device.sections), sectionOrder.value());
            }
            if (auto order = m_sensorOrder.constFind(device.key);
                order != m_sensorOrder.cend() && !order->isEmpty()) {
                for (SectionData &section : device.sections) {
                    section.sensors = reorderSensors(std::move(section.sensors), order.value());
                }
            }
        }
        merged.push_back(std::move(device));
    }

    if (!m_initialized) {
        if (!m_deviceOrder.isEmpty()) {
            merged = reorderDevices(std::move(merged), m_deviceOrder);
        }
        m_initialized = true;
    } else if (!m_devices.isEmpty()) {
        QStringList currentOrder;
        currentOrder.reserve(m_devices.size());
        for (const DeviceData &device : m_devices) {
            currentOrder.push_back(device.key);
        }
        merged = reorderDevices(std::move(merged), currentOrder);
    }

    const bool reset = !structureEquals(merged);
    if (reset) {
        beginResetModel();
    }
    m_devices = std::move(merged);
    if (reset) {
        endResetModel();
    } else {
        notifyValues();
    }
    return true;
}

void MonitorModel::resetLocalReadings()
{
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

void MonitorModel::hideSensor(const QModelIndex &index)
{
    if (!isSensor(index)) {
        return;
    }
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

void MonitorModel::showHiddenSensors(const QModelIndex &index)
{
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
        SectionData *section = sectionFromIndex(index);
        if (section != nullptr) {
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

bool MonitorModel::hasHiddenSensors(const QModelIndex &index) const
{
    if (isSection(index)) {
        const SectionData *section = sectionFromIndex(index);
        if (section == nullptr) {
            return false;
        }
        for (const SensorData &sensor : section->sensors) {
            if (sensor.hidden) {
                return true;
            }
        }
        return false;
    }
    const DeviceData *device = deviceFromIndex(index);
    if (device == nullptr) {
        return false;
    }
    for (const SectionData &section : device->sections) {
        for (const SensorData &sensor : section.sensors) {
            if (sensor.hidden) {
                return true;
            }
        }
    }
    return false;
}

bool MonitorModel::isDevice(const QModelIndex &index) const
{
    return index.isValid() && index.internalId() == 0;
}

bool MonitorModel::isSection(const QModelIndex &index) const
{
    return index.isValid() && index.internalId() != 0 && (index.internalId() & kSectionMask) == 0;
}

bool MonitorModel::isSensor(const QModelIndex &index) const
{
    return index.isValid() && (index.internalId() & kSectionMask) != 0;
}

void MonitorModel::setNodeExpanded(const QModelIndex &index, bool expanded)
{
    if (isDevice(index)) {
        DeviceData *device = deviceFromIndex(index);
        if (device == nullptr) {
            return;
        }
        device->expanded = expanded;
        if (expanded) {
            m_collapsedKeys.remove(device->key);
        } else {
            m_collapsedKeys.insert(device->key);
        }
        saveSettings();
        return;
    }
    if (isSection(index)) {
        SectionData *section = sectionFromIndex(index);
        if (section == nullptr) {
            return;
        }
        section->expanded = expanded;
        if (expanded) {
            m_collapsedKeys.remove(section->key);
        } else {
            m_collapsedKeys.insert(section->key);
        }
        saveSettings();
    }
}

bool MonitorModel::isNodeExpanded(const QModelIndex &index) const
{
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

void MonitorModel::setDarkTheme(bool dark)
{
    if (m_darkTheme == dark) {
        return;
    }
    m_darkTheme = dark;
    notifyValues();
}

void MonitorModel::setFlashDurationMs(int ms)
{
    m_flashDurationMs = qMax(50, ms);
}

void MonitorModel::resetSensorOrder()
{
    beginResetModel();
    for (DeviceData &device : m_devices) {
        for (SectionData &section : device.sections) {
            sortSensorsByName(section.sensors);
        }
    }
    endResetModel();
    saveSettings();
}

int MonitorModel::visibleSectionCount(int deviceIndex) const
{
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < m_devices[deviceIndex].sections.size(); ++i) {
        if (visibleSensorCount(deviceIndex, i) > 0) {
            ++count;
        }
    }
    return count;
}

int MonitorModel::visibleToSection(int deviceIndex, int visibleRow) const
{
    int seen = 0;
    for (int i = 0; i < m_devices[deviceIndex].sections.size(); ++i) {
        if (visibleSensorCount(deviceIndex, i) == 0) {
            continue;
        }
        if (seen == visibleRow) {
            return i;
        }
        ++seen;
    }
    return -1;
}

int MonitorModel::sectionToVisible(int deviceIndex, int sectionIndex) const
{
    if (deviceIndex < 0 || deviceIndex >= m_devices.size() || sectionIndex < 0
        || sectionIndex >= m_devices[deviceIndex].sections.size()) {
        return -1;
    }
    if (visibleSensorCount(deviceIndex, sectionIndex) == 0) {
        return -1;
    }
    int visible = 0;
    for (int i = 0; i < sectionIndex; ++i) {
        if (visibleSensorCount(deviceIndex, i) > 0) {
            ++visible;
        }
    }
    return visible;
}

int MonitorModel::visibleSensorCount(int deviceIndex, int sectionIndex) const
{
    if (deviceIndex < 0 || deviceIndex >= m_devices.size() || sectionIndex < 0
        || sectionIndex >= m_devices[deviceIndex].sections.size()) {
        return 0;
    }
    int count = 0;
    for (const SensorData &sensor : m_devices[deviceIndex].sections[sectionIndex].sensors) {
        if (!sensor.hidden) {
            ++count;
        }
    }
    return count;
}

int MonitorModel::visibleToSensor(int deviceIndex, int sectionIndex, int visibleRow) const
{
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

const DeviceData *MonitorModel::deviceFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return nullptr;
    }
    const int deviceIndex = isDevice(index) ? index.row() : deviceIndexFromId(index.internalId());
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        return nullptr;
    }
    return &m_devices[deviceIndex];
}

DeviceData *MonitorModel::deviceFromIndex(const QModelIndex &index)
{
    return const_cast<DeviceData *>(std::as_const(*this).deviceFromIndex(index));
}

const SectionData *MonitorModel::sectionFromIndex(const QModelIndex &index) const
{
    if (isDevice(index)) {
        return nullptr;
    }
    const int deviceIndex = deviceIndexFromId(index.internalId());
    int sectionIndex = -1;
    if (isSection(index)) {
        sectionIndex = visibleToSection(deviceIndex, index.row());
    } else {
        sectionIndex = sectionIndexFromId(index.internalId());
    }
    if (deviceIndex < 0 || deviceIndex >= m_devices.size() || sectionIndex < 0
        || sectionIndex >= m_devices[deviceIndex].sections.size()) {
        return nullptr;
    }
    return &m_devices[deviceIndex].sections[sectionIndex];
}

SectionData *MonitorModel::sectionFromIndex(const QModelIndex &index)
{
    return const_cast<SectionData *>(std::as_const(*this).sectionFromIndex(index));
}

const SensorData *MonitorModel::sensorFromIndex(const QModelIndex &index) const
{
    if (!isSensor(index)) {
        return nullptr;
    }
    const int deviceIndex = deviceIndexFromId(index.internalId());
    const int sectionIndex = sectionIndexFromId(index.internalId());
    const int sensorIndex = visibleToSensor(deviceIndex, sectionIndex, index.row());
    if (sensorIndex < 0) {
        return nullptr;
    }
    return &m_devices[deviceIndex].sections[sectionIndex].sensors[sensorIndex];
}

SensorData *MonitorModel::sensorFromIndex(const QModelIndex &index)
{
    return const_cast<SensorData *>(std::as_const(*this).sensorFromIndex(index));
}

void MonitorModel::loadSettings()
{
    QSettings settings;
    m_collapsedKeys = toSet(settings.value(QStringLiteral("collapsed")).toStringList());
    m_hiddenKeys = toSet(settings.value(QStringLiteral("hidden")).toStringList());
    m_deviceOrder = settings.value(QStringLiteral("deviceOrder")).toStringList();

    const int sensorCount = settings.beginReadArray(QStringLiteral("sensorOrders"));
    for (int i = 0; i < sensorCount; ++i) {
        settings.setArrayIndex(i);
        const QString key = settings.value(QStringLiteral("device")).toString();
        const QStringList order = settings.value(QStringLiteral("order")).toStringList();
        if (!key.isEmpty()) {
            m_sensorOrder.insert(key, order);
        }
    }
    settings.endArray();

    const int sectionCount = settings.beginReadArray(QStringLiteral("sectionOrders"));
    for (int i = 0; i < sectionCount; ++i) {
        settings.setArrayIndex(i);
        const QString key = settings.value(QStringLiteral("device")).toString();
        const QStringList order = settings.value(QStringLiteral("order")).toStringList();
        if (!key.isEmpty()) {
            m_sectionOrder.insert(key, order);
        }
    }
    settings.endArray();
}

void MonitorModel::saveSettings() const
{
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

    settings.beginWriteArray(QStringLiteral("sensorOrders"), m_devices.size());
    for (int i = 0; i < m_devices.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("device"), m_devices[i].key);
        QStringList order;
        for (const SectionData &section : m_devices[i].sections) {
            for (const SensorData &sensor : section.sensors) {
                order.push_back(sensor.key);
            }
        }
        settings.setValue(QStringLiteral("order"), order);
    }
    settings.endArray();

    settings.beginWriteArray(QStringLiteral("sectionOrders"), m_devices.size());
    for (int i = 0; i < m_devices.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("device"), m_devices[i].key);
        QStringList order;
        for (const SectionData &section : m_devices[i].sections) {
            order.push_back(section.key);
        }
        settings.setValue(QStringLiteral("order"), order);
    }
    settings.endArray();
}

void MonitorModel::notifyValues()
{
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
        emit dataChanged(this->index(0, 0, device), this->index(sections - 1, ColumnCount - 1, device));
        for (int sectionVisible = 0; sectionVisible < sections; ++sectionVisible) {
            const QModelIndex section = this->index(sectionVisible, 0, device);
            const int sectionIndex = visibleToSection(deviceIndex, sectionVisible);
            const int sensors = visibleSensorCount(deviceIndex, sectionIndex);
            if (sensors == 0) {
                continue;
            }
            emit dataChanged(this->index(0, 0, section),
                             this->index(sensors - 1, ColumnCount - 1, section));
        }
    }
}

bool MonitorModel::structureEquals(const QVector<DeviceData> &other) const
{
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
            if (sa.key != sb.key || sa.name != sb.name || sa.custom != sb.custom
                || sa.sensors.size() != sb.sensors.size()) {
                return false;
            }
            for (int j = 0; j < sa.sensors.size(); ++j) {
                if (sa.sensors[j].key != sb.sensors[j].key || sa.sensors[j].hidden != sb.sensors[j].hidden) {
                    return false;
                }
            }
        }
    }
    return true;
}

QVector<DeviceData> MonitorModel::parseDevices(const QJsonArray &root, qint64 *timestampOut) const
{
    QVector<DeviceData> devices;
    QHash<QString, int> deviceCounts;

    for (const QJsonValue &entry : root) {
        if (!entry.isObject()) {
            continue;
        }

        const QJsonObject object = entry.toObject();
        if (object.contains(QLatin1String("timestamp")) && !object.contains(QLatin1String("name"))
            && !object.contains(QLatin1String("sensors"))) {
            if (timestampOut != nullptr) {
                *timestampOut = object.value(QLatin1String("timestamp")).toVariant().toLongLong();
            }
            continue;
        }

        DeviceData device;
        device.name = object.value(QLatin1String("name")).toString();
        device.type = object.value(QLatin1String("type")).toInt(4);
        const QString deviceIdentity = QString::number(device.type) + QLatin1Char('|') + device.name;
        const int occurrence = deviceCounts[deviceIdentity]++;
        device.key = QStringLiteral("%1:%2#%3").arg(device.type).arg(device.name).arg(occurrence);

        QHash<QString, int> sensorCounts;
        const QJsonValue sensorsValue = object.value(QLatin1String("sensors"));
        if (sensorsValue.isObject()) {
            device.sections = parseCustomSections(device.key, sensorCounts, sensorsValue.toObject());
        } else if (sensorsValue.isArray()) {
            device.sections =
                groupSensors(device.key, parseSensorArray(device.key, sensorCounts, sensorsValue.toArray()),
                             {});
        }
        devices.push_back(std::move(device));
    }

    return devices;
}

QVector<SectionData> MonitorModel::groupSensors(const QString &deviceKey, QVector<SensorData> sensors,
                                                const QList<int> &typeOrder) const
{
    QList<int> order = typeOrder;
    QHash<int, QVector<SensorData>> buckets;
    for (SensorData &sensor : sensors) {
        if (!buckets.contains(sensor.type) && !order.contains(sensor.type)) {
            order.push_back(sensor.type);
        }
        buckets[sensor.type].push_back(std::move(sensor));
    }

    QVector<SectionData> sections;
    for (int type : order) {
        auto it = buckets.find(type);
        if (it == buckets.end() || it->isEmpty()) {
            continue;
        }
        SectionData section;
        section.type = type;
        section.name = sensorSectionName(type);
        section.custom = false;
        section.key = deviceKey + QStringLiteral("/sec/") + QString::number(type);
        section.expanded = !m_collapsedKeys.contains(section.key);
        section.sensors = std::move(it.value());
        sortSensorsByName(section.sensors);
        sections.push_back(std::move(section));
    }
    return sections;
}

QVector<SectionData> MonitorModel::reorderSections(QVector<SectionData> sections,
                                                   const QStringList &order) const
{
    auto matches = [](const SectionData &section, const QString &item) {
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
            if (used[i] || !matches(sections[i], item)) {
                continue;
            }
            ordered.push_back(std::move(sections[i]));
            used[i] = true;
            break;
        }
    }
    for (int i = 0; i < sections.size(); ++i) {
        if (!used[i]) {
            ordered.push_back(std::move(sections[i]));
        }
    }
    return ordered;
}

SensorData MonitorModel::parseSensor(const QString &deviceKey, QHash<QString, int> &sensorCounts,
                                     const QJsonObject &sensorObject) const
{
    SensorData sensor;
    sensor.name = sensorObject.value(QLatin1String("name")).toString();
    sensor.type = sensorObject.value(QLatin1String("type")).toInt(10);
    const QString sensorIdentity = QString::number(sensor.type) + QLatin1Char('|') + sensor.name;
    const int sensorOccurrence = sensorCounts[sensorIdentity]++;
    sensor.key = QStringLiteral("%1/%2:%3#%4")
                     .arg(deviceKey)
                     .arg(sensor.type)
                     .arg(sensor.name)
                     .arg(sensorOccurrence);

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

QVector<SensorData> MonitorModel::parseSensorArray(const QString &deviceKey,
                                                   QHash<QString, int> &sensorCounts,
                                                   const QJsonArray &sensorArray) const
{
    QVector<SensorData> sensors;
    sensors.reserve(sensorArray.size());
    for (const QJsonValue &sensorValue : sensorArray) {
        if (sensorValue.isObject()) {
            sensors.push_back(parseSensor(deviceKey, sensorCounts, sensorValue.toObject()));
        }
    }
    return sensors;
}

QVector<SectionData> MonitorModel::parseCustomSections(const QString &deviceKey,
                                                       QHash<QString, int> &sensorCounts,
                                                       const QJsonObject &sectionsObject) const
{
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
        SectionData section;
        section.name = it.key();
        section.custom = true;
        section.type = sensors.front().type;
        section.key = deviceKey + QStringLiteral("/sec/") + it.key();
        section.expanded = !m_collapsedKeys.contains(section.key);
        section.sensors = std::move(sensors);
        sortSensorsByName(section.sensors);
        sections.push_back(std::move(section));
    }
    return sections;
}

QVector<SensorData> MonitorModel::flattenSensors(const DeviceData &device) const
{
    QVector<SensorData> sensors;
    for (const SectionData &section : device.sections) {
        for (const SensorData &sensor : section.sensors) {
            sensors.push_back(sensor);
        }
    }
    return sensors;
}

QVector<SensorData> MonitorModel::mergeSensors(const QVector<SensorData> &oldSensors,
                                               const QVector<SensorData> &incoming) const
{
    QHash<QString, SensorData> incomingByKey;
    incomingByKey.reserve(incoming.size());
    for (const SensorData &sensor : incoming) {
        incomingByKey.insert(sensor.key, sensor);
    }

    QVector<SensorData> merged;
    QSet<QString> seen;
    merged.reserve(incoming.size());
    const qint64 until = nowMs() + m_flashDurationMs;

    for (const SensorData &old : oldSensors) {
        auto it = incomingByKey.constFind(old.key);
        if (it == incomingByKey.cend()) {
            continue;
        }
        SensorData next = it.value();
        next.hidden = old.hidden;
        next.flashCurrentUntil = next.value != old.value ? until : old.flashCurrentUntil;
        next.flashMinUntil = next.min != old.min ? until : old.flashMinUntil;
        next.flashMaxUntil = next.max != old.max ? until : old.flashMaxUntil;
        merged.push_back(std::move(next));
        seen.insert(old.key);
    }

    for (const SensorData &sensor : incoming) {
        if (seen.contains(sensor.key)) {
            continue;
        }
        SensorData next = sensor;
        next.hidden = m_hiddenKeys.contains(sensor.key);
        merged.push_back(std::move(next));
    }
    return merged;
}

QVector<DeviceData> MonitorModel::reorderDevices(QVector<DeviceData> devices, const QStringList &order) const
{
    QHash<QString, int> indexByKey;
    for (int i = 0; i < devices.size(); ++i) {
        indexByKey.insert(devices[i].key, i);
    }

    QVector<DeviceData> ordered;
    QSet<int> used;
    ordered.reserve(devices.size());
    for (const QString &key : order) {
        const auto it = indexByKey.constFind(key);
        if (it == indexByKey.cend() || used.contains(it.value())) {
            continue;
        }
        ordered.push_back(std::move(devices[it.value()]));
        used.insert(it.value());
    }
    for (int i = 0; i < devices.size(); ++i) {
        if (!used.contains(i)) {
            ordered.push_back(std::move(devices[i]));
        }
    }
    return ordered;
}

QVector<SensorData> MonitorModel::reorderSensors(QVector<SensorData> sensors, const QStringList &order) const
{
    QHash<QString, int> indexByKey;
    for (int i = 0; i < sensors.size(); ++i) {
        indexByKey.insert(sensors[i].key, i);
    }

    QVector<SensorData> ordered;
    QSet<int> used;
    ordered.reserve(sensors.size());
    for (const QString &key : order) {
        const auto it = indexByKey.constFind(key);
        if (it == indexByKey.cend() || used.contains(it.value())) {
            continue;
        }
        ordered.push_back(std::move(sensors[it.value()]));
        used.insert(it.value());
    }
    for (int i = 0; i < sensors.size(); ++i) {
        if (!used.contains(i)) {
            ordered.push_back(std::move(sensors[i]));
        }
    }
    return ordered;
}

void MonitorModel::sortSensorsByName(QVector<SensorData> &sensors) const
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(sensors.begin(), sensors.end(), [&](const SensorData &left, const SensorData &right) {
        const int compared = collator.compare(left.name, right.name);
        if (compared != 0) {
            return compared < 0;
        }
        return left.key < right.key;
    });
}

bool MonitorModel::moveDevice(int from, int to)
{
    if (from < 0 || from >= m_devices.size()) {
        return false;
    }
    if (to > m_devices.size()) {
        to = m_devices.size();
    }
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

bool MonitorModel::moveSection(int deviceIndex, int fromVisible, int toVisible)
{
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        return false;
    }
    const int visible = visibleSectionCount(deviceIndex);
    if (fromVisible < 0 || fromVisible >= visible) {
        return false;
    }
    if (toVisible > visible) {
        toVisible = visible;
    }
    if (toVisible < 0 || toVisible == fromVisible || toVisible == fromVisible + 1) {
        return false;
    }
    const QModelIndex parent = index(deviceIndex, 0);
    if (!beginMoveRows(parent, fromVisible, fromVisible, parent, toVisible)) {
        return false;
    }
    const int fromSection = visibleToSection(deviceIndex, fromVisible);
    int toSection = toVisible >= visible ? m_devices[deviceIndex].sections.size()
                                         : visibleToSection(deviceIndex, toVisible);
    SectionData section = m_devices[deviceIndex].sections.takeAt(fromSection);
    if (fromSection < toSection) {
        --toSection;
    }
    toSection = qBound(0, toSection, static_cast<int>(m_devices[deviceIndex].sections.size()));
    m_devices[deviceIndex].sections.insert(toSection, std::move(section));
    endMoveRows();
    saveSettings();
    return true;
}

bool MonitorModel::moveSensor(int deviceIndex, int sectionIndex, int fromVisible, int toVisible)
{
    if (deviceIndex < 0 || deviceIndex >= m_devices.size() || sectionIndex < 0
        || sectionIndex >= m_devices[deviceIndex].sections.size()) {
        return false;
    }
    const int visible = visibleSensorCount(deviceIndex, sectionIndex);
    if (fromVisible < 0 || fromVisible >= visible) {
        return false;
    }
    if (toVisible > visible) {
        toVisible = visible;
    }
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
    int toSensor = toVisible >= visible ? sensors.size() : visibleToSensor(deviceIndex, sectionIndex, toVisible);
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
