
# Windows test script for PowerShell (just puts artifacts in output directory)

$prefix = $env:LD_LIBRARY_FILE_PREFIX  # set in .ldrelease/config.yml
New-Item -ItemType Directory -Force -Path .\artifacts

cp ldserverapidynamic.dll ".\artifacts\$prefix-ldserverapi-dynamic.dll"
cp ldserverapidynamic.lib ".\artifacts\$prefix-ldserverapi-dynamic.lib"
cp ldserverapi.lib ".\artifacts\$prefix-ldserverapi-static.lib"
