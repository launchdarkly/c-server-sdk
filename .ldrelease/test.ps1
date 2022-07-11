
# Windows test script for PowerShell

$ErrorActionPreference = "Stop"

Import-Module ".\.ldrelease\helpers.psm1" -Force

SetupVSToolsEnv -architecture amd64

cd build-static
$env:GTEST_OUTPUT="xml:${PSScriptRoot}/../reports/"
ExecuteOrFail { cmake -E env CTEST_OUTPUT_ON_FAILURE=1 cmake --build . --target RUN_TESTS }
cd ..
