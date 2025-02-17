#include "SQL_MAP.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// -------------------------
// Configuration Constants
// -------------------------

// Initial bucket count should be a prime number for better distribution.
#define INITIAL_BUCKET_COUNT 1031
#define INITIAL_DATA_CAPACITY 16
#define MAX_LOAD_FACTOR 0.70

// -------------------------
// Interned String Pool
// -------------------------

// A simple linked list node for the interned string pool.
typedef struct InternNode {
    char *str;
    struct InternNode *next;
} InternNode;

static InternNode *intern_pool = NULL;

// Look for an interned string; if not found, duplicate and add to pool.
static char *intern_string(const char *s) {
    InternNode *node = intern_pool;
    while (node) {
        if (strcmp(node->str, s) == 0) {
            return node->str;
        }
        node = node->next;
    }
    // Not found: allocate and add to pool.
    char *newStr = malloc(strlen(s) + 1);
    if (!newStr) {
        perror("Failed to allocate interned string");
        exit(EXIT_FAILURE);
    }
    strcpy(newStr, s);
    InternNode *newNode = malloc(sizeof(InternNode));
    if (!newNode) {
        perror("Failed to allocate intern pool node");
        exit(EXIT_FAILURE);
    }
    newNode->str = newStr;
    newNode->next = intern_pool;
    intern_pool = newNode;
    return newStr;
}

// Free intern pool (call at program end if needed)
static void free_intern_pool(void) {
    InternNode *node = intern_pool;
    while (node) {
        InternNode *temp = node;
        node = node->next;
        free(temp->str);
        free(temp);
    }
    intern_pool = NULL;
}

// -------------------------
// Hash Function (djb2)
// -------------------------

static unsigned long hash_str(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash;
}

// -------------------------
// Forward Declaration
// -------------------------

static void rehash(SQLMap *map);

// -------------------------
// SQLMap Functions
// -------------------------

SQLMap* sql_map_create(void) {
    SQLMap *map = malloc(sizeof(SQLMap));
    if (!map) {
        perror("Failed to allocate SQLMap");
        exit(EXIT_FAILURE);
    }
    map->bucketCount = INITIAL_BUCKET_COUNT;
    map->entryCount = 0;
    map->buckets = calloc(map->bucketCount, sizeof(IndexNode*));
    if (!map->buckets) {
        perror("Failed to allocate bucket array");
        free(map);
        exit(EXIT_FAILURE);
    }
    map->dataCapacity = INITIAL_DATA_CAPACITY;
    map->dataCount = 0;
    map->dataNodes = malloc(map->dataCapacity * sizeof(DataNode));
    if (!map->dataNodes) {
        perror("Failed to allocate data nodes array");
        free(map->buckets);
        free(map);
        exit(EXIT_FAILURE);
    }
#ifdef USE_THREAD_SAFETY
    if (pthread_mutex_init(&map->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        exit(EXIT_FAILURE);
    }
#endif
    return map;
}

static void lock_map(SQLMap *map) {
#ifdef USE_THREAD_SAFETY
    pthread_mutex_lock(&map->mutex);
#endif
}

static void unlock_map(SQLMap *map) {
#ifdef USE_THREAD_SAFETY
    pthread_mutex_unlock(&map->mutex);
#endif
}

// Resize and rehash the buckets if load factor exceeds MAX_LOAD_FACTOR.
static void rehash(SQLMap *map) {
    size_t oldBucketCount = map->bucketCount;
    IndexNode **oldBuckets = map->buckets;

    // Double the bucket count and find next prime if desired. For simplicity, double it.
    map->bucketCount *= 2;
    map->buckets = calloc(map->bucketCount, sizeof(IndexNode*));
    if (!map->buckets) {
        perror("Failed to allocate new bucket array during rehash");
        exit(EXIT_FAILURE);
    }

    // Re-insert each index node into new buckets.
    for (size_t i = 0; i < oldBucketCount; i++) {
        IndexNode *node = oldBuckets[i];
        while (node) {
            IndexNode *nextNode = node->next; // Save next pointer
            unsigned long h = hash_str(node->key);
            size_t pos = h % map->bucketCount;
            // Insert node at head of new bucket's chain.
            node->next = map->buckets[pos];
            map->buckets[pos] = node;
            node = nextNode;
        }
    }
    free(oldBuckets);
}

// Insert or update a key-value pair.
void sql_map_put(SQLMap *map, const char *key, void *value) {
    lock_map(map);

    // Check if rehashing is needed.
    double loadFactor = (double)map->entryCount / map->bucketCount;
    if (loadFactor > MAX_LOAD_FACTOR) {
        rehash(map);
    }

    // Insert data into the dataNodes array.
    if (map->dataCount >= map->dataCapacity) {
        map->dataCapacity *= 2;
        map->dataNodes = realloc(map->dataNodes, map->dataCapacity * sizeof(DataNode));
        if (!map->dataNodes) {
            perror("Failed to expand dataNodes array");
            exit(EXIT_FAILURE);
        }
    }
    int dataIndexValue = (int)map->dataCount;
    map->dataNodes[map->dataCount].data = value;
    map->dataCount++;

    // Create a new int on the heap to store the index.
    int *dataIndexPtr = malloc(sizeof(int));
    if (!dataIndexPtr) {
        perror("Failed to allocate data index pointer");
        exit(EXIT_FAILURE);
    }
    *dataIndexPtr = dataIndexValue;

    // Intern the key string.
    char *internedKey = intern_string(key);

    // Prepare the new index node.
    IndexNode *newNode = malloc(sizeof(IndexNode));
    if (!newNode) {
        perror("Failed to allocate IndexNode");
        exit(EXIT_FAILURE);
    }
    newNode->key = internedKey;
    newNode->dataIndex = dataIndexPtr;
    newNode->next = NULL;

    // Insert into buckets using chaining.
    unsigned long h = hash_str(internedKey);
    size_t pos = h % map->bucketCount;
    IndexNode *current = map->buckets[pos];
    IndexNode *prev = NULL;
    while (current) {
        if (strcmp(current->key, internedKey) == 0) {
            // Key exists: update value. Free old dataIndex pointer.
            free(current->dataIndex);
            current->dataIndex = dataIndexPtr;
            free(newNode);
            unlock_map(map);
            return;
        }
        prev = current;
        current = current->next;
    }
    // Insert at head of chain.
    newNode->next = map->buckets[pos];
    map->buckets[pos] = newNode;
    map->entryCount++;

    unlock_map(map);
}

// Retrieve the value associated with a key.
void* sql_map_get(SQLMap *map, const char *key) {
    lock_map(map);
    char *internedKey = intern_string(key);
    unsigned long h = hash_str(internedKey);
    size_t pos = h % map->bucketCount;
    IndexNode *node = map->buckets[pos];
    while (node) {
        if (strcmp(node->key, internedKey) == 0) {
            int idx = *(node->dataIndex);
            assert(idx < (int)map->dataCount);
            void *result = map->dataNodes[idx].data;
            unlock_map(map);
            return result;
        }
        node = node->next;
    }
    unlock_map(map);
    return NULL;
}

// Remove a key-value pair from the SQLMap. Returns 1 if removed, 0 if not found.
int sql_map_remove(SQLMap *map, const char *key) {
    lock_map(map);
    char *internedKey = intern_string(key);
    unsigned long h = hash_str(internedKey);
    size_t pos = h % map->bucketCount;
    IndexNode *node = map->buckets[pos];
    IndexNode *prev = NULL;
    while (node) {
        if (strcmp(node->key, internedKey) == 0) {
            // Remove node from chain.
            if (prev) {
                prev->next = node->next;
            } else {
                map->buckets[pos] = node->next;
            }
            // Free allocated memory for the index pointer and the node.
            free(node->dataIndex);
            free(node);
            map->entryCount--;
            unlock_map(map);
            return 1;
        }
        prev = node;
        node = node->next;
    }
    unlock_map(map);
    return 0;
}

// Free all memory associated with the SQLMap.
// Note: The stored data pointers (void*) are not freed.
void sql_map_free(SQLMap *map) {
    lock_map(map);
    // Free each chain in the buckets.
    for (size_t i = 0; i < map->bucketCount; i++) {
        IndexNode *node = map->buckets[i];
        while (node) {
            IndexNode *temp = node;
            node = node->next;
            free(temp->dataIndex);
            free(temp);
        }
    }
    free(map->buckets);
    free(map->dataNodes);
#ifdef USE_THREAD_SAFETY
    pthread_mutex_destroy(&map->mutex);
#endif
    unlock_map(map);
    free(map);
    // Optionally free the intern pool at program end.
    free_intern_pool();
}

