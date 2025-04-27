#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>
#include <QSettings>
#include <QCoreApplication>
#include <QFileInfo>
#include "DataStructures.h" // For HeuristicWeights

class AppConfig {
public:
    // Constructor now takes the full path to the config file
    AppConfig(const QString& configFilePath);

    void load();
    void save();

    // Configurable Values (Getters remain the same)
    double smoothingK() const;
    int minRank() const;
    int maxRankConsidered() const;
    double rankWeightScaleDivisor() const;
    double lowPickRateThreshold() const;
    double lowConfidenceWinRateTarget() const;
    HeuristicWeights heuristicWeights() const; // Reads from m_currentWeights
    double mctsTimeLimit() const;
    double mctsExplorationParam() const;
    int mctsResultCount() const;
    int mctsUpdateIntervalIters() const;

    // Setters primarily for GUI updates -> save
    // setHeuristicWeights is now only used internally if needed, UI doesn't set it
    // void setHeuristicWeights(const HeuristicWeights& weights);
    void setMctsTimeLimit(double limit);

    // Helper for rank weighting
    double getRankWeight(int rank) const;

private:
    void loadDefaults();

    QSettings m_settings; // Will be initialized with the path

    // Default values stored internally
    double m_defaultSmoothingK = 2.0;
    int m_defaultMinRank = 10;
    int m_defaultMaxRankConsidered = 22;
    double m_defaultRankWeightScaleDivisor = 3.0;
    double m_defaultLowPrThreshold = 0.03;
    double m_defaultLowConfidenceWrTarget = 0.0;
    HeuristicWeights m_defaultWeights = {0.5, 0.3, 0.4, 0.2};
    double m_defaultMctsTimeLimit = 7.0;
    double m_defaultMctsExplorationParam = 1.414;
    int m_defaultMctsResultCount = 10;
    int m_defaultMctsUpdateIntervalIters = 250;

    // Current values (loaded from settings, potentially updated by setters)
    HeuristicWeights m_currentWeights;
    double m_currentMctsTimeLimit;

};

#endif // APPCONFIG_H