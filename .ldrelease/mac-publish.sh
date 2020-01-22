mkdir -p artifacts

cp build/libldserverapi.a artifacts/${LD_LIBRARY_FILE_PREFIX}-libldserverapi.a
cp build/libldserverapidynamic.dylib artifacts/${LD_LIBRARY_FILE_PREFIX}-libldserverapi.dylib
cp build/stores/redis/libldserverapi-redis.a artifacts/${LD_LIBRARY_FILE_PREFIX}-libldserverapi-redis.a
