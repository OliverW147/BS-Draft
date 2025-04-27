#ifndef STATSCALCULATOR_H
#define STATSCALCULATOR_H

#include <QHash>
#include <QString>
#include <QVector>
#include <QSet>
#include <optional> // C++17 required
#include "DataStructures.h"
#include "AppConfig.h"

class StatsCalculator {
public:
    // Constructor for calculating from games
    StatsCalculator(const QVector<ProcessedGame>& processedGames, const AppConfig& config);
    // Constructor for loading from cache (or empty)
    StatsCalculator(const AppConfig& config);


    void calculateStats(const QVector<ProcessedGame>& processedGames);
    void setStatsFromCacheData(const CacheData& cacheData); // Load from non-atomic cache struct
    CacheData getStatsForCache() const; // Get non-atomic data for saving

    // --- Stat Accessors ---
    // Use std::optional to indicate if stats exist for the map/mode
    std::optional<double> getWinRate(const QString& brawler, const QString& mapName, const QString& mode) const;
    std::optional<double> getPickRate(const QString& brawler, const QString& mapName, const QString& mode) const;
    // Synergy/Counter return 0.5 if no data, matching Python's behavior
    double getSynergyScore(const QString& brawler1, const QString& brawler2, const QString& mapName, const QString& mode) const;
    double getCounterScore(const QString& brawlerUs, const QString& brawlerThem, const QString& mapName, const QString& mode) const;

private:
    // Helper to safely get map/mode stats (returns pointer or nullptr)
    const MapModeStats* getMapModeStats(const QString& mapName, const QString& mode) const;
    MapModeStats* getMapModeStats(const QString& mapName, const QString& mode); // Non-const version

    void updateTeamSynergy(MapModeStats& mapModeStats, const QVector<PlayerData>& teamData, bool win);

    const AppConfig& m_config;
    // Main storage: Map -> Mode -> Stats
    // Use QHash for efficiency, outer key is map name, inner key is mode name
    QHash<QString, QHash<QString, MapModeStats>> m_stats;
};

#endif // STATSCALCULATOR_H