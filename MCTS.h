#ifndef MCTS_H
#define MCTS_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QFuture>
#include <QMutex>
#include <QThreadPool> // <-- ADD
#include <atomic>
#include <memory>
#include <random>

#include "DataStructures.h"
#include "DraftState.h"
#include "StatsCalculator.h"
#include "AppConfig.h"
#include "Heuristics.h"

class MCTSNode;

class MCTSNode : public std::enable_shared_from_this<MCTSNode> {
public:
    DraftState state;
    std::weak_ptr<MCTSNode> parent;
    QString move;
    QVector<std::shared_ptr<MCTSNode>> children;
    std::atomic<double> wins{0.0};
    std::atomic<int> visits{0};
    QVector<QString> untriedMoves;
    std::atomic<bool> isTerminal{false};
    QMutex mutex; // Protects untriedMoves and children during expansion

    MCTSNode(DraftState s, std::shared_ptr<MCTSNode> p = nullptr, QString m = "");

    bool isFullyExpanded();
    // uctSelectChild needs the engine for random tie-breaking/fallback
    std::shared_ptr<MCTSNode> uctSelectChild(double explorationParam, std::mt19937& randomEngine);
    // expand needs the engine if random move selection is used (currently takes last)
    std::shared_ptr<MCTSNode> expand(/*std::mt19937& randomEngine*/); // Engine not needed if just taking last
    void update(double result);
};


class MCTSManager : public QObject {
    Q_OBJECT

public:
    MCTSManager(const StatsCalculator& statsCalculator, const AppConfig& config, QObject *parent = nullptr);
    ~MCTSManager();

    bool isRunning() const; // Checks if the controller task is running

public slots:
    void startMcts(DraftState rootState, HeuristicWeights weights);
    void stopMcts();

signals:
    void mctsStatusUpdate(const QString& status);
    void mctsIntermediateResult(const QVector<MCTSResult>& results);
    void mctsFinalResult(const QVector<MCTSResult>& results);
    void mctsError(const QString& errorMsg);
    void mctsFinished();

private:
    // Renamed: This is now the controller task managing time/reporting
    void runMctsControllerTask(std::shared_ptr<MCTSNode> rootNode, HeuristicWeights weights);
    // New: Represents the work done by ONE iteration in a worker thread
    void runSingleMctsIteration(std::shared_ptr<MCTSNode> rootNode, const HeuristicWeights& weights, double explorationParam, std::mt19937& randomEngine);

    QVector<MCTSResult> getMctsResults(std::shared_ptr<MCTSNode> rootNode) const;
    // simulateRollout now needs the engine reference again
    double simulateRollout(DraftState currentState, const HeuristicWeights& weights, std::mt19937& randomEngine) const;

    const StatsCalculator& m_statsCalculator;
    const AppConfig& m_config;

    QThreadPool m_threadPool; // Manages worker threads
    QFuture<void> m_controllerFuture; // Tracks the controller task
    std::atomic<bool> m_stopRequested{false};
    std::atomic<long long> m_totalIterationsDone{0}; // Counter across threads

    // Remove m_randomEngine; workers use their own
};

#endif // MCTS_H