// 3DViewExplorer Application Entry Point Function is in this file

#include <string>
#include "RayLibEngine.h"
#include "tortellini.hh"
#include <fstream>

using namespace ViewExplorer;


//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    RayLibEngine engine; 

    tortellini::ini ini;

    std::ifstream in("3DViewExplorer.ini");
    in >> ini;

    std::string filePath = ini["Paths"]["Zip"] | "";
    std::string folderWatchPath = ini["Paths"]["FolderWatch"] | "";

    double zHeight = ini["Layer"]["zHeightScale"] | 1.0;

    engine.InitMultiLayerModel(filePath.data(), folderWatchPath.c_str(), zHeight);

    while (!engine.WindowClose())    // Detect window close button or ESC key
    {
        engine.RenderLoop();
    }

    in.close();

    return 0;
}