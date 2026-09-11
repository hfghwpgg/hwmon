#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

class QJsonArray;
class QJsonDocument;
class QJsonObject;

struct SensorData {
    QString key;
    QString name;
    int type = 10;
    double value = 0.0;
    double min = 0.0;
    double max = 0.0;
    double sum = 0.0;
    qint64 times = 0;
    bool hidden = false;
    bool primary = false;
    qint64 flashCurrentUntil = 0;
    qint64 flashMinUntil = 0;
    qint64 flashMaxUntil = 0;
};

struct SectionData {
    QString key;
    QString name;
    int type = 10;
    bool custom = false;
    bool expanded = true;
    QVector<SensorData> sensors;
};

struct DeviceData {
    QString key;
    QString name;
    int type = 4;
    bool expanded = true;
    QVector<SectionData> sections;
};

class MonitorModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column { NameColumn, CurrentColumn, MinColumn, MaxColumn, AvgColumn, ColumnCount };

    explicit MonitorModel(QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                         const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;

    bool applySnapshot(const QJsonDocument &document, qint64 *timestampOut);
    void resetLocalReadings();
    void hideSensor(const QModelIndex &index);
    void showHiddenSensors(const QModelIndex &index);
    bool hasHiddenSensors(const QModelIndex &index) const;
    bool isDevice(const QModelIndex &index) const;
    bool isSection(const QModelIndex &index) const;
    bool isSensor(const QModelIndex &index) const;
    void setNodeExpanded(const QModelIndex &index, bool expanded);
    bool isNodeExpanded(const QModelIndex &index) const;
    void setDarkTheme(bool dark);
    void setFlashDurationMs(int ms);
    void resetSensorOrder();

private:
    int visibleSectionCount(int deviceIndex) const;
    int visibleToSection(int deviceIndex, int visibleRow) const;
    int sectionToVisible(int deviceIndex, int sectionIndex) const;
    int visibleSensorCount(int deviceIndex, int sectionIndex) const;
    int visibleToSensor(int deviceIndex, int sectionIndex, int visibleRow) const;
    const DeviceData *deviceFromIndex(const QModelIndex &index) const;
    DeviceData *deviceFromIndex(const QModelIndex &index);
    const SectionData *sectionFromIndex(const QModelIndex &index) const;
    SectionData *sectionFromIndex(const QModelIndex &index);
    const SensorData *sensorFromIndex(const QModelIndex &index) const;
    SensorData *sensorFromIndex(const QModelIndex &index);
    void loadSettings();
    void saveSettings() const;
    void notifyValues();
    bool structureEquals(const QVector<DeviceData> &other) const;
    QVector<DeviceData> parseDevices(const QJsonArray &root, qint64 *timestampOut) const;
    QVector<SectionData> groupSensors(const QString &deviceKey, QVector<SensorData> sensors,
                                      const QList<int> &typeOrder) const;
    QVector<SectionData> reorderSections(QVector<SectionData> sections,
                                         const QStringList &order) const;
    SensorData parseSensor(const QString &deviceKey, QHash<QString, int> &sensorCounts,
                           const QJsonObject &sensorObject) const;
    QVector<SensorData> parseSensorArray(const QString &deviceKey, QHash<QString, int> &sensorCounts,
                                         const QJsonArray &sensorArray) const;
    QVector<SectionData> parseCustomSections(const QString &deviceKey,
                                             QHash<QString, int> &sensorCounts,
                                             const QJsonObject &sectionsObject) const;
    QVector<SensorData> flattenSensors(const DeviceData &device) const;
    QVector<SensorData> mergeSensors(const QVector<SensorData> &oldSensors,
                                     const QVector<SensorData> &incoming) const;
    QVector<DeviceData> reorderDevices(QVector<DeviceData> devices, const QStringList &order) const;
    QVector<SensorData> reorderSensors(QVector<SensorData> sensors, const QStringList &order) const;
    void sortSensorsByName(QVector<SensorData> &sensors) const;
    bool moveDevice(int from, int to);
    bool moveSection(int deviceIndex, int fromVisible, int toVisible);
    bool moveSensor(int deviceIndex, int sectionIndex, int fromVisible, int toVisible);

    QVector<DeviceData> m_devices;
    QSet<QString> m_collapsedKeys;
    QSet<QString> m_hiddenKeys;
    QStringList m_deviceOrder;
    QHash<QString, QStringList> m_sensorOrder;
    QHash<QString, QStringList> m_sectionOrder;
    bool m_initialized = false;
    bool m_darkTheme = true;
    int m_flashDurationMs = 1000;
};
