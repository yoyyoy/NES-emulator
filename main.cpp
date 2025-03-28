#include "nes.h"

using namespace std;

int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_EVERYTHING);

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
    
    

    NES nes(romFile);
    nes.Run();
    SDL_Quit();
}
