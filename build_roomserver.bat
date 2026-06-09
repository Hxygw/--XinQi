@echo off
chcp 65001 >nul
echo ==========================================
echo  芯棋 RoomServer 构建与打包脚本
echo  XinQi RoomServer Build ^& Package Script
echo ==========================================
echo.
echo 需要: Visual Studio 2022 (v145) + Node.js 18+
echo Requires: Visual Studio 2022 (v145) + Node.js 18+
echo.

set MSBUILD="D:\Program Files\VisualStudio\MSBuild\Current\Bin\MSBuild.exe"

REM ---- 步骤 1: 构建 C++ 依赖（XinQiCore） ----
echo [1/4] 构建 XinQiCore (核心引擎)...
%MSBUILD% "%~dp0XinQiCore\XinQiCore.vcxproj" /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
if %ERRORLEVEL% neq 0 (
    echo 构建 XinQiCore 失败！
    pause
    exit /b 1
)

REM ---- 步骤 2: 构建 RoomServer ----
echo.
echo [2/4] 构建 XinQiRoomServer (房间服务器)...
%MSBUILD% "%~dp0XinQiRoomServer\XinQiRoomServer.vcxproj" /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
if %ERRORLEVEL% neq 0 (
    echo 构建 RoomServer 失败！
    pause
    exit /b 1
)

REM ---- 步骤 3: 构建前端 ----
echo.
echo [3/4] 构建房间前端 (xinqi-room-frontend)...
cd /d "%~dp0xinqi-room-frontend"
call npm install
call npm run build
cd /d "%~dp0"

REM ---- 步骤 4: 打包 ----
echo.
echo [4/4] 打包发布...
set RELEASE_DIR=%~dp0XinQiRoomServer\x64\Release
set PKG_NAME=XinQiRoomServer-v0.1.0
set ZIP_PATH=%~dp0%PKG_NAME%.zip
set TMP_DIR=%~dp0RoomPkg

REM 复制前端到 Release 目录
xcopy /E /Y "%~dp0xinqi-room-frontend\dist" "%RELEASE_DIR%\dist\" >nul

REM 创建临时打包目录
if exist "%TMP_DIR%" rmdir /S /Q "%TMP_DIR%"
mkdir "%TMP_DIR%\%PKG_NAME%"

REM 复制 exe
copy "%RELEASE_DIR%\XinQiRoomServer.exe" "%TMP_DIR%\%PKG_NAME%\" >nul

REM 复制前端 dist
xcopy /E /Y "%RELEASE_DIR%\dist" "%TMP_DIR%\%PKG_NAME%\dist\" >nul

REM 删除旧 zip
if exist "%ZIP_PATH%" del "%ZIP_PATH%"

REM 打包
tar -caf "%ZIP_PATH%" -C "%TMP_DIR%" "%PKG_NAME%"

REM 清理
rmdir /S /Q "%TMP_DIR%"

echo.
echo ==========================================
echo  构建完成！输出文件: %ZIP_PATH%
echo  Build complete! Output: %ZIP_PATH%
echo ==========================================
echo.
echo ZIP 内容:
tar -tf "%ZIP_PATH%"

pause
