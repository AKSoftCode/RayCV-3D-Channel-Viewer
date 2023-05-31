// LayerCompressor.hpp - this file takes a collection of channel images (as r, g, b and a) with a specific
// naming convention in a pre-defined folder path and then compresses them in to a single image that has
// all the 4 channels encoded into it. The layers are then used to compress into a zip archive
// This is the concrete implementation of ILayersCompession
#pragma once
#include "include/ILayersCompession.h"

#include <string>
#include <map>
#include <future>
#include <filesystem>
#include <memory>

// Uses opencv4.2 as part of the nuget packages configured to this project (LayerMetaDataGenerator)
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>

#include "FileWatch.hpp"

namespace LayersMetaData
{
	struct InputImages
	{
		std::string rChPath;
		std::string gChPath;
		std::string bChPath;
		std::string aChPath;
		int index = -1;
	};

	class LayersCompression : public ILayersCompession
	{
	public:
		LayersCompression();

		virtual ~LayersCompression();

		virtual void CleanUp() override;

		virtual void PreprocessLayerMetaData(std::wstring inputFolderPath, std::wstring zipPath) override;

		virtual void RegisterForFileUpdates(CallbackFiler callback) override;

		virtual void StartFileWatcherService(std::wstring path) override;

		virtual void StopFileWatcherService() override;

		virtual LayerMap GetLayersFromZip(std::string zipPath, ZippiCallback callback) override;

		virtual void SplitImageIntoChannels(std::string baseImagePath, SplitCallback callback) override;

		virtual void Update(const ThresholdSettings & ts) override { tsApplied_ = ts; }

		virtual const ThresholdSettings& Threshold() const override { return tsApplied_; }

	private:

		bool fileNameFormatCheck(std::filesystem::path filePath, int& layerIndex);

		void showSplitImages(std::string image);

		cv::Mat readImage(std::string imageFileName, int flags = 1);

		cv::Mat mergeImages(InputImages metaData);

		std::wstring copyMergedImage(cv::Mat mergedImage, int index, std::wstring toCopyPath);

		void FileWatcherEventActionCallback(const std::filesystem::path& path, const filewatch::Event change_type, const LayersFileMap layerMap);

		bool ProcessSplit(int cIndex, cv::Mat& channel, SplitCallback callback);

	private:

		ThresholdSettings tsApplied_;

		std::map<int, InputImages> mapInputImages_;

		std::future<void> mergeImageFut_;

		std::unique_ptr<filewatch::FileWatch<std::filesystem::path>> fileWatcher_;

		bool bStartFileWatcher_ = false;

		std::wstring fileWatcherPath_;

		CallbackFiler filewatcherCallback_;
	};
}
