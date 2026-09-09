#pragma once

#include <QString>

inline QString deviceTypeLabel(int type)
{
    switch (type) {
    case 0:
        return QStringLiteral("CPU");
    case 1:
        return QStringLiteral("GPU");
    case 2:
        return QStringLiteral("RAM");
    case 3:
        return QStringLiteral("STOR");
    default:
        return QStringLiteral("DEV");
    }
}

inline QString sensorTypeLabel(int type)
{
    switch (type) {
    case 0:
        return QStringLiteral("TEMP");
    case 1:
        return QStringLiteral("FAN");
    case 2:
        return QStringLiteral("CLK");
    case 3:
        return QStringLiteral("PWR");
    case 4:
        return QStringLiteral("VOLT");
    case 5:
        return QStringLiteral("CURR");
    case 6:
        return QStringLiteral("ENRG");
    case 7:
        return QStringLiteral("UTIL");
    case 8:
        return QStringLiteral("MEM");
    case 9:
        return QStringLiteral("THRU");
    default:
        return QStringLiteral("SENS");
    }
}

inline QString sensorSectionName(int type)
{
    switch (type) {
    case 0:
        return QStringLiteral("Temperatures");
    case 1:
        return QStringLiteral("Fans");
    case 2:
        return QStringLiteral("Clocks");
    case 3:
        return QStringLiteral("Power");
    case 4:
        return QStringLiteral("Voltages");
    case 5:
        return QStringLiteral("Currents");
    case 6:
        return QStringLiteral("Energy");
    case 7:
        return QStringLiteral("Utilization");
    case 8:
        return QStringLiteral("Memory");
    case 9:
        return QStringLiteral("Throughput");
    default:
        return QStringLiteral("Other");
    }
}
