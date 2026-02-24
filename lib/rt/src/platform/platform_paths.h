#ifndef QD_QDRT_PLATFORM_PATHS_H
#define QD_QDRT_PLATFORM_PATHS_H

// Platform-specific data directory name
// Haiku uses "data" instead of "share" for data files
#ifdef __HAIKU__
static constexpr const char* DATA_DIR_NAME = "data";
#else
static constexpr const char* DATA_DIR_NAME = "share";
#endif

#endif // QD_QDRT_PLATFORM_PATHS_H
