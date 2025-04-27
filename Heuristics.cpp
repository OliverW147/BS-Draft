#include "Heuristics.h"
#include <QDebug>
#include <cmath>
#include <limits>
#include <algorithm> // for std::sort

QPair<QString, QHash<QString, HeuristicScoreComponents>>
suggestPickHeuristic(const DraftState& draftState,
                     const StatsCalculator& statsCalculator,
                     const HeuristicWeights& weights)
{
    QVector<QString> legalMoves = draftState.getLegalMoves();
    if (legalMoves.isEmpty()) {
        return {"", {}}; // No best pick, empty scores map
    }

    QString bestBrawler = "";
    double bestScore = -std::numeric_limits<double>::infinity();
    QHash<QString, HeuristicScoreComponents> brawlerScores;

    const QVector<QString>& currentTeamPicks = (draftState.currentTurn() == "team1") ? draftState.team1Picks() : draftState.team2Picks();
    const QVector<QString>& opponentPicks = (draftState.currentTurn() == "team1") ? draftState.team2Picks() : draftState.team1Picks();

    for (const QString& brawler : legalMoves) {
        HeuristicScoreComponents scores;

        // --- Win Rate Component ---
        // Use .value_or with a default (e.g., 0.5 or config default) if optional is nullopt
        double wr = statsCalculator.getWinRate(brawler, draftState.mapName(), draftState.modeName())
                .value_or(0.5); // Or a reasonable default if getWinRate itself could fail
        scores.winRate = wr;
        scores.wrComponent = weights.winRate * (wr - 0.5); // Score relative to 0.5 baseline

        // --- Synergy Component ---
        scores.avgSynergy = 0.5; // Default
        scores.synergyComponent = 0.0;
        if (!currentTeamPicks.isEmpty()) {
            double totalSynScoreDiff = 0.0;
            int count = 0;
            for (const QString& teammate : currentTeamPicks) {
                // getSynergyScore returns 0.5 if no data
                double pairWR = statsCalculator.getSynergyScore(brawler, teammate, draftState.mapName(), draftState.modeName());
                totalSynScoreDiff += (pairWR - 0.5);
                count++;
            }
            if (count > 0) {
                double avgSynDiff = totalSynScoreDiff / count;
                scores.avgSynergy = 0.5 + avgSynDiff; // Average actual synergy score
                scores.synergyComponent = weights.synergy * avgSynDiff;
            }
        }

        // --- Counter Component ---
        scores.avgCounter = 0.5; // Default
        scores.counterComponent = 0.0;
        if (!opponentPicks.isEmpty()) {
            double totalCtrScoreDiff = 0.0;
            int count = 0;
            for (const QString& opponent : opponentPicks) {
                // getCounterScore returns 0.5 if no data
                double matchupWR = statsCalculator.getCounterScore(brawler, opponent, draftState.mapName(), draftState.modeName());
                totalCtrScoreDiff += (matchupWR - 0.5);
                count++;
            }
            if (count > 0) {
                double avgCtrDiff = totalCtrScoreDiff / count;
                scores.avgCounter = 0.5 + avgCtrDiff; // Average actual counter score
                scores.counterComponent = weights.counter * avgCtrDiff;
            }
        }

        // --- Pick Rate Component ---
        // Use .value_or(0.0) if pick rate is not available
        double pr = statsCalculator.getPickRate(brawler, draftState.mapName(), draftState.modeName()).value_or(0.0);
        scores.pickRate = pr;
        scores.prComponent = weights.pickRate * pr; // Direct contribution from pick rate

        // --- Total Score ---
        scores.totalScore = scores.wrComponent + scores.synergyComponent + scores.counterComponent + scores.prComponent;

        brawlerScores.insert(brawler, scores);

        // Update best score/brawler
        if (scores.totalScore > bestScore) {
            bestScore = scores.totalScore;
            bestBrawler = brawler;
        }
    }

    // No need to sort the QHash here, MainWindow can sort the results if needed for display
    return {bestBrawler, brawlerScores};
}


QVector<QString>
suggestBanHeuristic(const DraftState& draftState,
                    const StatsCalculator& statsCalculator,
                    int numSuggestions)
{
    QVector<QString> legalMoves = draftState.getLegalMoves();
    if (legalMoves.isEmpty()) {
        return {};
    }

    QVector<QPair<QString, double>> banCandidates; // Store as pairs for sorting
    banCandidates.reserve(legalMoves.size());

    for (const QString& brawler : legalMoves) {
        // Base ban suggestion purely on adjusted win rate
        double wr = statsCalculator.getWinRate(brawler, draftState.mapName(), draftState.modeName())
               .value_or(0.0); // Default to 0 if WR calculation fails entirely
        banCandidates.append({brawler, wr});
    }

    // Sort candidates by win rate descending
    std::sort(banCandidates.begin(), banCandidates.end(),
              [](const QPair<QString, double>& a, const QPair<QString, double>& b) {
                  return a.second > b.second; // Sort descending by score
              });

    // Extract top N suggestions
    QVector<QString> suggestions;
    int count = 0;
    for (const auto& candidate : banCandidates) {
        if (count >= numSuggestions) break;
        suggestions.append(candidate.first);
        count++;
    }

    return suggestions;
}


double
predictWinProbabilityModel(const QVector<QString>& team1Brawlers,
                           const QVector<QString>& team2Brawlers,
                           const QString& mapName,
                           const QString& modeName,
                           const StatsCalculator& statsCalculator,
                           const HeuristicWeights& evalWeights) // Use specific eval weights
{
    if (team1Brawlers.size() != 3 || team2Brawlers.size() != 3) {
        qWarning() << "predictWinProbabilityModel called with incomplete teams.";
        return 0.5; // Default for invalid input
    }

    // 1. Average Win Rate Difference
    double t1AvgWR = 0.0, t2AvgWR = 0.0;
    for(const auto& b : team1Brawlers) t1AvgWR += statsCalculator.getWinRate(b, mapName, modeName).value_or(0.5);
    for(const auto& b : team2Brawlers) t2AvgWR += statsCalculator.getWinRate(b, mapName, modeName).value_or(0.5);
    t1AvgWR /= 3.0;
    t2AvgWR /= 3.0;
    double baseWrDiff = t1AvgWR - t2AvgWR;

    // 2. Average Synergy Difference
    auto calculateAvgSynergyDiff = [&](const QVector<QString>& team) {
        double synergySumDiff = 0.0;
        int pairs = 0;
        for (int i = 0; i < 3; ++i) {
            for (int j = i + 1; j < 3; ++j) {
                double synergy = statsCalculator.getSynergyScore(team[i], team[j], mapName, modeName);
                synergySumDiff += (synergy - 0.5);
                pairs++;
            }
        }
        return (pairs > 0) ? synergySumDiff / pairs : 0.0;
    };
    double t1AvgSynDiff = calculateAvgSynergyDiff(team1Brawlers);
    double t2AvgSynDiff = calculateAvgSynergyDiff(team2Brawlers);
    double synergyDiff = t1AvgSynDiff - t2AvgSynDiff;


    // 3. Counter Interaction Difference (Average and Peak)
    double t1_vs_t2_sum_diff = 0.0;
    double max_t1_vs_t2_score_diff = -1.0; // Max (T1[i] vs T2[j] score - 0.5)
    double max_t2_vs_t1_score_diff = -1.0; // Max (T2[j] vs T1[i] score - 0.5)
    int interactions = 0;
    for (const auto& b1 : team1Brawlers) {
        for (const auto& b2 : team2Brawlers) {
             // T1 vs T2 perspective
            double t1_vs_t2_score = statsCalculator.getCounterScore(b1, b2, mapName, modeName);
            double current_t1_vs_t2_diff = t1_vs_t2_score - 0.5;
            t1_vs_t2_sum_diff += current_t1_vs_t2_diff;
            max_t1_vs_t2_score_diff = std::max(max_t1_vs_t2_score_diff, current_t1_vs_t2_diff);

            // T2 vs T1 perspective (for peak calculation)
            double t2_vs_t1_score = statsCalculator.getCounterScore(b2, b1, mapName, modeName);
            double current_t2_vs_t1_diff = t2_vs_t1_score - 0.5;
             max_t2_vs_t1_score_diff = std::max(max_t2_vs_t1_score_diff, current_t2_vs_t1_diff);

            interactions++;
        }
    }
    double counterAdvAvg = (interactions > 0) ? t1_vs_t2_sum_diff / interactions : 0.0;
    // Peak counter advantage: How much better is T1's best matchup vs T2's best matchup?
    double peakCounterAdv = max_t1_vs_t2_score_diff - max_t2_vs_t1_score_diff;


    // Combine factors using evaluation weights
    // Mapping HeuristicWeights fields for evaluation:
    // evalWeights.winRate -> Weight for base WR diff
    // evalWeights.synergy -> Weight for synergy diff
    // evalWeights.counter -> Weight for average counter advantage
    // evalWeights.pickRate -> Weight for *peak* counter advantage (reusing field)
    double totalScoreDiff = (evalWeights.winRate * baseWrDiff) +
                            (evalWeights.synergy * synergyDiff) +
                            (evalWeights.counter * counterAdvAvg) +
                            (evalWeights.pickRate * peakCounterAdv); // Using pickRate weight for peak counter

    // Logistic function (sigmoid) to map score difference to probability
    // k controls the steepness of the curve
    double k = 2.0; // Same factor as Python example
    double predictedRate = 1.0 / (1.0 + std::exp(-k * totalScoreDiff));

    // Clamp result between 0 and 1
    return std::max(0.0, std::min(1.0, predictedRate));
}