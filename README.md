# A simple NES emulator made for fun

Only NROM and MMC1 games are supported. 
Tested games include:
 - Donkey Kong 
 - Super Mario bros
 - The Legend of Zelda
 - Metroid

![image](https://github.com/user-attachments/assets/d3de111e-10fc-4376-b05f-115d6f2132f3)
![image](https://github.com/user-attachments/assets/76ef2188-00a5-4066-9791-753630d58c28)


# Usage

Run the executable with the argument -p=[PATH_TO_ROM]. A saves directory will automatically be created.

## Controls

Player 1
|Controller Button|Keyboard Key  |
|--|--|
|A  |J  |
|B|K
|Start|Escape
|Select|Left Shift
|Up|W
|Down|S
|Left|A
|Right|D

Player 2
|Controller Button|Keyboard Key  |
|--|--|
|A  |Period  |
|B|Comma
|Start|Enter
|Select|Right Shift
|Up|Up
|Down|Down
|Left|Left
|Right|Right


# Building

Build the repo by running the following commands

    git clone https://github.com/yoyyoy/NES-emulator.git
    cd NES-emulator
    g++ -std=c++17 -O3 src/*.cpp src/*/*.cpp -o NESemulator -lSDL2

# Limitations

 - With only NROM and MMC1 support, game selection is limited
 - DMC audio is broken and has been silenced.
 - Emulation is not clock accurate, things like Audio and Graphics are updated per scanline
 - CPU clock cycles are not counted accurately
