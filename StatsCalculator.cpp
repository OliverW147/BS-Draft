#include "StatsCalculator.h"
#include "DataStructures.h"
#include <QDebug>
#include <cmath>     // For std::max, std::min
#include <numeric>   // For std::accumulate if needed
#include <algorithm> // For std::sort
#include <atomic> // Make sure this is included

// Helper function for atomic double addition
void atomic_add_double(std::atomic<double>& atomic_var, double value) {
    double current_value = atomic_var.load();
    double desired_value;
    do {
        desired_value = current_value + value;
    } while (!atomic_var.compare_exchange_weak(current_value, desired_value));
    // compare_exchange_weak updates current_value if it fails, so the loop continues
    // with the new current value until it succeeds.
}

// Constructor for calculating from games
StatsCalculator::StatsCalculator(const QVector<ProcessedGame>& processedGames, const AppConfig& config)
    : m_config(config)
{
    if (!processedGames.isEmpty()) {
        calculateStats(processedGames);
        qInfo() << "Statistics calculation complete.";
    } else {
         qInfo() << "StatsCalculator initialized without games to process immediately.";
    }
}

// Constructor for loading from cache (or empty)
StatsCalculator::StatsCalculator(const AppConfig& config)
    : m_config(config)
{
     qInfo() << "StatsCalculator initialized (likely for cache loading).";
}


void StatsCalculator::calculateStats(const QVector<ProcessedGame>& processedGames) {
    qInfo() << "Calculating rank-weighted statistics from" << processedGames.size() << "games...";
    // QElapsedTimer timer; timer.start(); // For timing

    m_stats.clear(); // Clear previous stats

    // Iterate through games and accumulate weighted stats
    for (const auto& game : processedGames) {
        // Get or create the entry for this map and mode
        // QHash automatically default-constructs MapModeStats if needed
        MapModeStats& currentMapModeStats = m_stats[game.map][game.mode];

        // Update Brawler Wins/Plays and Total Plays
        double gameTotalWeightContribution = 0; // Track weight added by this game to total plays

        // Winners
        for (const auto& playerData : game.winningTeamData) {
            double weight = m_config.getRankWeight(playerData.rank);
            BrawlerStats& bStats = currentMapModeStats.brawlerStats[playerData.brawlerName]; // Creates if new
            atomic_add_double(bStats.wins, weight);
            atomic_add_double(bStats.plays, weight);
            gameTotalWeightContribution += weight;
        }
        // Losers
        for (const auto& playerData : game.losingTeamData) {
            double weight = m_config.getRankWeight(playerData.rank);
            BrawlerStats& bStats = currentMapModeStats.brawlerStats[playerData.brawlerName]; // Creates if new
            // No wins update for losers
            atomic_add_double(bStats.plays, weight);
            gameTotalWeightContribution += weight;
        }
        atomic_add_double(currentMapModeStats.totalWeightedPlays, gameTotalWeightContribution);


        // Update Synergy Stats
        updateTeamSynergy(currentMapModeStats, game.winningTeamData, true);
        updateTeamSynergy(currentMapModeStats, game.losingTeamData, false);

        // Update Counter Stats
        for (const auto& winnerData : game.winningTeamData) {
            double weightWin = m_config.getRankWeight(winnerData.rank);
            for (const auto& loserData : game.losingTeamData) {
                 double weightLose = m_config.getRankWeight(loserData.rank);

                // Winner vs Loser perspective (Winner wins the matchup)
                QString winLoseKey = counterPairKey(winnerData.brawlerName, loserData.brawlerName);
                BrawlerStats& cStatsWin = currentMapModeStats.counterStats[winLoseKey];
                atomic_add_double(cStatsWin.wins, weightWin); // NEW
                atomic_add_double(cStatsWin.plays, weightWin); // NEW

                // Loser vs Winner perspective (Loser plays the matchup)
                QString loseWinKey = counterPairKey(loserData.brawlerName, winnerData.brawlerName);
                BrawlerStats& cStatsLose = currentMapModeStats.counterStats[loseWinKey];
                // Loser only contributes play count from their perspective
                atomic_add_double(cStatsLose.plays, weightLose);
            }
        }
    } // End game loop

    // qInfo() << "Statistics calculation took" << timer.elapsed() << "ms";
}

void StatsCalculator::setStatsFromCacheData(const CacheData& cacheData) {
     qInfo() << "Loading stats from cache data...";
     m_stats.clear();

     // Convert non-atomic CacheData structures to atomic MapModeStats
     for (auto mapIt = cacheData.stats.constBegin(); mapIt != cacheData.stats.constEnd(); ++mapIt) {
         const QString& mapName = mapIt.key();
         for (auto modeIt = mapIt.value().constBegin(); modeIt != mapIt.value().constEnd(); ++modeIt) {
             const QString& modeName = modeIt.key();
             const MapModeStatsData& sourceData = modeIt.value();
             MapModeStats& targetStats = m_stats[mapName][modeName]; // Create target entry

             targetStats.totalWeightedPlays = sourceData.totalWeightedPlays;

             // Convert brawler stats
             for(auto bsIt = sourceData.brawlerStats.constBegin(); bsIt != sourceData.brawlerStats.constEnd(); ++bsIt) {
                 targetStats.brawlerStats[bsIt.key()].wins = bsIt.value().wins;
                 targetStats.brawlerStats[bsIt.key()].plays = bsIt.value().plays;
             }
             // Convert synergy stats
             for(auto ssIt = sourceData.synergyStats.constBegin(); ssIt != sourceData.synergyStats.constEnd(); ++ssIt) {
                 targetStats.synergyStats[ssIt.key()].wins = ssIt.value().wins;
                 targetStats.synergyStats[ssIt.key()].plays = ssIt.value().plays;
             }
             // Convert counter stats
             for(auto csIt = sourceData.counterStats.constBegin(); csIt != sourceData.counterStats.constEnd(); ++csIt) {
                 targetStats.counterStats[csIt.key()].wins = csIt.value().wins;
                 targetStats.counterStats[csIt.key()].plays = csIt.value().plays;
             }
         }
     }
     qInfo() << "Stats loaded into calculator.";
}


CacheData StatsCalculator::getStatsForCache() const {
    qInfo() << "Preparing stats data for caching...";
    CacheData cacheData;

    // Convert atomic MapModeStats to non-atomic CacheData structures
    for (auto mapIt = m_stats.constBegin(); mapIt != m_stats.constEnd(); ++mapIt) {
        const QString& mapName = mapIt.key();
        for (auto modeIt = mapIt.value().constBegin(); modeIt != mapIt.value().constEnd(); ++modeIt) {
            const QString& modeName = modeIt.key();
            const MapModeStats& sourceStats = modeIt.value();
            MapModeStatsData& targetData = cacheData.stats[mapName][modeName]; // Create target entry

            targetData.totalWeightedPlays = sourceStats.totalWeightedPlays.load();

            // Convert brawler stats
            for(auto bsIt = sourceStats.brawlerStats.constBegin(); bsIt != sourceStats.brawlerStats.constEnd(); ++bsIt) {
                targetData.brawlerStats[bsIt.key()].wins = bsIt.value().wins.load();
                targetData.brawlerStats[bsIt.key()].plays = bsIt.value().plays.load();
            }
             // Convert synergy stats
            for(auto ssIt = sourceStats.synergyStats.constBegin(); ssIt != sourceStats.synergyStats.constEnd(); ++ssIt) {
                targetData.synergyStats[ssIt.key()].wins = ssIt.value().wins.load();
                targetData.synergyStats[ssIt.key()].plays = ssIt.value().plays.load();
            }
             // Convert counter stats
            for(auto csIt = sourceStats.counterStats.constBegin(); csIt != sourceStats.counterStats.constEnd(); ++csIt) {
                targetData.counterStats[csIt.key()].wins = csIt.value().wins.load();
                targetData.counterStats[csIt.key()].plays = csIt.value().plays.load();
            }
        }
    }
    qInfo() << "Stats data prepared for caching.";
    return cacheData; // RVO should handle this efficiently
}


// Helper to get stats pointer (const version)
const MapModeStats* StatsCalculator::getMapModeStats(const QString& mapName, const QString& mode) const {
    auto mapIt = m_stats.constFind(mapName);
    if (mapIt == m_stats.constEnd()) {
        return nullptr;
    }
    auto modeIt = mapIt.value().constFind(mode);
    if (modeIt == mapIt.value().constEnd()) {
        return nullptr;
    }
    return &(*modeIt); // Return address of the found MapModeStats
}

// Helper to get stats pointer (non-const version)
MapModeStats* StatsCalculator::getMapModeStats(const QString& mapName, const QString& mode) {
    auto mapIt = m_stats.find(mapName);
    if (mapIt == m_stats.end()) {
        return nullptr;
    }
    auto modeIt = mapIt.value().find(mode);
    if (modeIt == mapIt.value().end()) {
        return nullptr;
    }
    return &(*modeIt); // Return address of the found MapModeStats
}


// Helper to update synergy stats for a team
void StatsCalculator::updateTeamSynergy(MapModeStats& mapModeStats, const QVector<PlayerData>& teamData, bool win) {
    for (int i = 0; i < teamData.size(); ++i) {
        const PlayerData& p1 = teamData[i];
        for (int j = i + 1; j < teamData.size(); ++j) {
            const PlayerData& p2 = teamData[j];
            QString pairKey = sortedPairKey(p1.brawlerName, p2.brawlerName);

            // Use average rank for weighting synergy pairs
            double avgRank = (static_cast<double>(p1.rank) + p2.rank) / 2.0;
            double weight = m_config.getRankWeight(static_cast<int>(round(avgRank)));

            BrawlerStats& pairStats = mapModeStats.synergyStats[pairKey]; // Creates if new
            if (win) {
                atomic_add_double(pairStats.wins, weight);
            }
            atomic_add_double(pairStats.plays, weight);
        }
    }
}


// --- Stat Accessors ---

std::optional<double> StatsCalculator::getWinRate(const QString& brawler, const QString& mapName, const QString& mode) const {
    const MapModeStats* statsPtr = getMapModeStats(mapName, mode);
    if (!statsPtr) return std::nullopt; // No stats for this map/mode

    auto brawlerIt = statsPtr->brawlerStats.constFind(brawler);
    if (brawlerIt == statsPtr->brawlerStats.constEnd()) {
        // Brawler not found in stats for this map/mode, apply low confidence target
        return m_config.lowConfidenceWinRateTarget();
    }

    const BrawlerStats& bStats = *brawlerIt;
    double plays = bStats.plays.load();
    double wins = bStats.wins.load();
    double k = m_config.smoothingK();

    if (plays + k <= 0) {
        // Avoid division by zero, apply low confidence target
        return m_config.lowConfidenceWinRateTarget();
    }

    // Calculate smoothed win rate
    double smoothedWinRate = (wins + k * 0.5) / (plays + k);

    // Adjust for confidence based on pick rate
    std::optional<double> pickRateOpt = getPickRate(brawler, mapName, mode);
    double pickRate = pickRateOpt.value_or(0.0); // Use 0.0 if pick rate couldn't be calculated

    double prThreshold = m_config.lowPickRateThreshold();
    double confidenceFactor = 1.0; // Default to full confidence

    if (prThreshold > 0.0) {
         confidenceFactor = std::max(0.0, std::min(1.0, pickRate / prThreshold));
    }

    double adjustedWinRate = (smoothedWinRate * confidenceFactor) +
                             (m_config.lowConfidenceWinRateTarget() * (1.0 - confidenceFactor));

    // Clamp final rate between 0.0 and 1.0
    return std::max(0.0, std::min(1.0, adjustedWinRate));
}


std::optional<double> StatsCalculator::getPickRate(const QString& brawler, const QString& mapName, const QString& mode) const {
    const MapModeStats* statsPtr = getMapModeStats(mapName, mode);
    if (!statsPtr || statsPtr->totalWeightedPlays <= 0) {
        return std::nullopt; // No data or no plays for this map/mode
    }

    auto brawlerIt = statsPtr->brawlerStats.constFind(brawler);
    double brawlerPlays = 0.0;
    if (brawlerIt != statsPtr->brawlerStats.constEnd()) {
        brawlerPlays = brawlerIt->plays.load();
    }

    double totalPlays = statsPtr->totalWeightedPlays.load();
    if (totalPlays <= 0) return 0.0; // Avoid division by zero

    return brawlerPlays / totalPlays;
}


double StatsCalculator::getSynergyScore(const QString& brawler1, const QString& brawler2, const QString& mapName, const QString& mode) const {
    const MapModeStats* statsPtr = getMapModeStats(mapName, mode);
    if (!statsPtr) return 0.5; // Default if no map/mode stats

    QString pairKey = sortedPairKey(brawler1, brawler2);
    auto pairIt = statsPtr->synergyStats.constFind(pairKey);
    if (pairIt == statsPtr->synergyStats.constEnd()) {
        return 0.5; // No data for this pair
    }

    const BrawlerStats& pairStats = *pairIt;
    double plays = pairStats.plays.load();
    double wins = pairStats.wins.load();
    double k = m_config.smoothingK(); // Use same smoothing as win rate

    if (plays + k <= 0) {
        return 0.5; // Avoid division by zero or meaningless result
    }

    // Calculate smoothed win rate for the pair
    return std::max(0.0, std::min(1.0, (wins + k * 0.5) / (plays + k)));
}


double StatsCalculator::getCounterScore(const QString& brawlerUs, const QString& brawlerThem, const QString& mapName, const QString& mode) const {
    const MapModeStats* statsPtr = getMapModeStats(mapName, mode);
    if (!statsPtr) return 0.5; // Default if no map/mode stats

    QString matchupKey = counterPairKey(brawlerUs, brawlerThem);
    auto matchupIt = statsPtr->counterStats.constFind(matchupKey);
    if (matchupIt == statsPtr->counterStats.constEnd()) {
        return 0.5; // No data for this specific matchup
    }

    const BrawlerStats& matchupStats = *matchupIt;
    double plays = matchupStats.plays.load();
    double wins = matchupStats.wins.load();
    double k = m_config.smoothingK(); // Use same smoothing

    if (plays + k <= 0) {
        return 0.5; // Avoid division by zero or meaningless result
    }

    // Calculate smoothed win rate for us vs them
    return std::max(0.0, std::min(1.0, (wins + k * 0.5) / (plays + k)));
}