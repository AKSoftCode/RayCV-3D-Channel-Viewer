#pragma once

#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include "rlImGui.h"
#include "CommonStructures.h"

#include <concurrent_vector.h>
#include <future>
using namespace CommonStructures;

namespace ViewExplorer
{
	class SingleLayer
	{
	public:

		virtual ~SingleLayer();
		SingleLayer(const std::string filePath, int layerIndex);

		void Cleanup(int cleanupChannel = -1);

		void BuildLayer(bool bIsAsync = true);

		void TransformModel(Vector3 ang);

		void RenderAt(const RenderSettings& rs);

	private:
		Image rChImage_ = { 0 };
		Image gChImage_ = { 0 };
		Image bChImage_ = { 0 };
		Image aChImage_ = { 0 };

		Model rChModel_ = { 0 };
		Model gChModel_ = { 0 };
		Model bChModel_ = { 0 };
		Model aChModel_ = { 0 };

		int layerIndex_ = -1;

		bool bFirstLoad_ = true;

		bool bLoadRChOnly_ = false;
		bool bLoadGChOnly_ = false;
		bool bLoadBChOnly_ = false;
		bool bLoadAChOnly_ = false;

		std::future<void> getImageData;
		std::string fileName_;

		void LoadImageCallback(int index, int size, RawData data) noexcept;
		void LayerFirstLoad() noexcept;
		void LoadChannel(Image& channel, Model& model) noexcept;
		void LoadEachChannel() noexcept;
		void UnloadChannel(Model model) noexcept;
		void UnloadImages();
	};
}


