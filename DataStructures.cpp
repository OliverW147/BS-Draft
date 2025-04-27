#include "DataStructures.h"

// --- Serialization for BrawlerStatsData ---
QDataStream &operator<<(QDataStream &out, const BrawlerStatsData &stats) {
    out << stats.wins << stats.plays;
    return out;
}

QDataStream &operator>>(QDataStream &in, BrawlerStatsData &stats) {
    in >> stats.wins >> stats.plays;
    return in;
}

// --- Serialization for MapModeStatsData ---
QDataStream &operator<<(QDataStream &out, const MapModeStatsData &stats) {
    out << stats.brawlerStats << stats.synergyStats << stats.counterStats << stats.totalWeightedPlays;
    return out;
}

QDataStream &operator>>(QDataStream &in, MapModeStatsData &stats) {
    in >> stats.brawlerStats >> stats.synergyStats >> stats.counterStats >> stats.totalWeightedPlays;
    return in;
}


// --- Serialization for CacheMetadata ---
QDataStream &operator<<(QDataStream &out, const CacheMetadata &meta) {
    out << meta.cacheCreationTime;
    return out;
}
QDataStream &operator>>(QDataStream &in, CacheMetadata &meta) {
    in >> meta.cacheCreationTime;
    return in;
}

// --- Serialization for CacheData ---
QDataStream &operator<<(QDataStream &out, const CacheData &data) {
    out << data.stats << data.allBrawlers << data.discoveredMapModes << data.metadata;
    return out;
}

QDataStream &operator>>(QDataStream &in, CacheData &data) {
    in >> data.stats >> data.allBrawlers >> data.discoveredMapModes >> data.metadata;
    return in;
}