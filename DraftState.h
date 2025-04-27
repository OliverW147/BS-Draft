#ifndef DRAFTSTATE_H
#define DRAFTSTATE_H

#include <QString>
#include <QVector>
#include <QSet>
#include <QSharedPointer> // For potential use if state needs shared ownership

#include "DataStructures.h" // Not directly needed, but good practice

class DraftState {
public:
    // Constructor
    DraftState(QString map, QString mode, const QSet<QString>& allBrawlers,
               QSet<QString> bans = {},
               QVector<QString> team1Picks = {},
               QVector<QString> team2Picks = {},
               QString turn = "team1",
               int pickNumber = 1);

    // State properties
    QString mapName() const;
    QString modeName() const;
    const QSet<QString>& bans() const;
    const QVector<QString>& team1Picks() const;
    const QVector<QString>& team2Picks() const;
    QString currentTurn() const;
    int currentPickNumber() const;
    const QSet<QString>& availableBrawlers() const; // Brawlers not picked or banned

    // State checks
    bool isComplete() const;
    bool isValid() const; // Basic check for team sizes etc.

    // Actions (return a *new* state)
    DraftState applyMove(const QString& brawler) const;
    DraftState applyBan(const QString& brawler) const;

    // Get possible actions
    QVector<QString> getLegalMoves() const; // Returns available brawlers sorted

    // String representation for debugging
    QString toString() const;

private:
    QString m_map;
    QString m_mode;
    QSet<QString> m_masterBrawlerList;
    QSet<QString> m_bans;
    QVector<QString> m_team1Picks;
    QVector<QString> m_team2Picks;
    QString m_turn; // "team1", "team2", or "" (empty/null if complete)
    int m_pickNumber; // 1-based index of the pick *about* to be made
    QSet<QString> m_available;

    void updateAvailable(); // Helper to recalculate available brawlers
};

// Define QHash hash function for DraftState if you need to use it as a key
// uint qHash(const DraftState &key, uint seed = 0);

#endif // DRAFTSTATE_H