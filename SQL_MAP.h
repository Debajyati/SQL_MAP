#ifndef SQL_MAP_H
#define SQL_MAP_H

#include <stddef.h>

#if defined(__GNUC__)
    #define USE_THREAD_SAFETY
    #include <pthread.h>
#else
    #warning "Thread safety is disabled. Compile with gcc and pthreads to enable thread safety."
#endif

// -------------------------
// Structures and Typedefs
// -------------------------

typedef struct IndexNode {
    char *key;              // Interned key string
    int *dataIndex;         // Pointer to index in dataNodes array
    struct IndexNode *next; // For chaining within buckets
} IndexNode;

typedef struct DataNode {
    void *data;
} DataNode;

typedef struct SQLMap {
    IndexNode **buckets;    // Array of bucket pointers (chaining)
    size_t bucketCount;     // Number of buckets (capacity)
    size_t entryCount;      // Number of key-value entries

    DataNode *dataNodes;    // Dynamic array of data nodes
    size_t dataCapacity;    // Capacity of dataNodes
    size_t dataCount;       // Number of stored data nodes

#ifdef USE_THREAD_SAFETY
    pthread_mutex_t mutex;  // Mutex for thread safety
#endif
} SQLMap;

// -------------------------
// Function Prototypes
// -------------------------

// Create and initialize a new SQLMap instance.
SQLMap* sql_map_create(void);

// Insert or update a key-value pair in the SQLMap.
void sql_map_put(SQLMap *map, const char *key, void *value);

// Retrieve the value associated with the given key. Returns NULL if not found.
void* sql_map_get(SQLMap *map, const char *key);

// Remove a key-value pair from the SQLMap. Returns 1 if removed, 0 if not found.
int sql_map_remove(SQLMap *map, const char *key);

// Free all memory associated with the SQLMap.
// Note: The stored data (void*) is not freed; freeing it is the caller's responsibility.
void sql_map_free(SQLMap *map);

#endif  // SQL_MAP_H

