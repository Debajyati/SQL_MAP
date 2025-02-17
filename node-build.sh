#!/bin/bash

emcc ./SQL_MAP_PRODUCTION/SQL_MAP.c -o sql_map.js \
     -s EXPORTED_FUNCTIONS='[_sql_map_create, _sql_map_put, _sql_map_get, _sql_map_remove, _sql_map_free, _malloc, _free]' \
     -s EXPORTED_RUNTIME_METHODS='ccall,cwrap' \
     -pthread \
     -s PTHREAD_POOL_SIZE=1 \
     -s MODULARIZE=1 \
     -s ENVIRONMENT='node'
