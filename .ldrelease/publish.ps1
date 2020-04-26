
# Windows test script for PowerShell (just puts artifacts in output directory)

$ErrorActionPreference = "Stop"

$global:ProgressPreference = "SilentlyContinue"  # prevents console errors from CircleCI host

# the built in powershell zip seems to be horribly broken when working with nested directories
choco install zip

$prefix = $env:LD_LIBRARY_FILE_PREFIX  # set in .ldrelease/config.yml
New-Item -ItemType Directory -Force -Path .\artifacts

cd "build-static/release"
zip -r "../../artifacts/${prefix}-static.zip" *
cd ..
cd ..

cd "build-dynamic/release"
zip -r "../../artifacts/${prefix}-dynamic.zip" *
cd ..
cd ..
