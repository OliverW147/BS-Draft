#include "AppConfig.h"
#include <QDebug>
#include <algorithm> // for std::max, std::min

// Constructor accepts the full config file path
AppConfig::AppConfig(const QString& configFilePath)
    : m_settings(configFilePath, QSettings::IniFormat) // Use explicit path
{
    qInfo() << "Using config file:" << m_settings.fileName();
    if (!QFileInfo::exists(m_settings.fileName())) {
        qInfo() << "Config file not found, creating with defaults.";
        // Ensure defaults are set before initial save
        loadDefaults();
        save(); // Save defaults immediately if file doesn't exist
    } else {
        loadDefaults(); // Load defaults first
        load();         // Override with saved settings
    }
}

void AppConfig::loadDefaults() {
    // Reset internal 'current' values to defaults before loading/saving
    m_currentWeights = m_defaultWeights;
    m_currentMctsTimeLimit = m_defaultMctsTimeLimit;
    // Other defaults are read directly when needed using value() with fallback
}

void AppConfig::load() {
    // No need to call loadDefaults() here anymore, constructor handles it

    m_settings.beginGroup("Settings");
    // Read directly via getters using value() - example:
    // double loadedSmoothingK = m_settings.value("SmoothingK", m_defaultSmoothingK).toDouble();
    m_currentMctsTimeLimit = m_settings.value("MctsTimeLimit", m_defaultMctsTimeLimit).toDouble();
    // Exploration, Result Count, Interval getters read directly from m_settings
    m_settings.endGroup();

    m_settings.beginGroup("Weights");
    m_currentWeights.winRate = m_settings.value("WinRate", m_defaultWeights.winRate).toDouble();
    m_currentWeights.synergy = m_settings.value("Synergy", m_defaultWeights.synergy).toDouble();
    m_currentWeights.counter = m_settings.value("Counter", m_defaultWeights.counter).toDouble();
    m_currentWeights.pickRate = m_settings.value("PickRate", m_defaultWeights.pickRate).toDouble();
    m_settings.endGroup();

    qInfo() << "Configuration loaded from" << m_settings.fileName();
}

void AppConfig::save() {
    qInfo() << "Saving configuration to" << m_settings.fileName();
    m_settings.beginGroup("Settings");
    // Use the getter methods which read from QSettings or return default
    m_settings.setValue("SmoothingK", smoothingK());
    m_settings.setValue("MinRank", minRank());
    m_settings.setValue("MaxRankConsidered", maxRankConsidered());
    m_settings.setValue("RankWeightDivisor", rankWeightScaleDivisor());
    m_settings.setValue("LowPickRateThreshold", lowPickRateThreshold());
    m_settings.setValue("LowConfidenceWinRateTarget", lowConfidenceWinRateTarget());
    // Save the potentially updated values stored in members
    m_settings.setValue("MctsTimeLimit", m_currentMctsTimeLimit);
    m_settings.setValue("MctsExplorationParam", mctsExplorationParam());
    m_settings.setValue("MctsResultCount", mctsResultCount());
    m_settings.setValue("MctsUpdateIntervalIters", mctsUpdateIntervalIters());
    m_settings.endGroup();

    m_settings.beginGroup("Weights");
    // Save the potentially updated weights stored in members
    m_settings.setValue("WinRate", m_currentWeights.winRate);
    m_settings.setValue("Synergy", m_currentWeights.synergy);
    m_settings.setValue("Counter", m_currentWeights.counter);
    m_settings.setValue("PickRate", m_currentWeights.pickRate);
    m_settings.endGroup();

    m_settings.sync(); // Ensure changes are written to disk
}

// --- Getters ---
// (No changes needed in getters - they read from m_settings or return defaults,
// except for heuristicWeights and mctsTimeLimit which read from members)

double AppConfig::smoothingK() const {
    return m_settings.value("Settings/SmoothingK", m_defaultSmoothingK).toDouble();
}

int AppConfig::minRank() const {
    return m_settings.value("Settings/MinRank", m_defaultMinRank).toInt();
}

int AppConfig::maxRankConsidered() const {
    return m_settings.value("Settings/MaxRankConsidered", m_defaultMaxRankConsidered).toInt();
}

double AppConfig::rankWeightScaleDivisor() const {
    // Ensure divisor isn't zero or negative when reading/using
    double divisor = m_settings.value("Settings/RankWeightDivisor", m_defaultRankWeightScaleDivisor).toDouble();
    return (divisor <= 0) ? 1.0 : divisor; // Return 1.0 if config value is invalid
}


double AppConfig::lowPickRateThreshold() const {
     return m_settings.value("Settings/LowPickRateThreshold", m_defaultLowPrThreshold).toDouble();
}

double AppConfig::lowConfidenceWinRateTarget() const {
     return m_settings.value("Settings/LowConfidenceWinRateTarget", m_defaultLowConfidenceWrTarget).toDouble();
}

HeuristicWeights AppConfig::heuristicWeights() const {
    // Return the 'current' weights loaded/defaulted
    return m_currentWeights;
}

double AppConfig::mctsTimeLimit() const {
    // Return the 'current' time limit loaded/defaulted/set
    return m_currentMctsTimeLimit;
}

double AppConfig::mctsExplorationParam() const {
    return m_settings.value("Settings/MctsExplorationParam", m_defaultMctsExplorationParam).toDouble();
}

int AppConfig::mctsResultCount() const {
    return m_settings.value("Settings/MctsResultCount", m_defaultMctsResultCount).toInt();
}

int AppConfig::mctsUpdateIntervalIters() const {
     return m_settings.value("Settings/MctsUpdateIntervalIters", m_defaultMctsUpdateIntervalIters).toInt();
}

// --- Setters ---
// void AppConfig::setHeuristicWeights(const HeuristicWeights& weights) {
//     // This is now unused if UI is removed
//     // m_currentWeights = weights;
// }

void AppConfig::setMctsTimeLimit(double limit) {
    if (limit > 0) {
        m_currentMctsTimeLimit = limit;
    } else {
        qWarning() << "Attempted to set invalid MCTS time limit:" << limit << ". Using default:" << m_defaultMctsTimeLimit;
        m_currentMctsTimeLimit = m_defaultMctsTimeLimit; // Reset to default if invalid
    }
     // save() needs to be called explicitly later (e.g., on window close)
}


// --- Helper ---
double AppConfig::getRankWeight(int rank) const {
    int clampedRank = std::max(minRank(), std::min(rank, maxRankConsidered()));
    // Use the getter for the divisor, which handles invalid values
    double divisor = rankWeightScaleDivisor();
    // Ensure minRank is subtracted correctly
    double weight = (static_cast<double>(clampedRank - minRank()) + divisor) / divisor;
    return std::max(0.1, weight); // Minimum weight
}