#include "DraftState.h"
#include <QDebug>
#include <stdexcept> // For exceptions
#include <algorithm> // For std::sort

DraftState::DraftState(QString map, QString mode, const QSet<QString>& allBrawlers,
                       QSet<QString> bans, QVector<QString> team1Picks,
                       QVector<QString> team2Picks, QString turn, int pickNumber)
    : m_map(map), m_mode(mode), m_masterBrawlerList(allBrawlers),
      m_bans(bans), m_team1Picks(team1Picks), m_team2Picks(team2Picks),
      m_turn(turn), m_pickNumber(pickNumber)
{
    // Initial calculation of available brawlers
    updateAvailable();

    // Basic validation on creation (optional, but helpful)
    if (!isValid()) {
        qWarning() << "Created potentially invalid DraftState:" << toString();
        // Optionally throw or handle differently
    }
}

QString DraftState::mapName() const { return m_map; }
QString DraftState::modeName() const { return m_mode; }
const QSet<QString>& DraftState::bans() const { return m_bans; }
const QVector<QString>& DraftState::team1Picks() const { return m_team1Picks; }
const QVector<QString>& DraftState::team2Picks() const { return m_team2Picks; }
QString DraftState::currentTurn() const { return m_turn; }
int DraftState::currentPickNumber() const { return m_pickNumber; }
const QSet<QString>& DraftState::availableBrawlers() const { return m_available; }


bool DraftState::isComplete() const {
    // Draft is complete after 6 picks (pickNumber becomes 7)
    return m_pickNumber > 6;
}

bool DraftState::isValid() const {
    // Basic sanity checks
    if (m_team1Picks.size() > 3 || m_team2Picks.size() > 3 || m_bans.size() > 6) {
        return false;
    }
    // Check for duplicate picks/bans
    QSet<QString> pickedOrBanned = m_bans;
    for(const auto& p : m_team1Picks) pickedOrBanned.insert(p);
    for(const auto& p : m_team2Picks) pickedOrBanned.insert(p);
    if (pickedOrBanned.size() != m_bans.size() + m_team1Picks.size() + m_team2Picks.size()) {
        return false; // Duplicates found
    }
    // Check if all picked/banned are in master list
    for(const auto& b : pickedOrBanned) {
        if (!m_masterBrawlerList.contains(b)) return false;
    }
    return true;
}


DraftState DraftState::applyMove(const QString& brawler) const {
    if (isComplete()) {
        throw std::logic_error("Illegal move: Draft is already complete.");
    }
    if (!m_available.contains(brawler)) {
         throw std::invalid_argument("Illegal move: Brawler '" + brawler.toStdString() + "' is not available.");
    }

    // Create copies for the new state
    QVector<QString> nextTeam1 = m_team1Picks;
    QVector<QString> nextTeam2 = m_team2Picks;
    QSet<QString> nextBans = m_bans; // Bans don't change on a pick
    int nextPickNumber = m_pickNumber + 1;
    QString nextTurn = m_turn; // Will be updated based on pick number

    // Add the pick to the correct team
    if (m_turn == "team1") {
        if (nextTeam1.size() >= 3) throw std::logic_error("Illegal move: Team 1 already has 3 picks.");
        nextTeam1.append(brawler);
    } else if (m_turn == "team2") {
         if (nextTeam2.size() >= 3) throw std::logic_error("Illegal move: Team 2 already has 3 picks.");
        nextTeam2.append(brawler);
    } else {
         throw std::logic_error("Illegal move: Invalid turn '" + m_turn.toStdString() + "'.");
    }

    // Determine the next turn based on standard draft order (1-2-2-1-1-?)
    // Pick 1 (by T1) -> T2's turn (Pick 2)
    // Pick 2 (by T2) -> T2's turn (Pick 3)
    // Pick 3 (by T2) -> T1's turn (Pick 4)
    // Pick 4 (by T1) -> T1's turn (Pick 5)
    // Pick 5 (by T1) -> T2's turn (Pick 6)
    // Pick 6 (by T2) -> Complete (Turn becomes empty/null, Pick 7)
    switch (m_pickNumber) {
        case 1: nextTurn = "team2"; break;
        case 2: nextTurn = "team2"; break; // Stays T2 for pick 3
        case 3: nextTurn = "team1"; break;
        case 4: nextTurn = "team1"; break; // Stays T1 for pick 5
        case 5: nextTurn = "team2"; break;
        case 6: nextTurn = ""; break;      // Draft complete after pick 6
        default:
             qWarning() << "Unexpected pick number in applyMove:" << m_pickNumber;
             nextTurn = ""; // Treat as complete
             break;
    }

    // Create and return the new state
    return DraftState(m_map, m_mode, m_masterBrawlerList, nextBans, nextTeam1, nextTeam2, nextTurn, nextPickNumber);
}


DraftState DraftState::applyBan(const QString& brawler) const {
     // Bans usually happen before picks, this assumes banning is allowed during the picking phase if needed
     // Or that this state represents a pre-pick ban phase. Modify logic if bans are fixed upfront.
    if (m_bans.size() >= 6) {
         throw std::logic_error("Illegal ban: Maximum number of bans (6) already reached.");
    }
     if (!m_available.contains(brawler)) { // Can only ban available brawlers
         throw std::invalid_argument("Illegal ban: Brawler '" + brawler.toStdString() + "' is not available for banning.");
     }

    QSet<QString> nextBans = m_bans;
    nextBans.insert(brawler);

    // Ban does not advance pick number or change turn in this model
    return DraftState(m_map, m_mode, m_masterBrawlerList, nextBans, m_team1Picks, m_team2Picks, m_turn, m_pickNumber);
}


QVector<QString> DraftState::getLegalMoves() const {
    if (isComplete()) {
        return {}; // No moves if complete
    }
    // Legal moves are simply the currently available brawlers
    QVector<QString> legal = QVector<QString>::fromList(m_available.values()); // Convert QSet to QVector
    std::sort(legal.begin(), legal.end()); // Sort alphabetically for consistency
    return legal;
}


QString DraftState::toString() const {
    QString t1Str = m_team1Picks.join(", ");
    QString t2Str = m_team2Picks.join(", ");
    QList<QString> banList = m_bans.values(); // Get list from set
    std::sort(banList.begin(), banList.end()); // Sort for consistent output
    QString banStr = QStringList(banList).join(", "); // Use QStringList helper

    return QString("DraftState(Map: %1, Mode: %2, T1: [%3], T2: [%4], Bans: {%5}, Turn: %6, Pick: %7, Avail: %8)")
        .arg(m_map).arg(m_mode).arg(t1Str).arg(t2Str).arg(banStr)
        .arg(m_turn.isEmpty() ? "Complete" : m_turn)
        .arg(m_pickNumber)
        .arg(m_available.size());
}


void DraftState::updateAvailable() {
    m_available = m_masterBrawlerList;
    m_available -= m_bans; // Remove banned brawlers
    for (const auto& p : m_team1Picks) m_available.remove(p);
    for (const auto& p : m_team2Picks) m_available.remove(p);
}

// uint qHash(const DraftState &key, uint seed) {
//     // Implement hashing if needed for QHash keys
//     // Combine hashes of members like map, mode, picks, bans, turn, pickNumber
//     // Example: return qHash(key.mapName(), seed) ^ qHash(...) ...;
//     return 0; // Placeholder
// }