#ifndef DATALOADER_H
#define DATALOADER_H

#include <QString>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QPair>
#include <QJsonObject>  // <-- ADD
#include <QJsonArray>   // <-- ADD
#include <QJsonValue>   // <-- ADD (Used in extractTeamData signature)
#include "DataStructures.h"
#include "AppConfig.h"

class DataLoader {
public:
    DataLoader(QString filepath, const AppConfig& config);

    bool loadAndProcess();

    const QVector<ProcessedGame>& getProcessedGames() const;
    const QSet<QString>& getAllBrawlers() const;
    const QHash<QString, QSet<QString>>& getDiscoveredMapModes() const;

private:
    bool loadRawData();
    void preprocessData();
    QPair<QVector<PlayerData>, bool> extractTeamData(const QJsonValue& teamValue); // Use QJsonValue

    QString m_filepath;
    const AppConfig& m_config; // Store reference to config

    QVector<QJsonObject> m_rawGames; // Store raw JSON objects initially
    QVector<ProcessedGame> m_processedGames;
    QSet<QString> m_allBrawlers;
    QHash<QString, QSet<QString>> m_discoveredMapModes;
};

#endif // DATALOADER_H