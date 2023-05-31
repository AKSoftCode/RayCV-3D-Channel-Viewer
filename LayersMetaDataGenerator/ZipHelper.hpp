// ZipHelper.hpp - Simple zip interface that takes a folder path to compress 
// the dataset in that folder.

#pragma once

#include <string>
#include <map>

class ZipHelper
{
public:
	ZipHelper();

	static bool Compress(std::wstring path, std::wstring outputFileName);

	static std::map<int, std::string> ReadZip(std::string zipPath);
};

