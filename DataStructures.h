#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <QString>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QDataStream>
#include <limits>
#include <atomic>
#include <QMetaType>

// --- Basic Stats Structs ---

void atomic_add_double(std::atomic<double>& atomic_var, double value);

struct BrawlerStats {
    std::atomic<double> wins{0.0};
    std::atomic<double> plays{0.0};

    // Need default constructor for QHash/maps
    BrawlerStats() = default;
    // Copy constructor needed for atomic members
    BrawlerStats(const BrawlerStats& other) : wins(other.wins.load()), plays(other.plays.load()) {}
    // Assignment operator needed for atomic members
    BrawlerStats& operator=(const BrawlerStats& other) {
        if (this != &other) {
            wins.store(other.wins.load());
            plays.store(other.plays.load());
        }
        return *this;
    }
};

// Need non-atomic version for serialization
struct BrawlerStatsData {
    double wins = 0.0;
    double plays = 0.0;
};

QDataStream &operator<<(QDataStream &out, const BrawlerStatsData &stats);
QDataStream &operator>>(QDataStream &in, BrawlerStatsData &stats);


struct MapModeStats {
    // Use QHash for faster lookups, similar to Python dict
    QHash<QString, BrawlerStats> brawlerStats;
    QHash<QString, BrawlerStats> synergyStats; // Key: Sorted "Brawler1|Brawler2"
    QHash<QString, BrawlerStats> counterStats; // Key: "BrawlerUs|BrawlerThem"
    std::atomic<double> totalWeightedPlays{0.0};

    // Default constructor
    MapModeStats() = default;
    // Copy constructor for atomic members
    MapModeStats(const MapModeStats& other);
    // Assignment operator for atomic members
    MapModeStats& operator=(const MapModeStats& other);
};

// Need non-atomic version for serialization
struct MapModeStatsData {
     QHash<QString, BrawlerStatsData> brawlerStats;
     QHash<QString, BrawlerStatsData> synergyStats;
     QHash<QString, BrawlerStatsData> counterStats;
     double totalWeightedPlays = 0.0;
};

QDataStream &operator<<(QDataStream &out, const MapModeStatsData &stats);
QDataStream &operator>>(QDataStream &in, MapModeStatsData &stats);


// --- Heuristic Structs ---

struct HeuristicWeights {
    double winRate = 0.5;
    double synergy = 0.3;
    double counter = 0.4;
    double pickRate = 0.2;
};

Q_DECLARE_METATYPE(HeuristicWeights);
struct HeuristicScoreComponents {
    double totalScore = -std::numeric_limits<double>::infinity();
    double winRate = 0.0;
    double avgSynergy = 0.5;
    double avgCounter = 0.5;
    double pickRate = 0.0;
    double wrComponent = 0.0;
    double synergyComponent = 0.0;
    double counterComponent = 0.0;
    double prComponent = 0.0;
};


// --- MCTS Struct ---
struct MCTSResult {
    QString move;
    int visits = 0;
    double winRate = 0.0; // Probability of the *current* player winning if this move is made

    // Default constructor for QVector etc.
    MCTSResult() = default;
    MCTSResult(QString m, int v, double wr) : move(m), visits(v), winRate(wr) {}
};

// --- Processed Game Data (Example) ---
struct PlayerData {
    QString brawlerName;
    int rank = 0;
};

struct ProcessedGame {
    QString mode;
    QString map;
    QVector<PlayerData> winningTeamData;
    QVector<PlayerData> losingTeamData;
};


// --- Cache Data Structure ---
// Use QHash for stats as it's efficient for lookups
using StatsContainer = QHash<QString, QHash<QString, MapModeStatsData>>;

struct CacheMetadata {
    qint64 cacheCreationTime = 0;
    // Add config parameters if strict validation is needed later
};
QDataStream &operator<<(QDataStream &out, const CacheMetadata &meta);
QDataStream &operator>>(QDataStream &in, CacheMetadata &meta);


struct CacheData {
    StatsContainer stats;
    QSet<QString> allBrawlers;
    QHash<QString, QSet<QString>> discoveredMapModes;
    CacheMetadata metadata;
};

QDataStream &operator<<(QDataStream &out, const CacheData &data);
QDataStream &operator>>(QDataStream &in, CacheData &data);


// Helper for sorting pairs for synergy/counter keys
inline QString sortedPairKey(const QString& b1, const QString& b2) {
    return (b1 < b2) ? b1 + "|" + b2 : b2 + "|" + b1;
}

inline QString counterPairKey(const QString& bUs, const QString& bThem) {
    return bUs + "|" + bThem;
}

// Explicit implementations for copy constructor/assignment for MapModeStats
inline MapModeStats::MapModeStats(const MapModeStats& other)
    : totalWeightedPlays(other.totalWeightedPlays.load())
{
    // Deep copy the maps, handling atomic BrawlerStats
    for (auto it = other.brawlerStats.constBegin(); it != other.brawlerStats.constEnd(); ++it) {
        brawlerStats.insert(it.key(), it.value()); // Calls BrawlerStats copy constructor
    }
     for (auto it = other.synergyStats.constBegin(); it != other.synergyStats.constEnd(); ++it) {
        synergyStats.insert(it.key(), it.value());
    }
     for (auto it = other.counterStats.constBegin(); it != other.counterStats.constEnd(); ++it) {
        counterStats.insert(it.key(), it.value());
    }
}

inline MapModeStats& MapModeStats::operator=(const MapModeStats& other) {
    if (this != &other) {
        totalWeightedPlays.store(other.totalWeightedPlays.load());
        brawlerStats.clear();
        synergyStats.clear();
        counterStats.clear();
        for (auto it = other.brawlerStats.constBegin(); it != other.brawlerStats.constEnd(); ++it) {
            brawlerStats.insert(it.key(), it.value()); // Calls BrawlerStats copy constructor/assignment
        }
        for (auto it = other.synergyStats.constBegin(); it != other.synergyStats.constEnd(); ++it) {
            synergyStats.insert(it.key(), it.value());
        }
        for (auto it = other.counterStats.constBegin(); it != other.counterStats.constEnd(); ++it) {
            counterStats.insert(it.key(), it.value());
        }
    }
    return *this;
}


#endif // DATASTRUCTURES_H