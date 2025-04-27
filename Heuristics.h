#ifndef HEURISTICS_H
#define HEURISTICS_H

#include "DataStructures.h"
#include "DraftState.h"
#include "StatsCalculator.h"
#include "AppConfig.h" // For weights
#include <QPair>
#include <QHash>
#include <QString>
#include <QVector>

// Suggests a pick based on weighted heuristics
QPair<QString, QHash<QString, HeuristicScoreComponents>>
suggestPickHeuristic(const DraftState& draftState,
                     const StatsCalculator& statsCalculator,
                     const HeuristicWeights& weights);

// Suggests bans based on high win rate
QVector<QString>
suggestBanHeuristic(const DraftState& draftState,
                    const StatsCalculator& statsCalculator,
                    int numSuggestions = 3);

// Predicts win probability for Team 1 based on a heuristic model
// Note: The weights used here might differ from the pick suggestion weights
// Using the same HeuristicWeights struct for simplicity, but could be separate.
double
predictWinProbabilityModel(const QVector<QString>& team1Brawlers,
                           const QVector<QString>& team2Brawlers,
                           const QString& mapName,
                           const QString& modeName,
                           const StatsCalculator& statsCalculator,
                           const HeuristicWeights& evalWeights); // Weights for evaluation

#endif // HEURISTICS_H