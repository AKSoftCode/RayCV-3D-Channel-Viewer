#pragma once
#include <string>
#include <array>
#include <vector>
#include <map>
#include <functional>

namespace CommonStructures {
	enum class FileWatcherEvents {
		added,
		removed,
		modified,
		renamed_old,
		renamed_new,
		initial
	};

	enum ThresholdingEnum {
		eRThreshChanged = 1 << 0,
		eGThreshChanged = 1 << 1,
		eBThreshChanged = 1 << 2,
		eAThreshChanged = 1 << 3,
	};

	struct ThresholdSettings
	{
		std::vector<float> ThresholdBegin_ = { 0.5f, 0.5f, 0.5f ,0.5f };
		std::vector<float> ThresholdEnd_ = { 1.0f, 1.0f, 1.0f, 1.0f };

		std::array<bool, 4> flipThreshold_ = { true };

		int threshFlag_;

		bool operator==(const ThresholdSettings& srcSettings)
		{
			return srcSettings.ThresholdBegin_ == ThresholdBegin_ &&
				srcSettings.ThresholdEnd_ == ThresholdEnd_ &&
				srcSettings.flipThreshold_ == flipThreshold_ && srcSettings.threshFlag_ == threshFlag_;;
		}

		bool operator!=(const ThresholdSettings& srcSettings)
		{
			return !(*this == srcSettings);
		}
	};

	struct RenderSettings
	{
		bool bRChSelected;
		bool bGChSelected;
		bool bBChSelected;
		bool bAChSelected;
		bool bArrangeSeparate;
		double zHeightScale;
	};

	using LayersFileMap = std::map<int, std::string>;
	using SplitCallback = std::function<void(int, int, void*)>;
	using ZippiCallback = std::function<void(const bool)>;
	using CallbackFiler = std::function<void(const std::wstring&, const int index, const FileWatcherEvents, const LayersFileMap&)>;
	using LayerMap = std::map<int, std::string>;

	using RawData = void*;
}