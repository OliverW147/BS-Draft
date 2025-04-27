#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QString>
#include <QSet>
#include <QHash>
#include <QScopedPointer> // For PIMPL or managing UI pointers
#include <QListWidget> // Include for QListWidgetItem

#include "DataStructures.h"
#include "DraftState.h"
#include "StatsCalculator.h"
#include "AppConfig.h"
#include "MCTS.h"

// Forward declarations for UI elements
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QComboBox;
class QLineEdit;
// QListWidget included above
class QPushButton;
class QLabel;
class QTextEdit;
// class QDoubleSpinBox; // Removed - weights hidden
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const StatsCalculator& statsCalculator, // Pass dependencies
               const QSet<QString>& allBrawlers,
               const QHash<QString, QSet<QString>>& mapModeData,
               AppConfig& config, // Mutable config to save changes
               MCTSManager* mctsManager, // Pass manager pointer
               QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override; // To save config on close

private slots:
    // Control Slots
    void onModeChanged(int index);
    void onMapChanged(int index);
    void onResetDraftClicked();
    void validateMctsTimeInput(); // Slot for QLineEdit editingFinished or similar

    // Draft Action Slots
    void onPickTeam1Clicked();
    void onPickTeam2Clicked();
    void onBanClicked();
    void onUnbanClicked();
    void onUndoPickClicked();
    void onAvailableListDoubleClicked(QListWidgetItem *item);
    void onBansListDoubleClicked(QListWidgetItem *item);
    void onSearchTextChanged(const QString &text);

    // Suggestion Slots
    void onSuggestHeuristicClicked();
    void onSuggestMctsClicked();
    void onSuggestBanClicked();
    void onStopMctsClicked();

    // MCTS Update Slots
    void handleMctsStatus(const QString& status);
    void handleMctsIntermediateResult(const QVector<MCTSResult>& results);
    void handleMctsFinalResult(const QVector<MCTSResult>& results);
    void handleMctsError(const QString& errorMsg);
    void handleMctsFinished(); // Slot connected to MCTSManager::mctsFinished

private:
    void setupUi(); // Create and layout widgets manually or load .ui file
    void setupConnections(); // Connect signals and slots
    void loadInitialData(); // Populate mode dropdown etc.

    void initializeDraft(); // Resets internal state and UI for new draft
    void updateUiFromState(); // Updates all lists, labels, button states
    void updateAvailableListDisplay(); // Updates the available list based on search and state
    void setControlsEnabled(bool enabled); // Enables/disables UI elements during MCTS etc.
    void setStatus(const QString& text, bool isError = false, bool clearSuggestion = false);
    void clearSuggestionDisplay();
    void displayHeuristicScores(const QHash<QString, HeuristicScoreComponents>& scores);
    void displayBanScores(const QVector<QString>& suggestedBans); // Pass bans, lookup WR internally
    void displayMctsScores(const QVector<MCTSResult>& results, bool isIntermediate = false);
    void saveConfig(); // Saves current weights/settings

    // Helper to get selected item text
    QString getSelectedListWidgetItemText(QListWidget* listWidget) const;
    // Helper to get current weights from UI - REMOVED
    // HeuristicWeights getWeightsFromUi() const;


    // Dependencies (passed in constructor)
    const StatsCalculator& m_statsCalculator;
    const QSet<QString>& m_allBrawlersMasterList;
    const QHash<QString, QSet<QString>>& m_mapModeData;
    AppConfig& m_config; // Mutable reference
    MCTSManager* m_mctsManager; // Pointer to manager

    // Internal state
    std::optional<DraftState> m_currentDraftState; // Use optional to represent no active draft

    // --- UI Elements (Declare pointers) ---
    QComboBox *m_modeComboBox;
    QComboBox *m_mapComboBox;
    QLineEdit *m_mctsTimeLineEdit;
    QPushButton *m_resetButton;

    // Weights Frame REMOVED
    // QDoubleSpinBox *m_wWrSpinBox;
    // QDoubleSpinBox *m_wSynSpinBox;
    // QDoubleSpinBox *m_wCtrSpinBox;
    // QDoubleSpinBox *m_wPrSpinBox;

    // Display Frame
    QLineEdit *m_searchLineEdit;
    QListWidget *m_availableListWidget;
    QPushButton *m_pickT1Button;
    QPushButton *m_pickT2Button;
    QPushButton *m_banButton;
    QPushButton *m_unbanButton;
    QPushButton *m_undoPickButton;
    QListWidget *m_team1ListWidget;
    QListWidget *m_team2ListWidget;
    QListWidget *m_bansListWidget;
    QLabel *m_turnLabel;
    QLabel *m_pickNumLabel;

    // Suggestion Frame
    QPushButton *m_suggestHeuristicButton;
    QPushButton *m_suggestMctsButton;
    QPushButton *m_suggestBanButton;
    QPushButton *m_stopMctsButton;
    QLabel *m_suggestionLabel;
    QLabel *m_scoresTitleLabel; // Label above the text edit
    QTextEdit *m_scoresTextEdit;

    // Status Bar
    QLabel *m_statusLabel;
};

#endif // MAINWINDOW_H