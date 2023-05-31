// ILayerCompressor.hpp - this file takes a collection of channel images (as r, g, b and a) with a specific
// naming convention in a pre-defined folder path and then compresses them in to a single image that has
// all the 4 channels encoded into it.

#pragma once

#include <CommonStructures.h>

using namespace CommonStructures;

namespace LayersMetaData
{
	class ILayersCompession
	{
	public:	

		virtual void Update(const ThresholdSettings& ts) = 0;

		virtual const ThresholdSettings & Threshold() const = 0;

		virtual ~ILayersCompession() = default;

		virtual void CleanUp() = 0;

		virtual void PreprocessLayerMetaData(std::wstring inputFolderPath, std::wstring zipPath) = 0;

		virtual void RegisterForFileUpdates(CallbackFiler callback) = 0;

		virtual void StartFileWatcherService(std::wstring path) = 0;

		virtual void StopFileWatcherService() = 0;

		virtual LayerMap GetLayersFromZip(std::string zipPath, ZippiCallback callback) = 0;

		virtual void SplitImageIntoChannels(std::string baseImagePath, SplitCallback callback) = 0;
	};
}


extern "C"
{
	LayersMetaData::ILayersCompession& getLayerImageInstance();
}

