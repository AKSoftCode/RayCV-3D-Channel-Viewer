#pragma once

#include <vector>

class Utilities
{
public:
	static std::vector<std::string> Split(std::string s, std::string delimiter)
	{
		size_t pos_start = 0, pos_end, delim_len = delimiter.length();
		std::string token;
		std::vector<std::string> res;

		while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
		{
			token = s.substr(pos_start, pos_end - pos_start);
			pos_start = pos_end + delim_len;
			res.push_back(token);
		}

		res.push_back(s.substr(pos_start));
		return res;
	}

	static std::wstring stingtoWString(std::string value) 
	{
		const size_t cSize = value.size() + 1;

		std::wstring wc;
		wc.resize(cSize);

		size_t cSize1;
		mbstowcs_s(&cSize1, (wchar_t*)&wc[0], cSize, value.c_str(), cSize);

		wc.pop_back();

		return wc;
	}

	static double mapOneRangeToAnother(double sourceNumber, double fromA, double fromB, double toA, double toB, int decimalPrecision)
	{
		double deltaA = fromB - fromA;
		double deltaB = toB - toA;
		double scale = deltaB / deltaA;
		double negA = -1 * fromA;
		double offset = (negA * scale) + toA;
		double finalNumber = (sourceNumber * scale) + offset;
		int calcScale = (int)pow(10, decimalPrecision);
		return (double)round(finalNumber * calcScale) / calcScale;
	}

};