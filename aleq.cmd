@echo off
setlocal

set "ROOT=%~dp0"
set "BUILD=%ROOT%build"

if not exist "%BUILD%" mkdir "%BUILD%"

if not exist "%BUILD%\CMakeCache.txt" (
    cmake -S "%ROOT%." -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release >nul 2>&1
    if errorlevel 1 (
        cmake -S "%ROOT%." -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release
        if errorlevel 1 exit /b 1
    )
)

cmake --build "%BUILD%" --config Release --target aleq --parallel >nul 2>&1
if errorlevel 1 (
    cmake --build "%BUILD%" --config Release --target aleq --parallel
    if errorlevel 1 exit /b 1
)

set "EXE="
if exist "%BUILD%\aleq.exe" set "EXE=%BUILD%\aleq.exe"
if not defined EXE if exist "%BUILD%\Release\aleq.exe" set "EXE=%BUILD%\Release\aleq.exe"

if not defined EXE (
    echo error: built aleq executable was not found 1>&2
    exit /b 1
)

if "%~1"=="" (
    echo Built aleq: "%EXE%"
    exit /b 0
)

"%EXE%" %*
exit /b %errorlevel%
