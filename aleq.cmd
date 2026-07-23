@echo off
setlocal

set "ROOT=%~dp0"
set "BUILD=%ROOT%build"
set "BUILD_LOG=%BUILD%\.aleq-build.log"

if not exist "%BUILD%" mkdir "%BUILD%"

if not exist "%BUILD%\CMakeCache.txt" (
    cmake -S "%ROOT%." -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release >"%BUILD_LOG%" 2>&1
    if errorlevel 1 (
        type "%BUILD_LOG%" 1>&2
        del /q "%BUILD_LOG%" >nul 2>&1
        exit /b 1
    )
)

cmake --build "%BUILD%" --config Release --target aleq --parallel >"%BUILD_LOG%" 2>&1
if errorlevel 1 (
    type "%BUILD_LOG%" 1>&2
    del /q "%BUILD_LOG%" >nul 2>&1
    exit /b 1
)
del /q "%BUILD_LOG%" >nul 2>&1

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
