#ifndef CACHEUTILS_H
#define CACHEUTILS_H

#include <QString>
#include <optional>
#include "DataStructures.h" // For CacheData

namespace CacheUtils {

    // Saves the CacheData structure to a binary file using QDataStream
    bool saveCache(const QString& filepath, const CacheData& data);

    // Loads CacheData from a file. Returns std::nullopt if file doesn't exist or fails to load.
    std::optional<CacheData> loadCache(const QString& filepath);

} // namespace CacheUtils

#endif // CACHEUTILS_H