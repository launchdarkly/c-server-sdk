
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

If(Test-Path "curl-7.59.0") {
    Write-Host "Curl already present."
} Else {
    # NOTE: Recompute this SHA256 hash whenever the file is updated. This way we can detect
    # if the file has changed.
    $CurlSHA256 = "687b77fe00bc6f9dad5623742183028541b4d2c0f64bfd7e0acf7038fda27cdc"
    $CurlURL = "https://curl.haxx.se/download/curl-7.59.0.zip"

    DownloadAndUnzip -url $CurlURL -filename "curl.zip" -sha256 $CurlSHA256

    Write-Host
    Write-Host Building curl
    Push-Location
    cd curl-7.59.0/winbuild
    ExecuteOrFail { nmake /f Makefile.vc mode=static }
    Pop-Location
}

If(Test-Path "pcre-8.43") {
    Write-Host "PRCE already present."
} Else {
    # NOTE: Recompute this SHA256 hash whenever the file is updated. This way we can detect
    # if the file has changed.
    $PcreSHA256 = "ae236dc25d7e0e738a94e103218e0085eb02ff9bd98f637b6e061a48decdb433"
    $PcreURL = "https://sourceforge.net/projects/pcre/files/pcre/8.43/pcre-8.43.zip/download"

    DownloadAndUnzip -url $PcreURL -filename "pcre.zip" -sha256 $PcreSHA256

    Write-Host
    Write-Host Building PCRE
    Push-Location
    cd pcre-8.43
    New-Item -ItemType Directory -Force -Path .\build
    cd build
    ExecuteOrFail { cmake -G "Visual Studio 16 2019" -A x64 .. }
    ExecuteOrFail { cmake --build . }
    Pop-Location
}

function Build-Sdk {
    param (
        $OutputPath,
        $BuildStatic,
        $Config,
        $BuildTests
    )

    $BUILD_TESTING = If($BuildTests) {"ON"} Else {"OFF"}
    $BUILD_SHARED = If($BuildStatic) {"OFF"} Else {"ON"}

    Write-Host
    Write-Host Building SDK $(If($BuildStatic) {"statically"} Else {"dynamically"}) ${Config}
    Push-Location
    New-Item -ItemType Directory -Force -Path $OutputPath
    cd $OutputPath
    New-Item -ItemType Directory -Force -Path artifacts
    Write-Host
    ExecuteOrFail {
        cmake -G "Visual Studio 16 2019" -A x64 `
            -D ENABLE_CMAKE_PROJECT_TESTS=ON `
            -D BUILD_TESTING=${BUILD_TESTING} `
            -D BUILD_SHARED_LIBS=${BUILD_SHARED} `
            -D SKIP_DATABASE_TESTS=ON `
            -D CMAKE_INSTALL_PREFIX="${PSScriptRoot}/../${OutputPath}/artifacts" `
            -D CURL_LIBRARY="${PSScriptRoot}/../curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/lib/libcurl_a.lib" `
            -D CURL_INCLUDE_DIR="${PSScriptRoot}/../curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/include" `
            -D PCRE_LIBRARY="${PSScriptRoot}/../pcre-8.43/build/Debug/pcred.lib" `
            -D PCRE_INCLUDE_DIR="${PSScriptRoot}/../pcre-8.43/build" `
            ..
    }
    ExecuteOrFail { cmake --build . --config $Config }
    ExecuteOrFail { cmake --build . --config $Config --target install }
    Pop-Location
}

Build-Sdk ./build-static $true "Debug" $true
Build-Sdk ./build-dynamic $false "Debug" $false
Build-Sdk ./build-static-release $true "Release" $false
Build-Sdk ./build-dynamic-release $false "Release" $false

# Copy the binary archives to the artifacts directory; Releaser will find them
# there and attach them to the release (this also makes the artifacts available
# in dry-run mode)

# Check if the Releaser variable exists because this script could also be called
# from CI
if ("${env:LD_RELEASE_ARTIFACTS_DIR}" -ne "") {
    $prefix = $env:LD_LIBRARY_FILE_PREFIX  # set in .ldrelease/config.yml

    cd "build-static/artifacts"
    zip -r "${env:LD_RELEASE_ARTIFACTS_DIR}/${prefix}-static.zip" *
    cd ..
    cd ..

    cd "build-static-release/artifacts"
    zip -r "${env:LD_RELEASE_ARTIFACTS_DIR}/${prefix}-static-release.zip" *
    cd ..
    cd ..


    cd "build-dynamic/artifacts"
    zip -r "${env:LD_RELEASE_ARTIFACTS_DIR}/${prefix}-dynamic.zip" *
    cd ..
    cd ..

    cd "build-dynamic-release/artifacts"
    zip -r "${env:LD_RELEASE_ARTIFACTS_DIR}/${prefix}-dynamic-release.zip" *
    cd ..
    cd ..
}
