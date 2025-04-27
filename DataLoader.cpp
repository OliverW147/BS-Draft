#include "DataLoader.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QMessageBox> // For error reporting if needed directly

DataLoader::DataLoader(QString filepath, const AppConfig& config)
    : m_filepath(filepath), m_config(config) {}

bool DataLoader::loadAndProcess() {
    if (!loadRawData()) {
        return false;
    }
    preprocessData();
    // Check if essential data was discovered
    return !m_allBrawlers.isEmpty() && !m_discoveredMapModes.isEmpty();
}

bool DataLoader::loadRawData() {
    QFile file(m_filepath);
    if (!file.exists()) {
         qCritical() << "Data file not found:" << m_filepath;
         // Optional: Show message box, but main usually handles this
         // QMessageBox::critical(nullptr, "Error", "Data file not found:\n" + m_filepath);
         return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Failed to open data file:" << m_filepath << file.errorString();
        // Optional: QMessageBox::critical(nullptr, "Error", "Failed to open data file:\n" + file.errorString());
        return false;
    }

    qInfo() << "Loading raw data from:" << m_filepath;
    QTextStream in(&file);
    int lineNum = 0;
    while (!in.atEnd()) {
        lineNum++;
        QString line = in.readLine();
        if (line.trimmed().isEmpty()) continue;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "Skipping invalid JSON on line" << lineNum << ":" << parseError.errorString();
            continue;
        }

        if (!doc.isObject()) {
             qWarning() << "Skipping non-object JSON on line" << lineNum;
             continue;
        }

        m_rawGames.append(doc.object());
    }
    file.close();
    qInfo() << "Loaded" << m_rawGames.size() << "raw game entries.";
    return true;
}

void DataLoader::preprocessData() {
    qInfo() << "Starting data preprocessing...";
    int skippedCount = 0;
    int rankIssues = 0;
    int formatIssues = 0;
    int missingPlayerTag = 0; // Keep track if needed, Python code had it
    int processedCount = 0;

    m_processedGames.clear();
    m_allBrawlers.clear();
    m_discoveredMapModes.clear();

    for (int idx = 0; idx < m_rawGames.size(); ++idx) {
        const QJsonObject& game = m_rawGames[idx];

        // Basic structure check
        if (!game.contains("event") || !game["event"].isObject() ||
            !game.contains("battle") || !game["battle"].isObject() ||
            !game.contains("queried_player_tag") || !game["queried_player_tag"].isString())
        {
            formatIssues++; continue;
        }

        QJsonObject event = game["event"].toObject();
        QJsonObject battle = game["battle"].toObject();
        QString queriedPlayerTag = game["queried_player_tag"].toString();

        // Event details check
        if (!event.contains("mode") || !event["mode"].isString() ||
            !event.contains("map") || !event["map"].isString())
        {
             formatIssues++; continue;
        }
        QString mode = event["mode"].toString();
        QString mapName = event["map"].toString();
        if (mode.isEmpty() || mapName.isEmpty()) {
             formatIssues++; continue;
        }


        // Battle details check
        if (!battle.contains("result") || !battle["result"].isString() ||
            !battle.contains("teams") || !battle["teams"].isArray())
        {
             formatIssues++; continue;
        }
        QString result = battle["result"].toString();
        QJsonArray teamsRaw = battle["teams"].toArray();

        if (result.isEmpty() || teamsRaw.size() < 2) {
             formatIssues++; continue;
        }

        // Extract and validate team data
        auto [team1Data, team1Valid] = extractTeamData(teamsRaw.at(0));
        auto [team2Data, team2Valid] = extractTeamData(teamsRaw.at(1));

        if (!team1Valid || !team2Valid) {
            rankIssues++; continue; // Covers invalid player/rank data or team size != 3
        }

        // Discover brawlers and map/modes
        for(const auto& p : team1Data) m_allBrawlers.insert(p.brawlerName);
        for(const auto& p : team2Data) m_allBrawlers.insert(p.brawlerName);
        m_discoveredMapModes[mode].insert(mapName); // QHash automatically creates entry if mode is new

        // Check if queried player is in the game (Python logic included this)
        bool playerInT1 = false;
        if (teamsRaw.at(0).isArray()){
            for(const QJsonValue& playerVal : teamsRaw.at(0).toArray()){
                if (playerVal.isObject() && playerVal.toObject().value("tag").toString() == queriedPlayerTag) {
                    playerInT1 = true; break;
                }
            }
        }
        bool playerInT2 = false;
         if (teamsRaw.at(1).isArray()){
            for(const QJsonValue& playerVal : teamsRaw.at(1).toArray()){
                if (playerVal.isObject() && playerVal.toObject().value("tag").toString() == queriedPlayerTag) {
                    playerInT2 = true; break;
                }
            }
        }
        if (!playerInT1 && !playerInT2) {
            missingPlayerTag++; continue;
        }


        // Determine winning/losing teams based on result and player presence
        QVector<PlayerData> winningTeamData;
        QVector<PlayerData> losingTeamData;

        if ((playerInT1 && result == "victory") || (playerInT2 && result == "defeat")) {
            winningTeamData = team1Data;
            losingTeamData = team2Data;
        } else if ((playerInT1 && result == "defeat") || (playerInT2 && result == "victory")) {
            winningTeamData = team2Data;
            losingTeamData = team1Data;
        } else if (result == "draw" || result == "draw!") { // Handle draws explicitly if necessary (skip?)
             formatIssues++; continue; // Or handle differently if draws provide info
        }
        else {
            // This case might indicate player tag missing or inconsistent result/tag data
            qWarning() << "Skipping game index" << idx << "- inconsistent result/tag:" << result << "T1?" << playerInT1 << "T2?" << playerInT2;
            formatIssues++; continue;
        }

        // Add processed game
        m_processedGames.append({mode, mapName, winningTeamData, losingTeamData});
        processedCount++;

    } // End game loop

    qInfo() << "Discovered" << m_discoveredMapModes.size() << "modes and"
            << std::accumulate(m_discoveredMapModes.begin(), m_discoveredMapModes.end(), 0,
                               [](int sum, const QSet<QString>& maps){ return sum + maps.size(); })
            << "unique maps.";
    qInfo() << "Identified" << m_allBrawlers.size() << "unique brawlers.";
    qInfo() << "Successfully processed" << processedCount << "game entries.";
    if (skippedCount > 0) qWarning() << "Skipped" << skippedCount << "games due to unexpected processing errors.";
    if (rankIssues > 0) qWarning() << "Skipped" << rankIssues << "games due to invalid player/rank data or team size.";
    if (formatIssues > 0) qWarning() << "Skipped" << formatIssues << "games due to other format issues.";
    if (missingPlayerTag > 0) qWarning() << "Skipped" << missingPlayerTag << "games because queried player tag was missing from teams.";

}

// Helper to extract team data from a QJsonValue (expected to be QJsonArray)
QPair<QVector<PlayerData>, bool> DataLoader::extractTeamData(const QJsonValue& teamValue) {
    QVector<PlayerData> teamData;
    if (!teamValue.isArray()) return {{}, false}; // Check if it's an array

    QJsonArray teamArray = teamValue.toArray();
    if (teamArray.size() != 3) return {{}, false}; // Check team size

    for (const QJsonValue& playerValue : teamArray) {
        if (!playerValue.isObject()) return {{}, false}; // Check if player entry is an object
        QJsonObject playerObj = playerValue.toObject();

        if (!playerObj.contains("brawler") || !playerObj["brawler"].isObject()) return {{}, false};
        QJsonObject brawlerInfo = playerObj["brawler"].toObject();

        if (!brawlerInfo.contains("name") || !brawlerInfo["name"].isString() ||
            !brawlerInfo.contains("rank") || !brawlerInfo["rank"].isDouble()) // rank is often numeric
        {
            return {{}, false};
        }

        QString brawlerName = brawlerInfo["name"].toString();
        int rank = brawlerInfo["rank"].toInt(); // Convert double to int

        if (brawlerName.isEmpty() || rank <= 0) {
            return {{}, false}; // Invalid name or rank
        }
        teamData.append({brawlerName, rank});
    }

    return {teamData, true}; // Success
}


// --- Getters ---
const QVector<ProcessedGame>& DataLoader::getProcessedGames() const {
    return m_processedGames;
}

const QSet<QString>& DataLoader::getAllBrawlers() const {
    return m_allBrawlers;
}

const QHash<QString, QSet<QString>>& DataLoader::getDiscoveredMapModes() const {
    return m_discoveredMapModes;
}