#include "CacheUtils.h"
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QDir> // To ensure directory exists

namespace CacheUtils {

    bool saveCache(const QString& filepath, const CacheData& data) {
        QFileInfo fileInfo(filepath);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                 qCritical() << "Failed to create cache directory:" << dir.path();
                 return false;
            }
        }


        QFile file(filepath);
        if (!file.open(QIODevice::WriteOnly)) {
            qCritical() << "Error opening cache file for writing:" << filepath << file.errorString();
            return false;
        }

        QDataStream out(&file);
        // Optional: Add a version number for future compatibility
        out.setVersion(QDataStream::Qt_6_0); // Or your target Qt version
        quint32 magicNumber = 0xACEDBABE; // Simple magic number
        qint16 version = 1;
        out << magicNumber;
        out << version;

        // Serialize the main data structure
        out << data; // Uses the overloaded operator<< for CacheData

        file.close();

        if (out.status() != QDataStream::Ok) {
             qCritical() << "Error writing data to cache file:" << filepath;
             // Attempt to remove potentially corrupted file
             if(file.exists()) file.remove();
             return false;
        }

        qInfo() << "Successfully saved cache to" << filepath;
        return true;
    }


    std::optional<CacheData> loadCache(const QString& filepath) {
        QFile file(filepath);
        if (!file.exists()) {
            qInfo() << "Cache file not found:" << filepath;
            return std::nullopt;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Error opening cache file for reading:" << filepath << file.errorString();
            return std::nullopt;
        }

        qInfo() << "Attempting to load cache from:" << filepath;
        QDataStream in(&file);
        in.setVersion(QDataStream::Qt_6_0); // Match the version used for saving

        // Verify magic number and version
        quint32 magicNumber;
        qint16 version;
        in >> magicNumber;
        if (in.status() != QDataStream::Ok || magicNumber != 0xACEDBABE) {
            qWarning() << "Cache file has invalid magic number or read error:" << filepath;
            return std::nullopt;
        }
        in >> version;
         if (in.status() != QDataStream::Ok || version != 1) { // Check version compatibility
            qWarning() << "Cache file version mismatch (expected 1, got" << version << "):" << filepath;
            return std::nullopt;
        }


        CacheData loadedData;
        in >> loadedData; // Uses the overloaded operator>> for CacheData

        file.close();

        if (in.status() != QDataStream::Ok) {
            qWarning() << "Error reading data from cache file (likely corrupted):" << filepath;
            return std::nullopt;
        }

        // Basic validation (can be expanded)
        if (loadedData.allBrawlers.isEmpty() || loadedData.discoveredMapModes.isEmpty()) {
            qWarning() << "Loaded cache data seems incomplete (no brawlers or maps/modes). Invalidating.";
            return std::nullopt;
        }


        qInfo() << "Cache file loaded successfully:" << filepath;
        return loadedData;
    }

} // namespace CacheUtils