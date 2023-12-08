#include "LayersCompession.hpp"

#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "external/qoi.h"

#include "LayersCompession.hpp"
#include "Utilities.h"
#include "ZipHelper.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <future>
#include <unordered_map>

using namespace cv;
using namespace std;
using namespace LayersMetaData;

namespace
{
	std::future<void> initialLoadSync_;

	static std::unordered_map<string, Mat> layersCache_;

	const std::wstring tempImagesPath = L"TempImages";

	const auto tempDirPath = std::filesystem::temp_directory_path().append(L"Layers");

	std::tuple<int, string> PopulateChannelData(string filename)
	{
		string delimiter = "_";
		size_t pos_start = 0, pos_end, delim_len = delimiter.length();
		string token;
		vector<string> res;

		InputImages inputImages;
		while ((pos_end = filename.find(delimiter, pos_start)) != string::npos) {
			token = filename.substr(pos_start, pos_end - pos_start);
			pos_start = pos_end + delim_len;
			res.push_back(token);
		}

		res.push_back(filename.substr(pos_start));

		if (res.size() != 3 && res.front() != "Layer")
		{
			throw std::logic_error("File format is wrong");
		}

		std::string channel = "";
		if (res.back() == "r" || res.back() == "g" || res.back() == "b" || res.back() == "a")
		{
			channel = res.back();
		}
		else
		{
			throw std::logic_error("File format is wrong");
		}

		int index = stoi(res.at(1));

		return { index, channel };
	}

	std::byte* matToBytes(Mat image)
	{
		size_t size = image.total() * image.elemSize();
		std::byte* bytes = new std::byte[size];  // you will have to delete[] that later
		std::memcpy(bytes, image.data, size * sizeof(std::byte));
		return bytes;
	}

	Mat bytesToMat(RawData bytes, int width, int height, int channel)
	{
		Mat image = Mat(height, width, CV_8UC(channel), bytes).clone(); // make a copy
		return image;
	}
}

LayersMetaData::ILayersCompession& getLayerImageInstance()
{
	static LayersCompression layersImage;
	return layersImage;
}

LayersCompression::LayersCompression()
{

}

LayersCompression::~LayersCompression()
{
	StopFileWatcherService();
}

void LayersCompression::CleanUp()
{
	layersCache_.clear();
}

bool LayersCompression::fileNameFormatCheck(std::filesystem::path filePath, int& layerIndex)
{
	std::cout << "Directory: " << filePath << std::endl;
	auto [index, channel] = PopulateChannelData(filePath.filename().stem().string());
	layerIndex = index;
	auto& inputImage = mapInputImages_[index];
	inputImage.index = index;
	if (channel == "r")
	{
		inputImage.rChPath = filePath.string();
	}
	else if (channel == "g")
	{
		inputImage.gChPath = filePath.string();
	}
	else if (channel == "b")
	{
		inputImage.bChPath = filePath.string();
	}
	else if (channel == "a")
	{
		inputImage.aChPath = filePath.string();
	}

	return !inputImage.rChPath.empty() && !inputImage.gChPath.empty() && !inputImage.bChPath.empty() && !inputImage.aChPath.empty();
}

void LayersCompression::PreprocessLayerMetaData(std::wstring inputFolderPath, std::wstring zipPath)
{
	mergeImageFut_ = std::async(std::launch::async, [&]() {
		for (auto& dirEntry : std::filesystem::recursive_directory_iterator(inputFolderPath))
		{
			if (dirEntry.is_regular_file())
			{
				auto filePath = dirEntry.path();
				try
				{
					int index = 0;
					if (fileNameFormatCheck(filePath, index))
					{
						auto& inputImage = mapInputImages_[index];
						auto mergedImage = mergeImages(inputImage);
						copyMergedImage(mergedImage, index, tempImagesPath);
					}
				}
				catch (std::exception ex)
				{
					std::cout << "Skipping Layer as exception Occured in " << filePath.string() << " : " << ex.what() << std::endl;
				}
			}
		}
		});

	mergeImageFut_.get();

	ZipHelper::Compress(tempImagesPath, zipPath);

	std::filesystem::remove_all(tempImagesPath);

}

void LayersCompression::RegisterForFileUpdates(CallbackFiler callback)
{
	filewatcherCallback_ = callback;
}

void LayersCompression::FileWatcherEventActionCallback(const std::filesystem::path& path, const filewatch::Event change_type, const LayersFileMap layerMap)
{
	auto getIndexFromFileName = [](const std::string& fileName)
		{
			int index = 0;
			string delimiter = "_";
			size_t pos_start = 0, pos_end, delim_len = delimiter.length();
			string token;
			vector<string> res;

			InputImages inputImages;
			while ((pos_end = fileName.find(delimiter, pos_start)) != string::npos) {
				token = fileName.substr(pos_start, pos_end - pos_start);
				pos_start = pos_end + delim_len;
				res.push_back(token);
			}

			res.push_back(fileName.substr(pos_start));

			if (res.size() != 3 && res.front() != "Layer")
			{
				throw std::logic_error("File format is wrong");
			}

			return stoi(res.back());
		};

	try
	{
		if (path.extension() != L".qoi")
			return;

		switch (change_type)
		{
		case filewatch::Event::initial:
		{
			filewatcherCallback_(std::filesystem::path(fileWatcherPath_).append(path.string()), 0, FileWatcherEvents::initial, layerMap);
			break;
		}
		case filewatch::Event::added:
		{

			filewatcherCallback_(std::filesystem::path(fileWatcherPath_).append(path.string()), getIndexFromFileName(path.string()), FileWatcherEvents::added, layerMap);
			break;
		}
		case filewatch::Event::removed:
		{
			filewatcherCallback_(std::filesystem::path(fileWatcherPath_).append(path.string()), getIndexFromFileName(path.string()), FileWatcherEvents::removed, layerMap);
			break;
		}
		case filewatch::Event::renamed_old:
			std::cout << "The file was renamed and this is the old name." << '\n';
			break;
		case filewatch::Event::renamed_new:
			std::cout << "The file was renamed and this is the new name." << '\n';
			break;
		};
	}
	catch (...)
	{
		std::cout << "Unhandled Exception in LayersCompression::FileWatcherEventActionCallback" << std::endl;
	}

}
void LayersCompression::StartFileWatcherService(std::wstring inputSourcePath)
{
	try
	{
		fileWatcherPath_ = inputSourcePath;
		if (!std::filesystem::exists(inputSourcePath))
		{
			std::filesystem::create_directories(inputSourcePath);
		}


		initialLoadSync_ = std::async(std::launch::async, [&]() {
			//LoadCurrent Data
			std::map<int, std::string> layers;
			using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
			for (const auto& dirEntry : recursive_directory_iterator(inputSourcePath))
			{
				if (dirEntry.is_regular_file())
				{
					auto fileName = dirEntry.path().filename().stem().string();
					auto values = Utilities::Split(fileName, "_");
					layers[std::stoi(values.at(2))] = dirEntry.path().string();
				}
			}

			FileWatcherEventActionCallback("temp.qoi", filewatch::Event::initial, layers);

			});


		fileWatcher_ = std::make_unique<filewatch::FileWatch<std::filesystem::path>>(
			inputSourcePath,
			[&](const std::filesystem::path& path, const filewatch::Event change_type)
			{
				FileWatcherEventActionCallback(path, change_type, LayersFileMap{});
			}
		);
	}
	catch (...)
	{
		std::cout << "Unhandled Exception in StartFileWatcherService" << std::endl;
	}
}

void LayersCompression::StopFileWatcherService()
{
	if (fileWatcher_)
		fileWatcher_.release();

	if (!std::filesystem::exists(tempDirPath))
	{
		std::filesystem::remove_all(tempDirPath);
	}

}

void LayersCompression::showSplitImages(std::string image)
{
	Mat myImage;//declaring a matrix to load the image//
	Mat different_Channels[4];//declaring a matrix with three channels// 

	myImage = readImage(image);

	split(myImage, different_Channels);
	Mat B = different_Channels[0];//loading blue channels//
	Mat G = different_Channels[1];//loading green channels//
	Mat R = different_Channels[2];//loading red channels//  
	Mat A = different_Channels[3];//loading alpha channels//  

	std::cout << "Red:   " << R.size().width << " x " << R.size().height << " x " << R.channels() << std::endl;
	std::cout << "Green: " << G.size().width << " x " << G.size().height << " x " << G.channels() << std::endl;
	std::cout << "Blue:  " << B.size().width << " x " << B.size().height << " x " << B.channels() << std::endl;
	std::cout << "Alpha:  " << A.size().width << " x " << A.size().height << " x " << A.channels() << std::endl;

	if (1)
	{
		imshow("Blue Channel", B);//showing Blue channel//
		imshow("Green Channel", G);//showing Green channel//
		imshow("Red Channel", R);//showing Red channel//
		//imshow("Red Channel", A);//showing Red channel//
	}


	imshow("Actual Image", myImage);//showing Red channel//
	waitKey(0);//wait for key stroke
	destroyAllWindows();//closing all windows//
}

LayerMap LayersCompression::GetLayersFromZip(std::string zipPath, ZippiCallback callback)
{
	auto layers = ZipHelper::ReadZip(zipPath);
	callback(true);
	return layers;
}


Mat LayersCompression::readImage(std::string imageFileName, int flags)
{
	Mat image;
	try
	{
		auto ext = std::filesystem::path(imageFileName).extension();
		if (std::filesystem::path(imageFileName).extension() == L".qoi")
		{
			qoi_desc desc = { 0 };
			RawData img_data = qoi_read(imageFileName.c_str(), &desc, 0);
			int channels = desc.channels;
			int w = desc.width;
			int h = desc.height;

			Mat tempMat = bytesToMat(img_data, desc.width, desc.height, desc.channels);

			if (flags == 0)
				cv::cvtColor(tempMat, image, cv::COLOR_RGBA2GRAY);
			else
				image = tempMat;

			free(img_data);
			img_data = NULL;
		}
		else
		{
			// Read all the channel images
			image = imread(imageFileName, flags);

		}
	}
	catch (...)
	{
		std::cout << "Not able to read the file: " << imageFileName << std::endl;
	}

	return image;
};




void LayersCompression::SplitImageIntoChannels(std::string baseImagePath, SplitCallback callback)
{
	try
	{
		Mat sourceImage;//declaring a matrix to load the image//
		Mat different_Channels[4];//declaring a matrix with four channels// 

		auto it = layersCache_.find(baseImagePath);
		if (it != layersCache_.end())
		{
			sourceImage = it->second;
		}
		else
		{
			sourceImage = readImage(baseImagePath);

			if (sourceImage.data != NULL)
				layersCache_.emplace(baseImagePath, sourceImage); // RR: this is crashing in some runs with debug
			else
				return;
		}

		cv::cvtColor(sourceImage, sourceImage, cv::COLOR_BGRA2RGBA);

		split(sourceImage, different_Channels);
		std::vector<RawData> channelData;

		for (int i = 0; i < 4; ++i)
		{
			ProcessSplit(i, different_Channels[i], callback);
		}

	}
	catch (...)
	{
		std::cout << "Unhandled exception occured in LayersCompression::SplitImageIntoChannels" << std::endl;
	}
}

bool CheckThreshold(int cIndex, const ThresholdSettings& ts)
{
	if (ts.threshFlag_ != 0)
	{
		if (cIndex == 1) return false;

		switch (cIndex)
		{
		case 0:
			if (!(ts.threshFlag_ & ThresholdingEnum::eRThreshChanged)) return false;
			std::cout << "R Thresh values Changed" << std::endl;
			break;
		case 2:
			if (!(ts.threshFlag_ & ThresholdingEnum::eBThreshChanged)) return false;
			std::cout << "B Thresh values Changed" << std::endl;
			break;
		case 3:
			if (!(ts.threshFlag_ & ThresholdingEnum::eAThreshChanged)) return false;
			std::cout << "A Thresh values Changed" << std::endl;
			break;
		}
	}

	return true;
}

bool SetupThreshold(int cIndex, cv::Mat& channel, const ThresholdSettings& ts)
{
	if (cIndex == 1) return false;

	auto beginThresholdMapping = Utilities::mapOneRangeToAnother(ts.ThresholdBegin_[cIndex], 0, 1.0f, 0, 255.0f, 3);
	auto endThresholdMapping = Utilities::mapOneRangeToAnother(ts.ThresholdEnd_[cIndex], 0, 1.0f, 0, 255.0f, 3);

	// This can be heavily optimised with massively parallel pixel changes

	for (int k = 0; k < channel.rows; k++)
	{
		for (int j = 0; j < channel.cols; j++)
		{
			int editValue = channel.at<uchar>(k, j);
			auto bFlip = ts.flipThreshold_[cIndex];
			bool inRange = (editValue >= beginThresholdMapping) && (editValue <= endThresholdMapping);

			if (!bFlip && !inRange) channel.at<uchar>(k, j) = 0;
			else if (bFlip && !inRange) channel.at<uchar>(k, j) = 0;
		}
	}

	return true;
}

Mat BuildMerged(int cIndex, cv::Mat& channel)
{
	Mat alpha;
	cv::threshold(channel, alpha, 0, 255, cv::THRESH_BINARY);

	Mat otherChannel;
	cv::threshold(channel, otherChannel, 0, 0, cv::THRESH_BINARY);

	std::vector<cv::Mat> merged;

	switch (cIndex)
	{
	case 2:
	{
		merged = { channel, otherChannel, otherChannel, alpha };
	}
	break;
	case 1:
	{
		for (int k = 0; k < channel.rows; k++)
		{
			for (int j = 0; j < channel.cols; j++)
			{
				int editValue = channel.at<uchar>(k, j);

				if (editValue != 0) alpha.at<uchar>(k, j) = 10;
				else alpha.at<uchar>(k, j) = 0;
			}
		}
		merged = { otherChannel,  channel, otherChannel, alpha };
	}
	break;
	case 0:
	{
		merged = { otherChannel, otherChannel, channel , alpha };
	}
	break;
	case 3:
	{
		merged = { alpha, alpha, alpha, channel };
	}
	break;
	}

	Mat mergedImage;
	mergedImage.zeros(channel.rows, channel.cols, CV_8UC(4));
	merge(merged, mergedImage);
	cv::cvtColor(mergedImage, mergedImage, cv::COLOR_BGRA2RGBA);

	channel.release();
	alpha.release();
	otherChannel.release();

	for (Mat& image : merged)
		image.release();

	return mergedImage;
}


bool LayersCompression::ProcessSplit(int cIndex, cv::Mat& channel, SplitCallback callback)
{
	if (!CheckThreshold(cIndex, tsApplied_)) return false;

	if (cIndex != 1) SetupThreshold(cIndex, channel, tsApplied_);

	Mat mergedImage = BuildMerged(cIndex, channel);
	auto imageData = matToBytes(mergedImage);

	int channels = 0;
	channels = mergedImage.channels();
	// Force all odd encodings to be RGBA
	if (mergedImage.channels() != 3) {
		channels = 4;
	}

	qoi_desc desc;
	desc.width = mergedImage.size().width;
	desc.height = mergedImage.size().height;
	desc.channels = 4;
	desc.colorspace = QOI_SRGB;
	int size;
	RawData encoded = qoi_encode(imageData, &desc, &size);
	callback(cIndex, size, encoded);
	free(imageData);
	imageData = NULL;
	//cachedSetting_ = thresholdSettings;

	mergedImage.release();

	return true;
}



std::wstring LayersCompression::copyMergedImage(Mat mergedImage, int index, std::wstring toCopyPath)
{
	std::cout << "mergedImage Image:  " << mergedImage.size().width << " x " << mergedImage.size().height << " x " << mergedImage.channels() << std::endl;
	//! Create output folder is not present
	if (!std::filesystem::exists(toCopyPath))
	{
		std::filesystem::create_directories(toCopyPath);
	}

	auto imageData = matToBytes(mergedImage);

	char buf[20];
	snprintf(buf, sizeof(buf), "%04d", index);
	std::wstring paddedIndex = Utilities::stingtoWString(buf);

	std::wstring opImageFullPathQoi = toCopyPath + L"\\Layer_rgba_" + paddedIndex + L".qoi";
	int channels = 0;
	channels = mergedImage.channels();
	// Force all odd encodings to be RGBA
	if (mergedImage.channels() != 3) {
		channels = 4;
	}

	qoi_desc desc;
	desc.width = mergedImage.size().width;
	desc.height = mergedImage.size().height;
	desc.channels = 4;
	desc.colorspace = QOI_SRGB;

	qoi_write(std::string(opImageFullPathQoi.begin(), opImageFullPathQoi.end()).c_str(), imageData, &desc);

	free(imageData);
	imageData = NULL;

	return opImageFullPathQoi;
}

Mat LayersCompression::mergeImages(InputImages metaData)
{
	Mat rCh, bCh, gCh, aCh;
	Mat mergedImage;

	// Read all the channel images
	rCh = readImage(metaData.rChPath, 0);
	bCh = readImage(metaData.gChPath, 0);
	gCh = readImage(metaData.bChPath, 0);
	aCh = readImage(metaData.aChPath, 0);


	// Get dimension of final image
	int rows = max(rCh.rows, bCh.rows);
	rows = max(rows, gCh.rows);
	rows = max(rows, aCh.rows);
	int cols = rCh.cols + bCh.cols + gCh.cols + aCh.rows;

	// Create a black image
	auto _ = mergedImage.zeros(rows, cols, CV_8UC(4));

	gCh.copySize(rCh);
	bCh.copySize(rCh);
	aCh.copySize(rCh);


	// Merged Image
	std::vector<cv::Mat> array_to_merge = { bCh , gCh, rCh, aCh };

	cv::merge(array_to_merge, mergedImage);

	return mergedImage;
}
