#pragma once

#include "RotaryAxes.h"
#include "SingleLayer.h"

#include <vector>
#include <unordered_map>
#include <mutex>

namespace ViewExplorer
{
	template<typename T> using cvector = std::vector<T>;
	template<typename T1, typename T2> using cmap = std::map<T1, T2>;
	template<typename T> using SPtr = std::shared_ptr<T>;

	struct LayerViewerRange
	{
		int fromLayer_;
		int toLayer_;
	};

	enum UIStates
	{
		eLayerBeforeLoad,
		eLayersLoading,
		eLayersLoaded,
		eFilesNotFound
	};

	class RayLibEngine
	{
	public:

		void RenderLoop();

		void InitMultiLayerModel(const char* imageFilePath, const char* folderWatchPath, double zHeightScale);

		RayLibEngine();

		~RayLibEngine();

		bool WindowClose();

	private:
		// Initialization        
		const static int screenWidth = 1000;
		const static int screenHeight = 650;

		Texture2D textureGrid_;
		Camera3D camera_;
		Model axismodel_;
		Vector3 position = { 0.0f, 0.0f, 0.0f };                    // Set model position

		ThresholdSettings ts_ = {};

		double zHeightScale_ = 1;
		bool showBackText_ = false;
		char* zipLayersPath_ = nullptr;
		char* folderPath_ = nullptr;

		RotaryAxes axes_;
		std::mutex fileWatcherMutex_;

		std::mutex layersMut_;
		cmap<int, SPtr<SingleLayer>> layers_;

		UIStates states_ = UIStates::eLayerBeforeLoad;

		std::mutex layersCacheMut_;
		LayerMap layerFiles_;

		LayerViewerRange layerViewRange_;

		void HandleEvents();
		void TransformModel();
		void DrawTexureGrid();
		void UpdateCameraPos();
		void CreateModelInstances(int fromLayers, int toLayers);
		void InitBackground();

		void RenderImGUI();

		// Define the camera to look into our 3d world
		void SetupCamera();
		void PreRender();
		void Render();
		void PostRender();
		void Cleanup();

		void ShowOverlayInformation(bool* p_open);
		void Show3DViewExplorerControls(bool* p_open);
		void ShowRGBAControls(bool* p_open);

		void CheckForDroppedFile();

		void ShowThreshodingSettings(bool* p_open);

		void FolderWatcherCallback(const std::wstring& path, const int index, const FileWatcherEvents events, const LayersFileMap& layer) noexcept;

		void LoadFromZip();

		void CheckUIStates();

		Texture2D LoadChannelImage(const std::string& path);
	};
}
