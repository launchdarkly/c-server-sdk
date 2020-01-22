mkdir -p artifacts

cp build/libldserverapi.a artifacts/${LD_LIBRARY_FILE_PREFIX}-libldserverapi.a
cp build/libldserverapidynamic.so artifacts/${LD_LIBRARY_FILE_PREFIX}-libldserverapi.so
cp build/stores/redis/libldserverapi-redis.a artifacts/${LD_LIBRARY_FILE_PREFIX}-libldserverapi-redis.a
