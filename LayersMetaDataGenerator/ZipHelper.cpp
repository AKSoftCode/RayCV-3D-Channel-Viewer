#include "ZipHelper.hpp"
#include "Utilities.h"

#include <zip_file.hpp>
#include <filesystem>
using namespace std;

namespace
{
    // for string delimiter

}

ZipHelper::ZipHelper()
{
}

bool ZipHelper::Compress(std::wstring path, std::wstring outputFileName)
{
    miniz_cpp::zip_file zfile;

    for (auto& dirEntry : std::filesystem::recursive_directory_iterator(path)) {
        if (!dirEntry.is_regular_file()) {
            continue;
        }
        std::filesystem::path file = dirEntry.path();
        zfile.write(file.string());
    }

    zfile.save(std::string(outputFileName.begin(), outputFileName.end()));
    return false;
}

std::map<int, std::string> ZipHelper::ReadZip(std::string zipPath)
{
    std::map<int, std::string> layers;

    miniz_cpp::zip_file file(zipPath);

   std::string extractTempPath = std::filesystem::temp_directory_path().append("Layers").string();


   //! Create output folder is not present
   if (!std::filesystem::exists(extractTempPath))
   {
       std::filesystem::create_directories(extractTempPath);
   }
   else
   {
       std::filesystem::remove_all(extractTempPath);
       std::filesystem::create_directories(extractTempPath);
   }

   file.extractall(extractTempPath);

   using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
   for (const auto& dirEntry : recursive_directory_iterator(extractTempPath))
   {
       if (dirEntry.is_regular_file())
       {
           auto fileName = dirEntry.path().filename().stem().string();
           auto values = Utilities::Split(fileName, "_");
           layers[std::stoi(values.at(2))] = dirEntry.path().string();
       }
   }

   return layers;
}