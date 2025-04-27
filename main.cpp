#include "MainWindow.h"
#include "DataLoader.h"
#include "StatsCalculator.h"
#include "AppConfig.h"
#include "MCTS.h"
#include "CacheUtils.h"
#include "DataStructures.h"

#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

// --- Global Constants - File Names Only ---
const QString DATA_FILE_NAME = "high_level_ranked_games.jsonl"; // Renamed
const QString CACHE_FILE_NAME = "stats.pack";            // Renamed
const QString CONFIG_FILE_NAME = "draft_config.ini";         // Renamed
const QString LOG_FILE_NAME = "draft_log.log";          // Renamed


// --- Simple File Logger ---
// Now gets app path dynamically
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Get application path dynamically ONLY if QApplication exists
    QString logFilePath = LOG_FILE_NAME; // Default to relative path if app doesn't exist yet
    if (QCoreApplication::instance()) { // Check if app object exists
        logFilePath = QDir::cleanPath(QCoreApplication::applicationDirPath() + QDir::separator() + LOG_FILE_NAME);
    } else {
        // This case should ideally not happen often during normal logging
        // but handles early messages before `app` is fully initialized.
        // Log will be relative to the current working directory.
    }


    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr;

    switch (type) {
    case QtDebugMsg:    levelStr = "DEBUG"; break;
    case QtInfoMsg:     levelStr = "INFO "; break;
    case QtWarningMsg:  levelStr = "WARN "; break;
    case QtCriticalMsg: levelStr = "ERROR"; break;
    case QtFatalMsg:    levelStr = "FATAL"; break;
    }

    // Output to console/stderr remains the same
    fprintf(stderr, "%s %s: %s (%s:%u, %s)\n",
            timestamp.toLocal8Bit().constData(),
            levelStr.toLocal8Bit().constData(),
            localMsg.constData(), file, context.line, function);
    fflush(stderr);

     // Write to log file
     QFile outFile(logFilePath);
     if (outFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
         QTextStream ts(&outFile);
         ts << timestamp << " " << levelStr << ": " << msg << " (" << file << ":" << context.line << ", " << function << ")\n";
     } else {
         // Log error opening log file to stderr only
         fprintf(stderr, "%s %s: %s (%s:%u, %s)\n",
                 QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz").toLocal8Bit().constData(),
                 "ERROR",
                 QString("Failed to open log file for writing: %1 (Error: %2)").arg(logFilePath, outFile.errorString()).toLocal8Bit().constData(),
                 __FILE__, __LINE__, Q_FUNC_INFO);
     }

    if (type == QtFatalMsg) {
         abort();
    }
}


int main(int argc, char *argv[]) {
    // MUST be first Qt object created
    QApplication app(argc, argv);

    // Install logger AFTER app exists
    qInstallMessageHandler(messageHandler);

    // Now get application directory path safely
    const QString appDirPath = QCoreApplication::applicationDirPath();

    app.setOrganizationName("TexeshApps");
    app.setApplicationName("GlizzyDraft");

    qInfo() << "===================================";
    qInfo() << "Starting Glizzy Draft";
    qInfo() << "Application Directory:" << appDirPath;

    // --- Determine paths relative to application directory ---
    QString dataFilePath = QDir::cleanPath(appDirPath + QDir::separator() + DATA_FILE_NAME);
    QString cacheFilePath = QDir::cleanPath(appDirPath + QDir::separator() + CACHE_FILE_NAME);
    QString configFilePath = QDir::cleanPath(appDirPath + QDir::separator() + CONFIG_FILE_NAME);

    qInfo() << "Using data file:" << dataFilePath;
    qInfo() << "Using cache file:" << cacheFilePath;
    qInfo() << "Using config file:" << configFilePath;

    // --- Load Config ---
    AppConfig appConfig(configFilePath);

    // --- Initialize Core Components ---
    std::optional<StatsCalculator> statsCalculatorOpt;
    QSet<QString> allBrawlers;
    QHash<QString, QSet<QString>> discoveredMapModes;

    // --- Attempt to Load from Cache ---
    qInfo() << "Attempting to load data from cache...";
    auto cachedDataOpt = CacheUtils::loadCache(cacheFilePath);

    if (cachedDataOpt.has_value()) {
        try {
            CacheData& cachedData = cachedDataOpt.value();
             if (cachedData.allBrawlers.isEmpty() || cachedData.discoveredMapModes.isEmpty() || cachedData.stats.isEmpty()) {
                 qWarning() << "Cache data is incomplete. Forcing recalculation.";
                 cachedDataOpt.reset();
             } else {
                 allBrawlers = cachedData.allBrawlers;
                 discoveredMapModes = cachedData.discoveredMapModes;
                 statsCalculatorOpt.emplace(appConfig);
                 statsCalculatorOpt->setStatsFromCacheData(cachedData);
                 qInfo() << "Successfully initialized components from cache.";
             }
        } catch (const std::exception& e) {
             qCritical() << "Error processing loaded cache data:" << e.what() << ". Attempting recalculation.";
             statsCalculatorOpt.reset();
             cachedDataOpt.reset();
        } catch (...) {
             qCritical() << "Unknown error processing loaded cache data. Attempting recalculation.";
             statsCalculatorOpt.reset();
             cachedDataOpt.reset();
        }
    } else {
         qInfo() << "Cache not found or invalid.";
    }

    // --- If Cache Failed, Load and Process Data ---
    if (!statsCalculatorOpt.has_value()) {
        qInfo() << "Proceeding with source data loading and processing...";
        DataLoader dataLoader(dataFilePath, appConfig);

        if (!dataLoader.loadAndProcess()) {
            qCritical() << "Failed to load and process source data from:" << dataFilePath;
             if (!QFile::exists(dataFilePath)) {
                QMessageBox::critical(nullptr, "Fatal Error", "Data file not found:\n" + dataFilePath + "\nPlace it in the application directory.\nApplication cannot start without data.");
             } else {
                 QMessageBox::critical(nullptr, "Fatal Error", "Failed to process data file.\nCheck logs.\nApplication cannot start.");
             }
            return 1;
        }

        allBrawlers = dataLoader.getAllBrawlers();
        discoveredMapModes = dataLoader.getDiscoveredMapModes();
        const auto& processedGames = dataLoader.getProcessedGames();

        if (allBrawlers.isEmpty() || discoveredMapModes.isEmpty()) {
            qCritical() << "No brawlers or maps/modes identified after processing. Cannot proceed.";
            QMessageBox::critical(nullptr, "Fatal Error", "No usable data (brawlers/maps/modes) found.\nCheck data format and logs.\nApplication cannot start.");
            return 1;
        }
        if (processedGames.isEmpty()) {
             qWarning() << "No valid games were processed after filtering. Statistics will be minimal.";
             QMessageBox::StandardButton reply;
             reply = QMessageBox::question(nullptr, "Data Warning",
                                       "Warning: No valid games found after filtering.\nStatistics will be minimal (mostly 50% WR).\n\nContinue anyway?",
                                       QMessageBox::Yes | QMessageBox::No);
             if (reply == QMessageBox::No) {
                 return 0;
             }
        }

        qInfo() << "Initializing statistics calculator from source data...";
         statsCalculatorOpt.emplace(processedGames, appConfig);

        if (statsCalculatorOpt.has_value()) {
             qInfo() << "Attempting to save processed data to cache...";
             CacheData dataToCache = statsCalculatorOpt->getStatsForCache();
             dataToCache.allBrawlers = allBrawlers;
             dataToCache.discoveredMapModes = discoveredMapModes;
             dataToCache.metadata.cacheCreationTime = QDateTime::currentMSecsSinceEpoch();
             CacheUtils::saveCache(cacheFilePath, dataToCache);
        } else {
             qCritical() << "Stats calculator failed to initialize even after data processing.";
              QMessageBox::critical(nullptr, "Fatal Error", "Failed to initialize statistics engine.\nCheck logs.\nApplication cannot start.");
             return 1;
        }
    }

    // --- Final Sanity Check ---
    if (!statsCalculatorOpt.has_value() || allBrawlers.isEmpty() || discoveredMapModes.isEmpty()) {
         qCritical() << "Critical error: Core data components missing before GUI launch.";
         QMessageBox::critical(nullptr, "Fatal Error", "Failed to initialize core data components.\nCheck logs.\nApplication cannot start.");
         return 1;
    }

     StatsCalculator& calculator = *statsCalculatorOpt;
     MCTSManager mctsManager(calculator, appConfig);

    // --- Start GUI ---
    qInfo() << "Initializing GUI...";
    MainWindow mainWindow(calculator, allBrawlers, discoveredMapModes, appConfig, &mctsManager);
    mainWindow.show();

    qInfo() << "Application event loop started.";
    int execResult = app.exec();
    qInfo() << "Application event loop finished.";

    qInfo() << "Application closed.";
    return execResult;
}