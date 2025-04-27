#include "MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QStatusBar>
#include <QMessageBox>
#include <QGroupBox>
// #include <QDoubleSpinBox> // Removed
#include <QFormLayout>
#include <QCloseEvent>
#include <QDebug>
#include <algorithm>
#include <limits>
#include <QCoreApplication> // Include for processEvents


// Constructor (no changes needed here unless dependencies changed)
MainWindow::MainWindow(const StatsCalculator& statsCalculator,
                       const QSet<QString>& allBrawlers,
                       const QHash<QString, QSet<QString>>& mapModeData,
                       AppConfig& config,
                       MCTSManager* mctsManager,
                       QWidget *parent)
    : QMainWindow(parent),
      m_statsCalculator(statsCalculator),
      m_allBrawlersMasterList(allBrawlers),
      m_mapModeData(mapModeData),
      m_config(config),
      m_mctsManager(mctsManager)
{
    setWindowTitle("Glizzy Draft");
    setWindowIcon(QIcon(":/icon.ico"));

    setupUi();
    setupConnections();
    loadInitialData();
    updateUiFromState();
    setStatus("Status: Select Mode and Map to start.");
}

MainWindow::~MainWindow() {}

// Create and layout UI elements
void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // --- 1. Control Frame ---
    QGroupBox *controlGroup = new QGroupBox("Draft Setup");
    QHBoxLayout *controlLayout = new QHBoxLayout();
    m_modeComboBox = new QComboBox();
    m_mapComboBox = new QComboBox();
    m_mctsTimeLineEdit = new QLineEdit(QString::number(m_config.mctsTimeLimit()));
    m_mctsTimeLineEdit->setValidator(new QDoubleValidator(0.1, 600.0, 1, this));
    m_mctsTimeLineEdit->setFixedWidth(50);
    m_resetButton = new QPushButton("Reset Draft");

    controlLayout->addWidget(new QLabel("Mode:"));
    controlLayout->addWidget(m_modeComboBox);
    controlLayout->addWidget(new QLabel("Map:"));
    controlLayout->addWidget(m_mapComboBox);
    controlLayout->addStretch(1);
    controlLayout->addWidget(new QLabel("MCTS Time (s):"));
    controlLayout->addWidget(m_mctsTimeLineEdit);
    controlLayout->addWidget(m_resetButton);
    controlGroup->setLayout(controlLayout);
    mainLayout->addWidget(controlGroup);

    // --- 2. Weights Frame --- REMOVED ---
    // QGroupBox *weightsGroup = new QGroupBox("Heuristic Weights");
    // QFormLayout *weightsLayout = new QFormLayout();
    // ... creation of spin boxes ...
    // weightsGroup->setLayout(weightsLayout);
    // mainLayout->addWidget(weightsGroup); // <-- REMOVED


    // --- 3. Display Frame (Drafting Area) ---
    QGroupBox *displayGroup = new QGroupBox("Draft State");
    QGridLayout *displayLayout = new QGridLayout();

    // Col 0: Available Brawlers
    m_searchLineEdit = new QLineEdit();
    m_searchLineEdit->setPlaceholderText("Search Available...");
    m_availableListWidget = new QListWidget();
    displayLayout->addWidget(new QLabel("Available Brawlers:"), 0, 0); // Row 0
    displayLayout->addWidget(m_searchLineEdit, 1, 0);                 // Row 1
    displayLayout->addWidget(m_availableListWidget, 2, 0, 6, 1);      // Row 2, Span 6 rows

    // Col 1: Action Buttons
    QVBoxLayout *buttonLayout = new QVBoxLayout();
    m_pickT1Button = new QPushButton("Pick T1 ->");
    m_pickT2Button = new QPushButton("Pick T2 ->");
    m_banButton = new QPushButton("Ban ->");
    m_unbanButton = new QPushButton("<- Unban");
    m_undoPickButton = new QPushButton("<- Undo Pick");
    buttonLayout->addWidget(m_pickT1Button);
    buttonLayout->addWidget(m_pickT2Button);
    buttonLayout->addWidget(m_banButton);
    buttonLayout->addWidget(m_unbanButton);
    buttonLayout->addWidget(m_undoPickButton);
    buttonLayout->addStretch(1);
    displayLayout->addLayout(buttonLayout, 2, 1, 6, 1); // Row 2, Span 6 rows

    // Col 2: Teams and Bans - **Layout Adjusted**
    m_turnLabel = new QLabel("Turn: -"); m_turnLabel->setStyleSheet("font-weight: bold;");
    m_pickNumLabel = new QLabel("Pick #: -");
    QHBoxLayout* turnPickLayout = new QHBoxLayout();
    turnPickLayout->addWidget(m_turnLabel);
    turnPickLayout->addWidget(m_pickNumLabel);
    turnPickLayout->addStretch(1);

    m_team1ListWidget = new QListWidget(); m_team1ListWidget->setFixedHeight(60);
    m_team2ListWidget = new QListWidget(); m_team2ListWidget->setFixedHeight(60);
    m_bansListWidget = new QListWidget(); // Let it take remaining space

    // Row-by-row placement for clearer separation
    displayLayout->addLayout(turnPickLayout, 0, 2);        // Row 0: Turn/Pick #
    displayLayout->addWidget(new QLabel("Team 1 Picks:"), 1, 2); // Row 1: T1 Label
    displayLayout->addWidget(m_team1ListWidget, 2, 2);     // Row 2: T1 List
    displayLayout->addWidget(new QLabel("Team 2 Picks:"), 3, 2); // Row 3: T2 Label
    displayLayout->addWidget(m_team2ListWidget, 4, 2);     // Row 4: T2 List
    displayLayout->addWidget(new QLabel("Bans:"), 5, 2);         // Row 5: Bans Label
    displayLayout->addWidget(m_bansListWidget, 6, 2, 2, 1);    // Row 6: Bans List (Span 2 rows to fill remaining space)

    // Set column stretch factors
    displayLayout->setColumnStretch(0, 2); // Available list wider
    displayLayout->setColumnStretch(1, 0); // Buttons fixed width
    displayLayout->setColumnStretch(2, 2); // Teams/Bans wider
    // Set row stretch factor for the bans list row to take up space
    displayLayout->setRowStretch(6, 1);

    displayGroup->setLayout(displayLayout);
    mainLayout->addWidget(displayGroup);


    // --- 4. Suggestion Frame --- (No changes here)
    QGroupBox *suggestionGroup = new QGroupBox("Suggestions & Analysis");
    QGridLayout *suggestionLayout = new QGridLayout();

    m_suggestHeuristicButton = new QPushButton("Suggest Pick (Fast)");
    m_suggestMctsButton = new QPushButton("Suggest Pick (Deep)");
    m_suggestBanButton = new QPushButton("Suggest Ban");
    m_stopMctsButton = new QPushButton("Stop MCTS"); m_stopMctsButton->setEnabled(false);

    suggestionLayout->addWidget(m_suggestHeuristicButton, 0, 0);
    suggestionLayout->addWidget(m_suggestMctsButton, 0, 1);
    suggestionLayout->addWidget(m_suggestBanButton, 0, 2);
    suggestionLayout->addWidget(m_stopMctsButton, 0, 3);

    m_suggestionLabel = new QLabel("Suggestion: -");
    m_suggestionLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    m_suggestionLabel->setWordWrap(true);
    suggestionLayout->addWidget(m_suggestionLabel, 1, 0, 1, 4);

    m_scoresTitleLabel = new QLabel("Details:");
    suggestionLayout->addWidget(m_scoresTitleLabel, 2, 0, 1, 4);

    m_scoresTextEdit = new QTextEdit();
    m_scoresTextEdit->setReadOnly(true);
    m_scoresTextEdit->setLineWrapMode(QTextEdit::NoWrap);
    suggestionLayout->addWidget(m_scoresTextEdit, 3, 0, 1, 4);

    suggestionGroup->setLayout(suggestionLayout);
    mainLayout->addWidget(suggestionGroup);


    // --- 5. Status Bar --- (No changes here)
    m_statusLabel = new QLabel("Status: Initializing...");
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(new QLabel("Made by Texesh"));


    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
    resize(800, 900);
}

// Connect signals to slots (No changes needed related to weights)
void MainWindow::setupConnections() {
    // Control Frame
    connect(m_modeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onModeChanged(int)));
    connect(m_mapComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onMapChanged(int)));
    connect(m_resetButton, &QPushButton::clicked, this, &MainWindow::onResetDraftClicked);
    connect(m_mctsTimeLineEdit, &QLineEdit::editingFinished, this, &MainWindow::validateMctsTimeInput);

    // Display Frame (Drafting Area)
    connect(m_searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(m_availableListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::onAvailableListDoubleClicked);
    connect(m_bansListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::onBansListDoubleClicked);
    connect(m_pickT1Button, &QPushButton::clicked, this, &MainWindow::onPickTeam1Clicked);
    connect(m_pickT2Button, &QPushButton::clicked, this, &MainWindow::onPickTeam2Clicked);
    connect(m_banButton, &QPushButton::clicked, this, &MainWindow::onBanClicked);
    connect(m_unbanButton, &QPushButton::clicked, this, &MainWindow::onUnbanClicked);
    connect(m_undoPickButton, &QPushButton::clicked, this, &MainWindow::onUndoPickClicked);


    // Suggestion Frame
    connect(m_suggestHeuristicButton, &QPushButton::clicked, this, &MainWindow::onSuggestHeuristicClicked);
    connect(m_suggestMctsButton, &QPushButton::clicked, this, &MainWindow::onSuggestMctsClicked);
    connect(m_suggestBanButton, &QPushButton::clicked, this, &MainWindow::onSuggestBanClicked);
    connect(m_stopMctsButton, &QPushButton::clicked, this, &MainWindow::onStopMctsClicked);

    // MCTS Manager Signals -> MainWindow Slots
    connect(m_mctsManager, &MCTSManager::mctsStatusUpdate, this, &MainWindow::handleMctsStatus);
    connect(m_mctsManager, &MCTSManager::mctsIntermediateResult, this, &MainWindow::handleMctsIntermediateResult);
    connect(m_mctsManager, &MCTSManager::mctsFinalResult, this, &MainWindow::handleMctsFinalResult);
    connect(m_mctsManager, &MCTSManager::mctsError, this, &MainWindow::handleMctsError);
    connect(m_mctsManager, &MCTSManager::mctsFinished, this, &MainWindow::handleMctsFinished);
}

// Populate initial dropdown data (No changes needed)
void MainWindow::loadInitialData() {
    m_modeComboBox->clear();
    QStringList modes = m_mapModeData.keys();
    std::sort(modes.begin(), modes.end());
    m_modeComboBox->addItems(modes);

    if (!modes.isEmpty()) {
        m_modeComboBox->setCurrentIndex(0);
        onModeChanged(0);
    } else {
         setStatus("FATAL: No modes found!", true);
         setControlsEnabled(false);
    }
}

// --- Slot Implementations ---

// onModeChanged, onMapChanged, onResetDraftClicked, validateMctsTimeInput (No changes needed)
void MainWindow::onModeChanged(int index) {
    if (index < 0) return;
    QString selectedMode = m_modeComboBox->itemText(index);

    m_mapComboBox->clear();
    if (m_mapModeData.contains(selectedMode)) {
        QSet<QString> mapsSet = m_mapModeData.value(selectedMode);
        QStringList mapsList = mapsSet.values();
        std::sort(mapsList.begin(), mapsList.end());
        m_mapComboBox->addItems(mapsList);
        if (!mapsList.isEmpty()) {
            m_mapComboBox->setCurrentIndex(0);
            onMapChanged(0);
        } else {
             setStatus("No maps found for selected mode.", true);
             m_currentDraftState.reset();
             updateUiFromState();
        }
    } else {
         setStatus("Selected mode not found in data (internal error).", true);
          m_currentDraftState.reset();
         updateUiFromState();
    }
}

void MainWindow::onMapChanged(int index) {
    if (index >= 0 && m_mapComboBox->count() > 0) {
        initializeDraft();
    } else {
        m_currentDraftState.reset();
        updateUiFromState();
        setStatus("Select Mode and Map.");
    }
}

void MainWindow::onResetDraftClicked() {
    initializeDraft();
}

void MainWindow::validateMctsTimeInput() {
    bool ok;
    double timeVal = m_mctsTimeLineEdit->text().toDouble(&ok);
    if (!ok || timeVal <= 0) {
        QMessageBox::warning(this, "Invalid Input", "MCTS Time must be a positive number.");
        m_mctsTimeLineEdit->setText(QString::number(m_config.mctsTimeLimit()));
    } else {
        m_config.setMctsTimeLimit(timeVal);
    }
}


void MainWindow::initializeDraft() {
    if (m_mctsManager->isRunning()) {
        QMessageBox::warning(this, "MCTS Running", "Stop MCTS before starting a new draft.");
        return;
    }

    QString mode = m_modeComboBox->currentText();
    QString map = m_mapComboBox->currentText();

    if (mode.isEmpty() || map.isEmpty()) {
        setStatus("Select Mode and Map first.", true);
        m_currentDraftState.reset();
        updateUiFromState();
        return;
    }

    try {
        m_currentDraftState.emplace(map, mode, m_allBrawlersMasterList);
        setStatus(QString("New draft started for %1 - %2.").arg(mode, map));
        qInfo() << "Initialized new draft:" << m_currentDraftState->toString();
    } catch (const std::exception& e) {
        qCritical() << "Error initializing DraftState:" << e.what();
        QMessageBox::critical(this, "Error", QString("Failed to initialize draft state:\n%1").arg(e.what()));
        m_currentDraftState.reset();
    } catch(...) {
         qCritical() << "Unknown error initializing DraftState.";
         QMessageBox::critical(this, "Error", "Unknown error initializing draft state.");
         m_currentDraftState.reset();
    }

    m_searchLineEdit->clear();
    clearSuggestionDisplay();
    updateUiFromState();
}

// --- Draft Action Slots ---
// onPickTeam1Clicked, onPickTeam2Clicked, onBanClicked, onUnbanClicked (No changes needed)
void MainWindow::onPickTeam1Clicked() {
    if (!m_currentDraftState || m_mctsManager->isRunning()) return;
    QString brawler = getSelectedListWidgetItemText(m_availableListWidget);
    if (brawler.isEmpty()) { setStatus("Select a brawler from 'Available'.", true); return; }

    try {
        if (m_currentDraftState->currentTurn() != "team1") throw std::logic_error("Not Team 1's turn.");
        m_currentDraftState = m_currentDraftState->applyMove(brawler); // Update state
        setStatus(QString("Picked %1 for T1.").arg(brawler), false, true);
        qInfo() << "Action: Picked" << brawler << "for T1. New state:" << m_currentDraftState->toString();
        updateUiFromState();
    } catch (const std::exception& e) {
        setStatus(QString("Invalid Action: %1").arg(e.what()), true);
        qWarning() << "Invalid action (Pick T1):" << e.what();
        QMessageBox::warning(this, "Invalid Action", e.what());
         if (m_currentDraftState) {
            if (!m_currentDraftState->availableBrawlers().contains(brawler)) {
                 qWarning() << "UI inconsistency detected. Refreshing available list.";
                 updateAvailableListDisplay();
            }
         }
    }
}
void MainWindow::onPickTeam2Clicked() {
     if (!m_currentDraftState || m_mctsManager->isRunning()) return;
    QString brawler = getSelectedListWidgetItemText(m_availableListWidget);
    if (brawler.isEmpty()) { setStatus("Select a brawler from 'Available'.", true); return; }

    try {
        if (m_currentDraftState->currentTurn() != "team2") throw std::logic_error("Not Team 2's turn.");
        m_currentDraftState = m_currentDraftState->applyMove(brawler);
        setStatus(QString("Picked %1 for T2.").arg(brawler), false, true);
         qInfo() << "Action: Picked" << brawler << "for T2. New state:" << m_currentDraftState->toString();
        updateUiFromState();
    } catch (const std::exception& e) {
        setStatus(QString("Invalid Action: %1").arg(e.what()), true);
        qWarning() << "Invalid action (Pick T2):" << e.what();
        QMessageBox::warning(this, "Invalid Action", e.what());
         if (m_currentDraftState) {
            if (!m_currentDraftState->availableBrawlers().contains(brawler)) {
                 qWarning() << "UI inconsistency detected. Refreshing available list.";
                 updateAvailableListDisplay();
            }
         }
    }
}
void MainWindow::onBanClicked() {
     if (!m_currentDraftState || m_mctsManager->isRunning()) return;
    QString brawler = getSelectedListWidgetItemText(m_availableListWidget);
    if (brawler.isEmpty()) { setStatus("Select a brawler from 'Available'.", true); return; }

    try {
         if (m_currentDraftState->bans().size() >= 6) throw std::logic_error("Max bans (6) reached.");
        m_currentDraftState = m_currentDraftState->applyBan(brawler);
        setStatus(QString("Banned %1.").arg(brawler), false, true);
         qInfo() << "Action: Banned" << brawler << ". New state:" << m_currentDraftState->toString();
        updateUiFromState();
    } catch (const std::exception& e) {
        setStatus(QString("Invalid Action: %1").arg(e.what()), true);
        qWarning() << "Invalid action (Ban):" << e.what();
        QMessageBox::warning(this, "Invalid Action", e.what());
         if (m_currentDraftState) {
            if (!m_currentDraftState->availableBrawlers().contains(brawler)) {
                 qWarning() << "UI inconsistency detected. Refreshing available list.";
                 updateAvailableListDisplay();
            }
         }
    }
}
void MainWindow::onUnbanClicked() {
    if (!m_currentDraftState || m_mctsManager->isRunning()) return;
    QString brawler = getSelectedListWidgetItemText(m_bansListWidget);
    if (brawler.isEmpty()) { setStatus("Select a brawler from 'Bans'.", true); return; }

    if (!m_currentDraftState->bans().contains(brawler)) {
        setStatus(QString("Error: %1 not found in bans.").arg(brawler), true);
        return;
    }

    QSet<QString> nextBans = m_currentDraftState->bans();
    nextBans.remove(brawler);

    try {
         m_currentDraftState.emplace(m_currentDraftState->mapName(), m_currentDraftState->modeName(),
                                    m_allBrawlersMasterList, nextBans,
                                    m_currentDraftState->team1Picks(), m_currentDraftState->team2Picks(),
                                    m_currentDraftState->currentTurn(), m_currentDraftState->currentPickNumber());
        setStatus(QString("Unbanned %1.").arg(brawler), false, true);
        qInfo() << "Action: Unbanned" << brawler << ". New state:" << m_currentDraftState->toString();
        updateUiFromState();
    } catch (const std::exception& e) {
         setStatus(QString("Error during unban: %1").arg(e.what()), true);
         qCritical() << "Error recreating state after unban: " << e.what();
         QMessageBox::critical(this, "Error", QString("Failed to unban:\n%1").arg(e.what()));
         updateUiFromState();
    }
}

// --- MODIFIED: onUndoPickClicked ---
// --- MODIFIED: onUndoPickClicked ---
void MainWindow::onUndoPickClicked() {
    // **CRITICAL CHECK:** Ensure MCTS is not running before attempting undo
    if (m_mctsManager->isRunning()) {
        setStatus("Cannot undo while MCTS is running.", true);
        QMessageBox::warning(this, "Undo Failed", "Please wait for MCTS to finish or stop it before undoing.");
        return;
    }

    // Check if there is an active draft state
    if (!m_currentDraftState) {
       setStatus("Cannot undo: No active draft.", true);
       return;
    }

    // Allow undo even if the draft is technically complete (pick number > 6)
    // if (m_currentDraftState->isComplete()){ setStatus("Cannot undo, draft complete."); return; } // REMOVED

    int currentPickNum = m_currentDraftState->currentPickNumber();
    if (currentPickNum <= 1) {
        setStatus("Cannot undo first pick.");
        return;
    }

    qInfo() << "Attempting to undo pick number" << (currentPickNum - 1);

    // Store current state details before modification, in case recreation fails
    QString currentMap = m_currentDraftState->mapName();
    QString currentMode = m_currentDraftState->modeName();
    QSet<QString> currentBans = m_currentDraftState->bans();


    // --- Logic to determine previous state ---
    int prevPickNum = currentPickNum - 1;
    QString prevTurn = "";
    QString lastPickedBrawler = "";
    // Make copies to modify for the potential previous state
    QVector<QString> prevTeam1Picks = m_currentDraftState->team1Picks();
    QVector<QString> prevTeam2Picks = m_currentDraftState->team2Picks();

    // Determine previous turn and remove last pick based on *current* pick number
    // (The pick number *before* the one being undone)
    switch (currentPickNum) {
        // Who made pick 'prevPickNum' (which is currentPickNum - 1)?
        case 2: // T1 made pick 1
            prevTurn = "team1";
            if (!prevTeam1Picks.isEmpty()) {
               lastPickedBrawler = prevTeam1Picks.last(); // Get brawler first
               prevTeam1Picks.removeLast();               // Then remove
            }
            break;
        case 3: // T2 made pick 2
            prevTurn = "team2";
            if (!prevTeam2Picks.isEmpty()) {
               lastPickedBrawler = prevTeam2Picks.last();
               prevTeam2Picks.removeLast();
            }
            break;
        case 4: // T2 made pick 3
            prevTurn = "team2";
            if (!prevTeam2Picks.isEmpty()) {
                lastPickedBrawler = prevTeam2Picks.last();
                prevTeam2Picks.removeLast();
             }
            break;
        case 5: // T1 made pick 4
            prevTurn = "team1";
            if (!prevTeam1Picks.isEmpty()) {
                lastPickedBrawler = prevTeam1Picks.last();
                prevTeam1Picks.removeLast();
            }
            break;
        case 6: // T1 made pick 5
            prevTurn = "team1";
            if (!prevTeam1Picks.isEmpty()) {
                 lastPickedBrawler = prevTeam1Picks.last();
                 prevTeam1Picks.removeLast();
             }
            break;
        case 7: // T2 made pick 6 (State is AFTER pick 6, so undoing pick 6)
            prevTurn = "team2"; // The player whose turn it becomes is T2 (to make pick 6 again)
            if (!prevTeam2Picks.isEmpty()) {
                lastPickedBrawler = prevTeam2Picks.last();
                prevTeam2Picks.removeLast();
            }
            // Corrected Logic: When pick # is 7 (after pick 6), undoing means going
            // back to pick #6, and it was T2's turn to make that pick.
            break;
        default:
            qWarning() << "Undo error: Unexpected current pick number" << currentPickNum;
            setStatus("Undo failed (invalid state).", true);
            return;
    }

    // Check if a brawler was actually removed (handles empty team lists unexpectedly)
    if (lastPickedBrawler.isEmpty()) {
        qWarning() << "Undo error: Could not determine last picked brawler for pick number" << currentPickNum;
        setStatus("Undo failed (state error).", true);
        return;
    }

    // --- Recreate the previous state ---
    try {
         m_currentDraftState.emplace(currentMap, currentMode, // Use stored map/mode
                                    m_allBrawlersMasterList, currentBans, // Use stored bans
                                    prevTeam1Picks, prevTeam2Picks, // Use modified pick lists
                                    prevTurn, prevPickNum); // Use calculated previous turn/pick#

        // Success: Update status and UI
        setStatus(QString("Undid pick of %1. Back to pick %2 (%3's turn).")
                    .arg(lastPickedBrawler).arg(prevPickNum).arg(prevTurn), false, true);
        qInfo() << "Undo successful. Reverted state:" << m_currentDraftState->toString();
        updateUiFromState(); // Update UI *after* successful state change

    } catch (const std::exception& e) {
        // Failed to recreate state - this indicates a bug in DraftState constructor or logic above
        setStatus(QString("Error recreating state after undo: %1").arg(e.what()), true);
        qCritical() << "Error during undo state recreation: " << e.what();
        QMessageBox::critical(this, "Undo Error", QString("Failed to undo - Error recreating state:\n%1").arg(e.what()));
        // State might be inconsistent, try to force UI update based on potentially corrupt state
        updateUiFromState();
    } catch (...) {
       // Catch any other unexpected errors during state recreation
       setStatus("Unknown error recreating state after undo.", true);
       qCritical() << "Unknown error during undo state recreation.";
       QMessageBox::critical(this, "Undo Error", "Failed to undo - Unknown error recreating state.");
       updateUiFromState();
    }
}

// onAvailableListDoubleClicked, onBansListDoubleClicked, onSearchTextChanged (No changes needed)
void MainWindow::onAvailableListDoubleClicked(QListWidgetItem *item) {
    if (!item || !m_currentDraftState || m_mctsManager->isRunning()) return;
    const DraftState& ds = *m_currentDraftState;
    if (ds.currentTurn() == "team1" && ds.team1Picks().size() < 3) {
        onPickTeam1Clicked();
    } else if (ds.currentTurn() == "team2" && ds.team2Picks().size() < 3) {
        onPickTeam2Clicked();
    } else if (ds.bans().size() < 6 && !ds.isComplete()) {
        onBanClicked();
    } else {
         setStatus(QString("Cannot auto-pick/ban %1 currently.").arg(item->text()));
    }
}

void MainWindow::onBansListDoubleClicked(QListWidgetItem *item) {
    if (!item || !m_currentDraftState || m_mctsManager->isRunning()) return;
    onUnbanClicked();
}


void MainWindow::onSearchTextChanged(const QString &text) {
    updateAvailableListDisplay();
}


// --- Suggestion Slots ---

// HeuristicWeights MainWindow::getWeightsFromUi() const { // <-- REMOVED
//     return { ... };
// }

void MainWindow::onSuggestHeuristicClicked() {
     if (!m_currentDraftState || m_currentDraftState->isComplete()) {
        setStatus("Cannot suggest: Draft not active or complete."); return;
     }
     if (m_mctsManager->isRunning()) { setStatus("Stop MCTS first."); return; }

    // Use weights directly from config
    HeuristicWeights weights = m_config.heuristicWeights();

    setStatus("Calculating heuristic...");
    m_suggestionLabel->setText("Suggestion: Calculating...");
    clearSuggestionDisplay();
    QCoreApplication::processEvents(); // Allow UI update

    try {
        auto [bestPick, scoresDict] = suggestPickHeuristic(*m_currentDraftState, m_statsCalculator, weights);

        if (!bestPick.isEmpty()) {
            m_suggestionLabel->setText(QString("Heuristic Suggestion: %1").arg(bestPick));
            displayHeuristicScores(scoresDict);
            setStatus("Heuristic suggestion complete.");
        } else {
            m_suggestionLabel->setText("Suggestion: No legal moves found.");
            setStatus("No heuristic suggestions possible.");
        }
    } catch (const std::exception& e) {
         setStatus(QString("Heuristic calc error: %1").arg(e.what()), true, true);
         qCritical() << "Heuristic calculation error: " << e.what();
         QMessageBox::critical(this, "Heuristic Error", QString("Error:\n%1").arg(e.what()));
    }
}

void MainWindow::onSuggestMctsClicked() {
     if (!m_currentDraftState || m_currentDraftState->isComplete()) {
         setStatus("Cannot start MCTS: Draft not active or complete."); return;
     }
     if (m_mctsManager->isRunning()) {
         QMessageBox::warning(this, "MCTS Running", "MCTS is already running."); return;
     }

     validateMctsTimeInput();
     // double timeLimit = m_config.mctsTimeLimit(); // Not needed directly here

     // Use weights directly from config
     HeuristicWeights weights = m_config.heuristicWeights();

    setStatus("Starting MCTS...");
    m_suggestionLabel->setText("Suggestion: Starting MCTS...");
    clearSuggestionDisplay();
    setControlsEnabled(false);
    m_stopMctsButton->setEnabled(true);

    // Pass the weights from config to the MCTS manager
    QMetaObject::invokeMethod(m_mctsManager, "startMcts", Qt::QueuedConnection,
                              Q_ARG(DraftState, *m_currentDraftState),
                              Q_ARG(HeuristicWeights, weights));
}

// onSuggestBanClicked, onStopMctsClicked (No changes needed)
void MainWindow::onSuggestBanClicked() {
    if (!m_currentDraftState || m_currentDraftState->isComplete()) {
        setStatus("Cannot suggest ban: Draft not active or complete."); return;
     }
     if (m_mctsManager->isRunning()) { setStatus("Stop MCTS first."); return; }
     if (m_currentDraftState->bans().size() >= 6) { setStatus("Max bans reached."); return; }


    setStatus("Calculating ban suggestions...");
    m_suggestionLabel->setText("Suggestion: Calculating Bans...");
    clearSuggestionDisplay();
     QCoreApplication::processEvents();


    try {
         int numSuggestions = 5;
        QVector<QString> suggestedBans = suggestBanHeuristic(*m_currentDraftState, m_statsCalculator, numSuggestions);

        if (!suggestedBans.isEmpty()) {
            m_suggestionLabel->setText(QString("Ban Suggestions: %1").arg(QStringList::fromVector(suggestedBans).join(", ")));
            displayBanScores(suggestedBans);
            setStatus("Ban suggestions complete.");
        } else {
            m_suggestionLabel->setText("Suggestion: No ban suggestions available.");
            setStatus("No ban suggestions found.");
        }
    } catch (const std::exception& e) {
         setStatus(QString("Ban suggestion error: %1").arg(e.what()), true, true);
         qCritical() << "Ban suggestion error: " << e.what();
         QMessageBox::critical(this, "Ban Suggestion Error", QString("Error:\n%1").arg(e.what()));
    }
}

void MainWindow::onStopMctsClicked() {
    if (m_mctsManager->isRunning()) {
        qInfo() << "Stop MCTS button clicked.";
        setStatus("Attempting to stop MCTS...");
        m_stopMctsButton->setEnabled(false);
        QMetaObject::invokeMethod(m_mctsManager, "stopMcts", Qt::QueuedConnection);
    } else {
        qWarning() << "Stop MCTS clicked, but MCTS not running.";
        m_stopMctsButton->setEnabled(false);
    }
}


// --- MCTS Update Slots --- (No changes needed)
void MainWindow::handleMctsStatus(const QString& status) {
     bool controlsCurrentlyDisabled = !m_suggestMctsButton->isEnabled();
     bool isFinalStatus = status.contains("Finished", Qt::CaseInsensitive) ||
                          status.contains("Error", Qt::CaseInsensitive) ||
                          status.contains("Stopped", Qt::CaseInsensitive) ||
                          status.contains("Reached", Qt::CaseInsensitive);

     if (controlsCurrentlyDisabled || isFinalStatus) {
        setStatus(QString("Status: %1").arg(status), status.contains("Error", Qt::CaseInsensitive));
     }
}

void MainWindow::handleMctsIntermediateResult(const QVector<MCTSResult>& results) {
     if (m_mctsManager->isRunning()) {
        displayMctsScores(results, true);
        if (!results.isEmpty()) {
             m_suggestionLabel->setText(QString("MCTS Suggestion (Live): %1").arg(results[0].move));
        } else {
             m_suggestionLabel->setText("Suggestion: MCTS Running...");
        }
     }
}

void MainWindow::handleMctsFinalResult(const QVector<MCTSResult>& results) {
     qInfo() << "Processing final MCTS result.";
     displayMctsScores(results, false);
     if (!results.isEmpty()) {
         m_suggestionLabel->setText(QString("MCTS Suggestion: %1").arg(results[0].move));
         if (!m_statusLabel->text().contains("Finished") && !m_statusLabel->text().contains("Stopped")) {
            setStatus("MCTS finished.");
         }
     } else {
         m_suggestionLabel->setText("Suggestion: MCTS found no moves.");
         if (!m_statusLabel->text().contains("Finished") && !m_statusLabel->text().contains("Stopped")) {
             setStatus("MCTS finished, no suggestion.");
         }
     }
}

void MainWindow::handleMctsError(const QString& errorMsg) {
    setStatus(QString("Status: %1").arg(errorMsg), true, true);
    qCritical() << "MCTS Error reported: " << errorMsg;
    QMessageBox::critical(this, "MCTS Error", errorMsg);
}

void MainWindow::handleMctsFinished() {
     qInfo() << "MCTS finished signal received. Re-enabling controls.";
     setControlsEnabled(true);
     m_stopMctsButton->setEnabled(false);
     if (!m_statusLabel->text().contains("Finished") && !m_statusLabel->text().contains("Error") && !m_statusLabel->text().contains("Stopped")) {
        setStatus("Status: MCTS process completed.");
     }
}

// --- UI Update Helpers ---

void MainWindow::updateUiFromState() {
    // Clear lists
    m_availableListWidget->clear();
    m_team1ListWidget->clear();
    m_team2ListWidget->clear();
    m_bansListWidget->clear();

    bool draftActive = m_currentDraftState.has_value();
    bool mctsRunning = m_mctsManager->isRunning();

    if (draftActive && !mctsRunning) {
        const DraftState& ds = *m_currentDraftState;
        updateAvailableListDisplay();

        for (const auto& b : ds.team1Picks()) m_team1ListWidget->addItem(b);
        for (const auto& b : ds.team2Picks()) m_team2ListWidget->addItem(b);
        QStringList bansSorted = ds.bans().values();
        std::sort(bansSorted.begin(), bansSorted.end());
        m_bansListWidget->addItems(bansSorted);

        bool isComplete = ds.isComplete();
        QString turnText = isComplete ? "Complete" : ds.currentTurn();
        m_turnLabel->setText(QString("Turn: %1").arg(turnText));
        QString pickText = (ds.currentPickNumber() <= 6) ? QString::number(ds.currentPickNumber()) : "Done";
        m_pickNumLabel->setText(QString("Pick #: %1").arg(pickText));

        // --- Button State Logic ---
        bool canPickT1 = !isComplete && ds.currentTurn() == "team1" && ds.team1Picks().size() < 3;
        bool canPickT2 = !isComplete && ds.currentTurn() == "team2" && ds.team2Picks().size() < 3;
        bool canBan = !isComplete && ds.bans().size() < 6;
        bool canUnban = !ds.bans().isEmpty();
        // Allow undo unless it's the very beginning
        bool canUndoPick = ds.currentPickNumber() > 1;

        m_pickT1Button->setEnabled(canPickT1);
        m_pickT2Button->setEnabled(canPickT2);
        m_banButton->setEnabled(canBan);
        m_unbanButton->setEnabled(canUnban);
        m_undoPickButton->setEnabled(canUndoPick);

        m_suggestHeuristicButton->setEnabled(!isComplete);
        m_suggestMctsButton->setEnabled(!isComplete);
        m_suggestBanButton->setEnabled(canBan); // Suggest ban only if banning is possible

        m_resetButton->setEnabled(true);

    } else if (mctsRunning) {
         // Update lists based on state *before* MCTS started
         if(m_currentDraftState) {
            updateAvailableListDisplay();
            for (const auto& b : m_currentDraftState->team1Picks()) m_team1ListWidget->addItem(b);
            for (const auto& b : m_currentDraftState->team2Picks()) m_team2ListWidget->addItem(b);
            QStringList bansSorted = m_currentDraftState->bans().values(); std::sort(bansSorted.begin(), bansSorted.end());
            m_bansListWidget->addItems(bansSorted);
            QString turnText = m_currentDraftState->isComplete() ? "Complete" : m_currentDraftState->currentTurn();
            m_turnLabel->setText(QString("Turn: %1").arg(turnText));
            QString pickText = (m_currentDraftState->currentPickNumber() <= 6) ? QString::number(m_currentDraftState->currentPickNumber()) : "Done";
            m_pickNumLabel->setText(QString("Pick #: %1").arg(pickText));
         }
         // Controls are disabled by setControlsEnabled(false)

    } else {
        // No draft active
        m_turnLabel->setText("Turn: -");
        m_pickNumLabel->setText("Pick #: -");
        m_pickT1Button->setEnabled(false);
        m_pickT2Button->setEnabled(false);
        m_banButton->setEnabled(false);
        m_unbanButton->setEnabled(false);
        m_undoPickButton->setEnabled(false);
        m_suggestHeuristicButton->setEnabled(false);
        m_suggestMctsButton->setEnabled(false);
        m_suggestBanButton->setEnabled(false);
        m_resetButton->setEnabled(!m_modeComboBox->currentText().isEmpty() && !m_mapComboBox->currentText().isEmpty());
    }
}


void MainWindow::updateAvailableListDisplay() {
    m_availableListWidget->clear();
    if (m_currentDraftState) {
        QString searchTerm = m_searchLineEdit->text().trimmed().toLower();
        QVector<QString> available = m_currentDraftState->getLegalMoves();

        for (const QString& brawler : available) {
            if (searchTerm.isEmpty() || brawler.toLower().contains(searchTerm)) {
                m_availableListWidget->addItem(brawler);
            }
        }
    }
}

void MainWindow::setControlsEnabled(bool enabled) {
    bool draftIsActive = m_currentDraftState.has_value();
    bool draftIsComplete = draftIsActive && m_currentDraftState->isComplete();
    bool draftCanProgress = draftIsActive && !draftIsComplete;

    // Always controllable when enabled
    m_modeComboBox->setEnabled(enabled);
    m_mapComboBox->setEnabled(enabled);
    m_mctsTimeLineEdit->setEnabled(enabled);
    // Enable reset only if draft is possible (mode/map selected) or active
    m_resetButton->setEnabled(enabled && (!m_modeComboBox->currentText().isEmpty() && !m_mapComboBox->currentText().isEmpty()));

    // Only available if draft is active
    m_searchLineEdit->setEnabled(enabled && draftIsActive);
    m_availableListWidget->setEnabled(enabled && draftIsActive);

    // Disable all action buttons if 'enabled' is false
    m_pickT1Button->setEnabled(enabled && draftCanProgress); // Further refine in updateUiFromState
    m_pickT2Button->setEnabled(enabled && draftCanProgress); // Further refine in updateUiFromState
    m_banButton->setEnabled(enabled && draftCanProgress);    // Further refine in updateUiFromState
    m_unbanButton->setEnabled(enabled && draftIsActive);     // Further refine in updateUiFromState
    m_undoPickButton->setEnabled(enabled && draftIsActive);  // Further refine in updateUiFromState

    m_suggestHeuristicButton->setEnabled(enabled && draftCanProgress);
    m_suggestMctsButton->setEnabled(enabled && draftCanProgress);
    m_suggestBanButton->setEnabled(enabled && draftCanProgress); // Further refine in updateUiFromState

    m_stopMctsButton->setEnabled(!enabled); // Stop button is enabled ONLY when other controls are disabled

    if (enabled) {
        // If re-enabling, call updateUiFromState immediately
        // to set the correct specific states for action buttons
        updateUiFromState();
    }
}


// setStatus, clearSuggestionDisplay, displayHeuristicScores, displayBanScores, displayMctsScores (No changes needed)
void MainWindow::setStatus(const QString& text, bool isError, bool clearSuggestion) {
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError ? "color: red;" : "");
    if (clearSuggestion) {
        clearSuggestionDisplay();
    }
    if (isError) qWarning() << text; else qInfo() << text;
}

void MainWindow::clearSuggestionDisplay() {
    m_suggestionLabel->setText("Suggestion: -");
    m_scoresTitleLabel->setText("Details:");
    m_scoresTextEdit->clear();
}

void MainWindow::displayHeuristicScores(const QHash<QString, HeuristicScoreComponents>& scoresDict) {
    m_scoresTitleLabel->setText("Heuristic Scores (Top 30):");
    m_scoresTextEdit->clear();

    if (scoresDict.isEmpty()) {
        m_scoresTextEdit->setText("No heuristic scores available.");
        return;
    }

    QVector<QPair<QString, HeuristicScoreComponents>> sortedScores;
    sortedScores.reserve(scoresDict.size());
    for (auto it = scoresDict.constBegin(); it != scoresDict.constEnd(); ++it) {
        sortedScores.append({it.key(), it.value()});
    }

    std::sort(sortedScores.begin(), sortedScores.end(),
              [](const QPair<QString, HeuristicScoreComponents>& a, const QPair<QString, HeuristicScoreComponents>& b) {
                  return a.second.totalScore > b.second.totalScore;
              });

    QString text;
    QTextStream stream(&text);
    stream << QString("%1 | %2 | %3 | %4 | %5 | %6\n")
              .arg("Brawler", -18).arg("Score", 8).arg("Adj WR", 8)
              .arg("Avg Syn", 8).arg("Avg Ctr", 8).arg("PickRate", 8);
    stream << QString("-").repeated(78) << "\n";

    int count = 0;
    const int DISPLAY_LIMIT = 30;
    for (const auto& pair : sortedScores) {
        const HeuristicScoreComponents& scores = pair.second;
        stream << QString("%1 | %2 | %3 | %4 | %5 | %6\n")
                  .arg(pair.first, -18)
                  .arg(scores.totalScore, 8, 'f', 3)
                  .arg(scores.winRate, 8, 'f', 3)
                  .arg(scores.avgSynergy, 8, 'f', 3)
                  .arg(scores.avgCounter, 8, 'f', 3)
                  .arg(scores.pickRate, 8, 'f', 3);
        count++;
        if (count >= DISPLAY_LIMIT) {
            stream << "\n... (Top " << count << " shown)";
            break;
        }
    }

    m_scoresTextEdit->setFontFamily("monospace");
    m_scoresTextEdit->setText(text);
}

void MainWindow::displayBanScores(const QVector<QString>& suggestedBans) {
     m_scoresTitleLabel->setText("Ban Suggestion Details (Adj Win Rate):");
     m_scoresTextEdit->clear();

     if (suggestedBans.isEmpty()) {
         m_scoresTextEdit->setText("No ban suggestions.");
         return;
     }

     QVector<QPair<QString, double>> banDetails;
     if(m_currentDraftState){
         for(const QString& brawler : suggestedBans) {
              double wr = m_statsCalculator.getWinRate(brawler, m_currentDraftState->mapName(), m_currentDraftState->modeName())
                            .value_or(m_config.lowConfidenceWinRateTarget());
              banDetails.append({brawler, wr});
         }
         std::sort(banDetails.begin(), banDetails.end(),
                   [](const QPair<QString, double>& a, const QPair<QString, double>& b){
                       return a.second > b.second;
                   });
     } else {
         m_scoresTextEdit->setText("Error: Draft state missing for ban details.");
         return;
     }


     QString text;
     QTextStream stream(&text);
     stream << QString("%1 | %2\n").arg("Brawler", -18).arg("Adj Win Rate", 12);
     stream << QString("-").repeated(35) << "\n";

     for (const auto& pair : banDetails) {
         stream << QString("%1 | %2\n")
                   .arg(pair.first, -18)
                   .arg(pair.second, 12, 'f', 3);
     }

     m_scoresTextEdit->setFontFamily("monospace");
     m_scoresTextEdit->setText(text);
}


void MainWindow::displayMctsScores(const QVector<MCTSResult>& results, bool isIntermediate) {
    m_scoresTitleLabel->setText(QString("MCTS Top Picks%1:").arg(isIntermediate ? " (Live)" : ""));
    m_scoresTextEdit->clear();

    if (results.isEmpty()) {
        m_scoresTextEdit->setText("No MCTS results available.");
        return;
    }

    QString text;
    QTextStream stream(&text);
    stream << QString("%1 | %2 | %3\n")
              .arg("Brawler", -18).arg("Visits", 8).arg("Est Win %", 10);
    stream << QString("-").repeated(42) << "\n";

    int displayCount = m_config.mctsResultCount();
    int count = 0;
    for (const auto& result : results) {
        if (count >= displayCount) break;
        double winPercent = result.winRate * 100.0;
        stream << QString("%1 | %2 | %3%\n")
                  .arg(result.move, -18)
                  .arg(result.visits, 8)
                  .arg(winPercent, 9, 'f', 1);
        count++;
    }
    if (results.size() > displayCount) {
         stream << "\n... (Top " << displayCount << " shown)";
    }

    m_scoresTextEdit->setFontFamily("monospace");
    m_scoresTextEdit->setText(text);
}


// --- Utility Helpers ---

QString MainWindow::getSelectedListWidgetItemText(QListWidget* listWidget) const {
    QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return "";
    }
    return selectedItems.first()->text();
}

void MainWindow::saveConfig() {
     qInfo() << "Saving configuration...";
     validateMctsTimeInput();
     // No weights to get from UI anymore
     // m_config.setHeuristicWeights(getWeightsFromUi()); // REMOVED
     m_config.save();
}

// --- Event Overrides ---

void MainWindow::closeEvent(QCloseEvent *event) {
    qInfo() << "Close event triggered.";
    if (m_mctsManager->isRunning()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "MCTS Running",
                                      "MCTS is currently running. Stop it and exit?",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            qInfo() << "Stopping MCTS before exiting...";
            onStopMctsClicked();
            saveConfig(); // Save config even if stopping MCTS
            event->accept();
        } else {
            qInfo() << "Exit cancelled by user.";
            event->ignore();
        }
    } else {
        saveConfig();
        event->accept();
    }
}