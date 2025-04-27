#include "MCTS.h"
#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>
#include <QThread> // For msleep and idealThreadCount
#include <QDebug>
#include <cmath>
#include <limits>
#include <algorithm>
#include <random>
#include <functional> // For std::ref used with QtConcurrent with members
#include "DataStructures.h"


// --- MCTSNode Implementation ---

MCTSNode::MCTSNode(DraftState s, std::shared_ptr<MCTSNode> p, QString m)
    : state(std::move(s)), parent(p), move(std::move(m))
{
    isTerminal = state.isComplete();
    if (!isTerminal) {
        untriedMoves = state.getLegalMoves();
        // Optional shuffling could happen here using an engine if needed at creation
    }
}

bool MCTSNode::isFullyExpanded() {
    QMutexLocker locker(&mutex);
    return untriedMoves.isEmpty();
}

std::shared_ptr<MCTSNode> MCTSNode::uctSelectChild(double explorationParam, std::mt19937& randomEngine) {
    // Selection doesn't modify the node structure (children list), only reads visits/wins.
    // Reads on atomics are safe without external locks.
    // Mutex might only be needed if children *vector itself* could be modified,
    // but expansion is guarded separately. Let's assume reads are fine here.

    if (children.isEmpty()) {
        return nullptr;
    }

    std::shared_ptr<MCTSNode> bestChild = nullptr;
    double bestScore = -std::numeric_limits<double>::infinity();
    int parentVisits = visits.load(std::memory_order_relaxed); // Relaxed is ok for reads

    if (parentVisits == 0) {
        if (children.isEmpty()) return nullptr;
        // Use the PASSED engine for tie-breaking
        std::uniform_int_distribution<qsizetype> dist(0, children.size() - 1);
        return children.at(dist(randomEngine));
    }

    double logParentVisits = log(static_cast<double>(parentVisits));

    // Accessing children vector itself should be safe if expansion is properly locked
    for (const auto& child : children) {
        double score = 0.0;
        int childVisits = child->visits.load(std::memory_order_relaxed);

        if (childVisits == 0) {
            score = std::numeric_limits<double>::infinity();
        } else {
            double winRate = child->wins.load(std::memory_order_relaxed) / childVisits;
            // Prevent log of zero or negative - ensure childVisits > 0 was checked
            if (childVisits <= 0) continue; // Should not happen if visits > 0 check works
            double exploration = explorationParam * sqrt(logParentVisits / childVisits);
            score = winRate + exploration;
        }

        if (score > bestScore) {
            bestScore = score;
            bestChild = child;
        }
    }

    if (!bestChild && !children.isEmpty()) {
        qWarning() << "UCT selection failed, returning random.";
        std::uniform_int_distribution<qsizetype> dist(0, children.size() - 1);
        return children.at(dist(randomEngine)); // Use PASSED engine
    }

    return bestChild;
}

// expand doesn't need the engine if we just take the last move
std::shared_ptr<MCTSNode> MCTSNode::expand(/*std::mt19937& randomEngine*/) {
    QMutexLocker locker(&mutex); // Lock untriedMoves and children modification

    if (untriedMoves.isEmpty()) {
        return nullptr;
    }

    // --- Take last move (no randomness) ---
    QString moveToTry = untriedMoves.takeLast();

    // --- Optional: Random move selection ---
    // if (untriedMoves.isEmpty()) return nullptr;
    // std::uniform_int_distribution<qsizetype> dist(0, untriedMoves.size() - 1);
    // qsizetype index = dist(randomEngine); // Use engine if selecting randomly
    // QString moveToTry = untriedMoves.takeAt(index);

    try {
        DraftState nextState = state.applyMove(moveToTry);
        // Use shared_from_this() which is safe now due to inheritance
        auto newNode = std::make_shared<MCTSNode>(nextState, shared_from_this(), moveToTry);
        children.append(newNode); // Append is thread-safe for QVector if only one thread appends *after locking*
        return newNode;
    } catch (const std::exception& e) {
        qCritical() << "MCTS Expansion Error applying move" << moveToTry << ":" << e.what() << "State:" << state.toString();
        return nullptr;
    } catch (...) {
        qCritical() << "MCTS Expansion Error applying move" << moveToTry << ": Unknown exception. State:" << state.toString();
        return nullptr;
    }
}

void MCTSNode::update(double result) {
    // Update visits atomically
    visits.fetch_add(1, std::memory_order_relaxed); // Relaxed is fine for counters
    // Update wins using the compare-and-swap helper
    atomic_add_double(wins, result);
}


// --- MCTSManager Implementation ---

MCTSManager::MCTSManager(const StatsCalculator& statsCalculator, const AppConfig& config, QObject *parent)
    : QObject(parent),
      m_statsCalculator(statsCalculator),
      m_config(config)
{
    // Set max threads for the pool (can be adjusted)
    m_threadPool.setMaxThreadCount(QThread::idealThreadCount());
    qInfo() << "MCTSManager using thread pool with max" << m_threadPool.maxThreadCount() << "threads.";
}

MCTSManager::~MCTSManager() {
    stopMcts(); // Request stop
    // Wait for controller task to finish
    if (m_controllerFuture.isRunning()) {
        m_controllerFuture.waitForFinished();
    }
    // Wait for worker threads to finish (optional, pool manages them)
    // m_threadPool.waitForDone(); // Can block if called from main thread with active workers
}

bool MCTSManager::isRunning() const {
    // Check if the controller task is running
    return m_controllerFuture.isRunning();
}

void MCTSManager::startMcts(DraftState rootState, HeuristicWeights weights) {
    if (isRunning()) {
        qWarning() << "MCTS is already running.";
        emit mctsError("MCTS already running.");
        return;
    }
    if (rootState.isComplete() || rootState.getLegalMoves().isEmpty()) {
        qInfo() << "MCTS not started: Root state terminal or no legal moves.";
        emit mctsFinalResult({});
        emit mctsFinished();
        return;
    }

    // Reset state variables
    m_stopRequested = false;
    m_totalIterationsDone = 0;

    // Create the shared root node
    auto rootNode = std::make_shared<MCTSNode>(rootState);

    int numThreads = m_threadPool.maxThreadCount(); // Use configured max threads
    qInfo() << "Starting MCTS with" << numThreads << "worker threads.";

    // Store needed parameters accessible by workers (capture list or members)
    double explorationParam = m_config.mctsExplorationParam();

    // Launch Worker Threads via Thread Pool
    for (int i = 0; i < numThreads; ++i) {
        // Use pool's start() with a lambda
        m_threadPool.start([this, rootNode, weights, explorationParam, i]() {
            // Each worker thread gets its own random engine, seeded uniquely
            std::mt19937 threadRandomEngine(std::random_device{}() + i); // Simple unique seeding

            try {
                 // Worker loop: continues as long as stop is not requested
                while (!m_stopRequested.load(std::memory_order_relaxed)) {
                    runSingleMctsIteration(rootNode, weights, explorationParam, threadRandomEngine);
                    // Increment shared iteration counter atomically
                    m_totalIterationsDone.fetch_add(1, std::memory_order_relaxed);
                }
            } catch (const std::exception& e) {
                 qCritical() << "Exception in MCTS worker thread" << i << ":" << e.what();
                 // Optionally signal an error from here? Might be tricky with multiple threads.
                 // Stop might be triggered by controller anyway.
            } catch (...) {
                qCritical() << "Unknown exception in MCTS worker thread" << i;
            }
             //qDebug() << "MCTS Worker thread" << i << "finished.";
        });
    }

    // Launch the Controller Task in a separate thread
    // Pass rootNode by value (shared_ptr copy), weights by value.
    m_controllerFuture = QtConcurrent::run([this, rootNode, weights]() {
        this->runMctsControllerTask(rootNode, weights);
    });

    qInfo() << "MCTS controller and worker threads launched for state:" << rootState.toString();
    emit mctsStatusUpdate("MCTS Started...");
}

void MCTSManager::stopMcts() {
    if (!m_stopRequested.load()) { // Only signal stop once
        qInfo() << "Signaling MCTS threads to stop...";
        m_stopRequested = true;
        // Workers and controller will check this flag and exit their loops.
        // Optionally wait for controller future here if needed immediately,
        // but destructor handles waiting.
    }
}

// New function: Performs one MCTS iteration (Select, Expand, Simulate, Backprop)
// This is the core logic executed by each worker thread.
void MCTSManager::runSingleMctsIteration(std::shared_ptr<MCTSNode> rootNode, const HeuristicWeights& weights, double explorationParam, std::mt19937& randomEngine)
{
    // 1. Selection
    std::shared_ptr<MCTSNode> node = rootNode;
    while (!node->isTerminal.load() && node->isFullyExpanded()) {
        auto selectedChild = node->uctSelectChild(explorationParam, randomEngine); // Pass worker's engine
        if (!selectedChild) {
            // This can happen if selection fails concurrently, maybe retry or log warning
            qWarning() << "MCTS Selection returned null despite node being fully expanded. Retrying selection from root.";
             // Simple recovery: Restart selection from root for this iteration
             // A more complex strategy might be needed for high contention.
            node = rootNode;
            // Or just return and skip this iteration for this worker?
            // return;
            continue; // Retry selection loop
        }
        node = selectedChild;
    }

    // 2. Expansion
    // Check terminal state *after* selection loop completes
    if (!node->isTerminal.load()) {
         // expand() handles internal locking
         std::shared_ptr<MCTSNode> expandedNode = node->expand(/*randomEngine*/); // Engine not needed for takeLast()
         if (expandedNode) {
             node = expandedNode; // Rollout from the newly expanded node
         }
         // If expansion failed (returned nullptr, e.g., concurrent expansion finished first),
         // 'node' remains the parent node, rollout happens from there.
    }

    // 3. Simulation
    // simulateRollout needs the worker's random engine
    double result = simulateRollout(node->state, weights, randomEngine); // Result is win prob for T1

    // 4. Backpropagation
    std::shared_ptr<MCTSNode> tempNode = node;
    std::shared_ptr<MCTSNode> rootShared = rootNode; // Need shared_ptr for comparison

    while (tempNode != nullptr) {
        QString parentTurn;
        std::shared_ptr<MCTSNode> parentPtr = tempNode->parent.lock();

        if (parentPtr) {
             parentTurn = parentPtr->state.currentTurn();
        } else if (tempNode == rootShared) {
             // Special case for root node: assume parent turn matches root's turn
             // Or determine based on who would make the first move *to* the root (usually player 1)
             parentTurn = rootShared->state.currentTurn(); // Check this logic - who gets the score at root?
             // Backprop score should be from the perspective of the player *making the move*
             // If root state is T1's turn, the score should reflect T1's win chance
        } else {
             // Parent weak_ptr expired, stop backpropagation
             break;
        }


        // 'result' = win prob for T1. resultForNode = score for the player whose turn it was at parentPtr.
        double resultForNode = (parentTurn == "team1") ? result : (1.0 - result);

        tempNode->update(resultForNode); // atomic updates inside

        // Move up the tree
        tempNode = parentPtr; // Continue with the locked parent pointer
    }
}


// Renamed: This now ONLY controls timing and reporting, doesn't run iterations itself.
void MCTSManager::runMctsControllerTask(std::shared_ptr<MCTSNode> rootNode, HeuristicWeights weights) {
    try {
        QElapsedTimer timer;
        timer.start();
        long long lastIterationCount = 0;
        double timeLimitMs = m_config.mctsTimeLimit() * 1000.0;
        int reportIntervalMs = 200; // How often to check status/emit reports
        int intermediateResultIntervalMs = m_config.mctsUpdateIntervalIters() > 0 ? 1000 : 0; // Approx interval for intermediate results (e.g., 1 sec)
        qint64 nextIntermediateResultTime = intermediateResultIntervalMs > 0 ? timer.elapsed() + intermediateResultIntervalMs : -1;

        qInfo() << "MCTS Controller Task Started.";

        // Controller loop
        while (!m_stopRequested.load(std::memory_order_relaxed)) {
            qint64 elapsed = timer.elapsed();

            // Check time limit
            if (elapsed >= timeLimitMs) {
                qInfo() << "MCTS time limit (" << m_config.mctsTimeLimit() << "s) reached by controller.";
                emit mctsStatusUpdate("MCTS Time Limit Reached");
                stopMcts(); // Signal workers to stop
                break; // Exit controller loop
            }

            // Emit status update periodically
            long long currentIterations = m_totalIterationsDone.load(std::memory_order_relaxed);
            // Only emit if count changed or first time? Avoid spamming if stalled.
            //if (currentIterations != lastIterationCount) { // Check if iterations increased
                 emit mctsStatusUpdate(QString("Running MCTS: %1 iter (%2s / %3s)")
                                       .arg(currentIterations)
                                       .arg(elapsed / 1000.0, 0, 'f', 1)
                                       .arg(m_config.mctsTimeLimit(), 0, 'f', 1));
                 lastIterationCount = currentIterations;
            //}


            // Emit intermediate results periodically (based on time now)
            if (intermediateResultIntervalMs > 0 && elapsed >= nextIntermediateResultTime) {
                QVector<MCTSResult> intermediate = getMctsResults(rootNode);
                emit mctsIntermediateResult(intermediate);
                nextIntermediateResultTime = elapsed + intermediateResultIntervalMs; // Schedule next report
            }


            // Sleep briefly to avoid busy-waiting
            QThread::msleep(reportIntervalMs); // Check every ~200ms

        } // End controller loop

        // --- MCTS Stopped (Time Limit or External Request) ---
        if (m_stopRequested.load() && timer.elapsed() < timeLimitMs) {
             qInfo() << "MCTS Controller received stop signal.";
             emit mctsStatusUpdate("MCTS Stopped Early");
        }

        qInfo() << "MCTS Controller task finishing. Total iterations:" << m_totalIterationsDone.load();

        // Wait briefly for worker threads to potentially finish their current iteration after stop signal
        // This is optional and might not be strictly necessary.
        // QThread::msleep(50); // Small delay

        // Get and emit final results
        QVector<MCTSResult> finalResults = getMctsResults(rootNode);
        emit mctsFinalResult(finalResults);


    } catch (const std::exception& e) {
        qCritical() << "Unhandled exception in MCTS controller thread:" << e.what();
        emit mctsError(QString("MCTS Controller Error: %1").arg(e.what()));
        stopMcts(); // Ensure stop is signaled on error
    } catch (...) {
        qCritical() << "Unknown unhandled exception in MCTS controller thread.";
        emit mctsError("Unknown MCTS Controller Error");
        stopMcts(); // Ensure stop is signaled on error
    }

    // Signal overall completion
    emit mctsFinished();
    qInfo() << "MCTS Controller Task Finished.";
}


// Simulate a game rollout using heuristics (Needs engine reference)
double MCTSManager::simulateRollout(DraftState currentState, const HeuristicWeights& weights, std::mt19937& randomEngine) const {
    DraftState rolloutState = currentState; // Copy for simulation

    while (!rolloutState.isComplete()) {
        QVector<QString> possibleMoves = rolloutState.getLegalMoves();
        if (possibleMoves.isEmpty()) {
            qWarning() << "Rollout reached non-terminal state with no legal moves:" << rolloutState.toString();
            break;
        }

        auto [heuristicMove, scores] = suggestPickHeuristic(rolloutState, m_statsCalculator, weights);
        QString move;

        if (!heuristicMove.isEmpty() && possibleMoves.contains(heuristicMove)) {
            move = heuristicMove;
        } else {
            // Use the PASSED worker's engine for fallback
            std::uniform_int_distribution<qsizetype> dist(0, possibleMoves.size() - 1);
            move = possibleMoves[dist(randomEngine)];
        }

        try {
            rolloutState = rolloutState.applyMove(move);
        } catch (const std::exception& e) {
            qCritical() << "MCTS Rollout Error applying move" << move << ":" << e.what() << "State:" << rolloutState.toString();
            break;
        }
    }

    // Evaluate final state
    double winProbTeam1 = 0.5;
    if (rolloutState.isComplete()) {
        try {
            winProbTeam1 = predictWinProbabilityModel(
                rolloutState.team1Picks(), rolloutState.team2Picks(),
                rolloutState.mapName(), rolloutState.modeName(),
                m_statsCalculator, weights);
        } catch (const std::exception& e) {
            qCritical() << "Error during MCTS final evaluation:" << e.what();
            winProbTeam1 = 0.5;
        }
    } else {
        qWarning() << "Rollout did not complete. Evaluating intermediate state as 0.5.";
        winProbTeam1 = 0.5;
    }
    return winProbTeam1;
}


// Extracts the results (top moves) from the root node's children
QVector<MCTSResult> MCTSManager::getMctsResults(std::shared_ptr<MCTSNode> rootNode) const {
    QVector<MCTSResult> results;
    if (!rootNode || rootNode->children.isEmpty()) {
        return results;
    }

    // Reading children vector and atomic stats should be safe concurrently
    // However, if children could be ADDED during this time (they shouldn't if expansion locks), a lock might be needed.
    // Let's assume expansion lock prevents concurrent modification of the children vector *structure*.

    results.reserve(rootNode->children.size());

    for (const auto& child : rootNode->children) {
        int childVisits = child->visits.load(std::memory_order_relaxed);
        if (childVisits > 0) {
            double childWins = child->wins.load(std::memory_order_relaxed);
            // Prevent division by zero just in case
            double winRate = (childVisits > 0) ? (childWins / childVisits) : 0.0;
            results.append(MCTSResult(child->move, childVisits, winRate));
        }
    }

    // Sort results
    std::sort(results.begin(), results.end(), [](const MCTSResult& a, const MCTSResult& b) {
        if (a.visits != b.visits) return a.visits > b.visits;
        return a.winRate > b.winRate;
    });

    // Limit results
    int limit = m_config.mctsResultCount();
    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}