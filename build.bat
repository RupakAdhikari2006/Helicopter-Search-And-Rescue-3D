@echo off
echo ==============================================
echo Compiling 3D Helicopter Search ^& Rescue
echo ==============================================

:: Check if GCC exists (MinGW)
gcc --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] GCC compiler not found in PATH!
    echo Please install MinGW and make sure gcc is in your system PATH environment variable via Control Panel.
    echo.
    echo If you use Code::Blocks or Dev-C++, you can also compile this by adding all .c and .h files to a new project and linking -lopengl32 -lglu32 -lfreeglut
    pause
    exit /b
)

:: Compile the codebase
:: Make sure freeglut is installed and the compiler can find the headers and binary libs
gcc main.c helicopter.c terrain.c camera.c -o helicopter_game.exe -I. -Ifreeglut-MinGW\freeglut\include -Lfreeglut-MinGW\freeglut\lib -lopengl32 -lglu32 -lfreeglut

:: Note: Depending on your mingw setup, freeglut might just be -lglut32 or -lfreeglut, and might need header include directories.
:: If you get missing freeglut errors, you might need to copy GLUT files into MinGW's lib/ and include/ dirs.

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Compilation failed. Ensure that you have freeglut installed for MinGW and linked properly.
    echo A popular alternative is to import these source files into a Dev-C++ or CodeBlocks OpenGL project.
    pause
    exit /b
)

echo.
echo [SUCCESS] Build complete! 
echo Run the game by double clicking on helicopter_game.exe or typing helicopter_game.exe
echo.
pause
