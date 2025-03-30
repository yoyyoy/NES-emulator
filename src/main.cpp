#include "nes.h"
#include <filesystem>

using namespace std;

int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_EVERYTHING);
    filesystem::create_directory("saves");
    
    string romPath="";
    for(int i=0; i<argc; i++)
    {
        string argument(argv[i]);
        if(argument.length()>3 && argument[0]=='-' && argument[1]=='p' && argument[2]=='=')
        {
            romPath=argument.substr(3);
            break;
        }
    }

    if(romPath=="")
    {
        cerr << "No rom path specified. use -p=<PATH TO ROM>\n";
        return 1;
    }
    ifstream romFile(romPath, ifstream::basic_ios::binary);
    if(!romFile.is_open())
    {
        cerr << "Unable to read file " << romPath << '\n';
        return 2;
    }
    
    filesystem::path filePath = romPath;

    NES nes(romFile, filePath.stem());
    nes.Run();
    SDL_Quit();
}
