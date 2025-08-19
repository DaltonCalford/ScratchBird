#ifndef SCRATCHBIRD_ENGINE_STORAGE_H
#define SCRATCHBIRD_ENGINE_STORAGE_H

#include <string>

namespace scratchbird
{
    namespace engine
    {

        struct StorageConfig {
            std::string path;
        };

        class StorageEngine
        {
          public:
            bool open(const StorageConfig&)
            {
                return true;
            }
            void close() {}
        };

    } // namespace engine
} // namespace scratchbird

#endif // SCRATCHBIRD_ENGINE_STORAGE_H
