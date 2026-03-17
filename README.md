# 3D Helicopter Search & Rescue

A 3D OpenGL helicopter game powered by `freeglut` where you rescue stranded survivors from intensely varied biomes (Ice Peaks, Tropical Islands, Lush Highlands, and Deep Swamps).

## How to Play

### Pre-Compiled Executable
Simply double-click on `helicopter_game.exe` to run the game immediately. 
Note: Ensure `freeglut.dll` is kept in the same folder as the executable.

### Compiling from Source
If you wish to re-compile the source code:
1. Ensure you have MinGW `gcc` installed on your system and added to your PATH.
2. Double-click on `build.bat` in this directory.
3. The script will automatically link against the provided `freeglut-MinGW` libraries and generate a fresh `helicopter_game.exe`.

## Controls
- **W / S**: Pitch forward / backward
- **A / D**: Bank left / right
- **Up / Down Arrows**: Increase / Decrease altitude (throttle)
- **Left / Right Arrows**: Yaw left / right (turn helicopter)
- **Spacebar**: Toggle Engine ON / OFF
- **C**: Cycle camera modes (Third Person, Cockpit, Cinematic)
- **Esc**: Exit Game

Enjoy the flight and avoid the solid mountains!
