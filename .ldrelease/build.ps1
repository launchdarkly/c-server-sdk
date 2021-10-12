
# Windows build script for PowerShell

$ErrorActionPreference = "Stop"

$global:ProgressPreference = "SilentlyContinue"  # prevents console errors from CircleCI host

# The built in powershell zip seems to be horribly broken when working with nested directories.
# We only need this if we're doing a release as opposed to just building in CI
if ("${env:LD_RELEASE_ARTIFACTS_DIR}" -ne "") {
    choco install zip    
}

Import-Module ".\.ldrelease\helpers.psm1" -Force

SetupVSToolsEnv -architecture amd64

DownloadAndUnzip -url "https://curl.haxx.se/download/curl-7.59.0.zip" -filename "curl.zip"
DownloadAndUnzip -url "https://ftp.pcre.org/pub/pcre/pcre-8.43.zip" -filename "pcre.zip"

Write-Host
Write-Host Building curl
Push-Location
cd curl-7.59.0/winbuild
ExecuteOrFail { nmake /f Makefile.vc mode=static }
Pop-Location

Write-Host
Write-Host Building PCRE
Push-Location
cd pcre-8.43
New-Item -ItemType Directory -Force -Path .\build
cd build
ExecuteOrFail { cmake -G "Visual Studio 16 2019" -A x64 .. }
ExecuteOrFail { cmake --build . }
Pop-Location

Write-Host
Write-Host Building SDK statically
Push-Location
New-Item -ItemType Directory -Force -Path ".\build-static"
cd "build-static"
New-Item -ItemType Directory -Force -Path release
Write-Host
ExecuteOrFail {
    cmake -G "Visual Studio 16 2019" -A x64 `
        -D SKIP_DATABASE_TESTS=ON `
        -D CMAKE_INSTALL_PREFIX="${PSScriptRoot}/../build-static/release" `
        -D CURL_LIBRARY="${PSScriptRoot}/../curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/lib/libcurl_a.lib" `
        -D CURL_INCLUDE_DIR="${PSScriptRoot}/../curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/include" `
        -D PCRE_LIBRARY="${PSScriptRoot}/../pcre-8.43/build/Debug/pcred.lib" `
        -D PCRE_INCLUDE_DIR="${PSScriptRoot}/../pcre-8.43/build" `
        ..
}
ExecuteOrFail { cmake --build . }
ExecuteOrFail { cmake --build . --target install }
Pop-Location

Write-Host
Write-Host Building SDK dynamically
Push-Location
New-Item -ItemType Directory -Force -Path ".\build-dynamic"
cd "build-dynamic"
New-Item -ItemType Directory -Force -Path release
ExecuteOrFail {
    cmake -G "Visual Studio 16 2019" -A x64 `
        -D BUILD_TESTING=OFF `
        -D BUILD_SHARED_LIBS=ON `
        -D CMAKE_INSTALL_PREFIX="${PSScriptRoot}/../build-dynamic/release" `
        -D CURL_LIBRARY="${PSScriptRoot}/../curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/lib/libcurl_a.lib" `
        -D CURL_INCLUDE_DIR="${PSScriptRoot}/../curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/include" `
        -D PCRE_LIBRARY="${PSScriptRoot}/../pcre-8.43/build/Debug/pcred.lib" `
        -D PCRE_INCLUDE_DIR="${PSScriptRoot}/../pcre-8.43/build" `
        ..
}
ExecuteOrFail { cmake --build . }
ExecuteOrFail { cmake --build . --target install }
Pop-Location

# Copy the binary archives to the artifacts directory; Releaser will find them
# there and attach them to the release (this also makes the artifacts available
# in dry-run mode)

# Check if the Releaser variable exists because this script could also be called
# from CI
if ("${env:LD_RELEASE_ARTIFACTS_DIR}" -ne "") {
    $prefix = $env:LD_LIBRARY_FILE_PREFIX  # set in .ldrelease/config.yml

    cd "build-static/release"
    zip -r "${env:LD_RELEASE_ARTIFACTS_DIR}/${prefix}-static.zip" *
    cd ..
    cd ..

    cd "build-dynamic/release"
    zip -r "${env:LD_RELEASE_ARTIFACTS_DIR}/${prefix}-dynamic.zip" *
    cd ..
    cd ..
}
