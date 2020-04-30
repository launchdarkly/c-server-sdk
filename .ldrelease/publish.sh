set -e

mkdir -p artifacts

base=$(pwd)

# cp build/libldserverapi.a artifacts/${LD_LIBRARY_FILE_PREFIX}-libldserverapi.a
# cp build/libldserverapidynamic.so artifacts/${LD_LIBRARY_FILE_PREFIX}-libldserverapidynamic.so
# build/stores/redis/libldserverapi-redis.a artifacts/${LD_LIBRARY_FILE_PREFIX}-libldserverapi-redis.a

cd "${base}/build-static/release"
zip -r "${base}/artifacts/${LD_LIBRARY_FILE_PREFIX}-static.zip" *
tar -cf "${base}/artifacts/${LD_LIBRARY_FILE_PREFIX}-static.tar" *

cd "${base}/build-dynamic/release"
zip -r "${base}/artifacts/${LD_LIBRARY_FILE_PREFIX}-dynamic.zip" *
tar -cf "${base}/artifacts/${LD_LIBRARY_FILE_PREFIX}-dynamic.tar" *

cd "${base}/build-redis-static/release"
zip -r "${base}/artifacts/${LD_LIBRARY_FILE_PREFIX}-redis-static.zip" *
tar -cf "${base}/artifacts/${LD_LIBRARY_FILE_PREFIX}-redis-static.tar" *

cd "${base}/build-redis-dynamic/release"
zip -r "${base}/artifacts/${LD_LIBRARY_FILE_PREFIX}-redis-dynamic.zip" *
tar -cf "${base}/artifacts/${LD_LIBRARY_FILE_PREFIX}-redis-dynamic.tar" *
