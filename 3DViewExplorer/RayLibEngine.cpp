#include "RayLibEngine.h"

#include <filesystem>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "3DViewStyle.h"
#include "IconsMaterialDesign.h"
#include "Utilities.h"

#include <iostream>
#include <future>
#include <imgui_internal.h>
#include <algorithm>
#include <execution>
#include <ILayersCompession.h>

#include <atomic>
#include <vector>
#include <algorithm>
#include <execution>

using namespace ViewExplorer;
using namespace CommonStructures;

#define IMGUI_RIGHT_LABEL(func, label, ...) (func("##" label, __VA_ARGS__),  ImGui::SameLine(), ImGui::SetCursorPosY(ImGui::GetCursorPosY()), ImGui::TextUnformatted(label))

namespace
{
	enum Buttons
	{
		eTopThreshold,
		eBottomThreshold,
		eRChVisible,
		eGChVisible,
		eBChVisible,
		eAChVisible,
	};

	// Const text
	const char* menuWindowHeader = "Layer Details";
	const char* layersTextBox = "Total Layers";
	const char* clonesTextBox = "Clones";
	const char* layersStartupText = "";

	bool settingsPageActive = false;

	auto preSelectedIndex = -1;

	bool topThreadshold = false;
	bool bottomThreadshold = true;

	bool bShowOnlyLayer = false;

	bool bRChSelected = true;
	bool bGChSelected = true;
	bool bBChSelected = true;
	bool bAChSelected = true;


	bool bRedChannelFlipOn = false;
	bool bGreenChannelFlipOn = false;
	bool bBlueChannelFlipOn = false;
	bool bAlphaChannelFlipOn = false;
	bool bArrangeSeparately = true;

	std::future<void> loadZipFut;

	// Define controls variables
	bool bLoadAgain = false;
	bool folderWatchSource = true;

	std::wstring fileWatcherPath_;

	static int noOfLayers = -1;
	static char NoOfLayers[128] = "-1";

	std::atomic_bool bIsFileWatcherEventActivated = false;
	// Define controls variables
	int currentLayerIndex = -1;

	void SetNoOfLayers(int layerNo)
	{
		noOfLayers = layerNo;
		sprintf(NoOfLayers, "%d", noOfLayers);
	}
}

void RayLibEngine::InitBackground()
{
	UnloadTexture(textureGrid_);

	auto width = GetScreenWidth();
	auto height = GetScreenWidth();

	auto widthPercent = (int)(width / (width * 0.1) + 0.5);
	auto heightPercent = (int)(height / (height * 0.1) + 0.5);
	auto col1 = CLITERAL(Color) { 0, 0, 0, 255 };
	auto col2 = CLITERAL(Color) { 138, 200, 150, 255 };
	auto backImg = GenImageGradientV(width, height, col1, col2);
	textureGrid_ = LoadTextureFromImage(backImg);

	SetTextureFilter(textureGrid_, TEXTURE_FILTER_ANISOTROPIC_16X);
	SetTextureWrap(textureGrid_, TEXTURE_WRAP_CLAMP);

	UnloadImage(backImg);
}

void RayLibEngine::CheckUIStates()
{
	switch (states_)
	{
		case UIStates::eFilesNotFound:
		{
			layersStartupText = "Files Not Found!!";
			bLoadAgain = false;
			SetNoOfLayers(-1);
			currentLayerIndex = -1;
		}
		break;
		case UIStates::eLayerBeforeLoad:
		{
			layersStartupText = "Nothing to load !!";
			bLoadAgain = false;
			SetNoOfLayers(-1);
			currentLayerIndex = -1;
		}
		break;
		case UIStates::eLayersLoaded:
		{
			layersStartupText = "Loaded";
			bLoadAgain = false;
		}
		break;
		case UIStates::eLayersLoading:
		{
			layersStartupText = "Layers Loading in Progress....";
			bLoadAgain = true;
		}
		break;
	}
}

Texture2D RayLibEngine::LoadChannelImage(const std::string& path)
{
	Texture2D chTexture = { 0 };

	Image image = LoadImage(path.c_str());
	ImageColorTint(&image, {64,100,128});
	if (image.data != NULL)
	{
		chTexture = LoadTextureFromImage(image);
		UnloadImage(image);
	}
	chTexture.height = 23;
	chTexture.width = 23;
	return std::move(chTexture);
}

void RayLibEngine::InitMultiLayerModel(const char* imageZipFilePath, const  char* folderWatchPath, double zHeightScale)
{
	zipLayersPath_ = (char*)imageZipFilePath;
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(screenWidth, screenHeight, "3DView");

	Image icon = LoadImage("Resources//images//dice.qoi");
	SetWindowIcon(icon);
	UnloadImage(icon);

	GuiLoadStyle();

	SetTargetFPS(144);

	rlImGuiSetup(true);
	SetupCamera();

	InitBackground();

	zHeightScale_ = zHeightScale;

	fileWatcherPath_ = Utilities::stingtoWString(folderWatchPath);

	getLayerImageInstance().RegisterForFileUpdates([&](const std::wstring& path, const int index, const FileWatcherEvents events, const LayersFileMap& layer) {
		FolderWatcherCallback(path, index, events, layer);
		});

	if (folderWatchSource)
	{
		states_ = UIStates::eFilesNotFound;

		getLayerImageInstance().StartFileWatcherService(fileWatcherPath_);
	}
	else
	{
		states_ = UIStates::eFilesNotFound;

		getLayerImageInstance().StopFileWatcherService();

		LoadFromZip();
	}
}

void RayLibEngine::LoadFromZip()
{
	if (std::filesystem::exists(zipLayersPath_))
	{
		states_ = UIStates::eLayerBeforeLoad;
		loadZipFut = std::async(std::launch::async, [&]() {
			auto files = getLayerImageInstance().GetLayersFromZip(zipLayersPath_, [&](bool bStatus) {
				bLoadAgain = bStatus;
				});

			states_ = UIStates::eLayersLoading;

			{
				std::lock_guard<std::mutex> lk(layersCacheMut_);
				layerFiles_ = files;
			}

			});
	}
	else
	{
		states_ = UIStates::eFilesNotFound;
	}
}

void RayLibEngine::SetupCamera()
{
	camera_ = Camera{ 0 };
	camera_.position = { 100.0f, 50.0f, 50.0f };// Camera position
	camera_.target = { 0.0f, 0.0f, 0.0f };      // Camera looking at point
	camera_.up = { 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
	camera_.fovy = 45.0f;                       // Camera field-of-view Y
	camera_.projection = CAMERA_PERSPECTIVE;    // Camera mode type

	SetCameraMode(camera_, CAMERA_FREE); // Set free camera mode

}

RayLibEngine::RayLibEngine()
{
	auto* imgui_context = ImGui::CreateContext();
	ImGui::SetCurrentContext(imgui_context);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontDefault();

	auto & ls = getLayerImageInstance();
	ls.Update(ts_);

	// merge in icons from Font Awesome
	static const ImWchar icons_ranges[] = { ICON_MIN_MD, ICON_MAX_16_MD, 0 };
	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_MD, 18.0f, &icons_config, icons_ranges);
	// use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid
}

RayLibEngine::~RayLibEngine()
{
	Cleanup();

	UnloadTexture(textureGrid_);
	
	CloseWindow();        // Close window and OpenGL context
}

void RayLibEngine::Cleanup()
{
	{
		std::lock_guard<std::mutex> lk(layersCacheMut_);
		layerFiles_.clear();
	}

	{
		std::lock_guard<std::mutex> lk(layersMut_);
		layers_.clear();
	}
	
	bAChSelected = true;
	bBChSelected = true;
	bGChSelected = true;
	bRChSelected = true;

	topThreadshold = false;
	bottomThreadshold = false;
	bLoadAgain = false;
	bShowOnlyLayer = false;

	states_ = UIStates::eLayerBeforeLoad;

	currentLayerIndex = -1;
	SetNoOfLayers(-1);

	layersStartupText = "Nothing to load !!";
}

void RayLibEngine::CheckForDroppedFile()
{
	if (IsFileDropped())
	{
		FilePathList droppedFiles = LoadDroppedFiles();

		if (droppedFiles.count == 1)
		{
			if (folderWatchSource)
				return;

			Cleanup();

			auto fileExtension = std::filesystem::path(droppedFiles.paths[0]).extension();
			if (fileExtension == ".zip" && !folderWatchSource)
			{
				states_ = UIStates::eLayerBeforeLoad;
				std::memcpy(zipLayersPath_, droppedFiles.paths[0], strlen(droppedFiles.paths[0]) + 1);
				LoadFromZip();
			}

			UnloadDroppedFiles(droppedFiles);
		}
	}
}

void RayLibEngine::RenderLoop()
{
	CheckUIStates();

	CheckForDroppedFile();

	PreRender();

	Render();

	PostRender();
}

void RayLibEngine::PreRender()
{
	HandleEvents();
	UpdateCameraPos();
	TransformModel();

	//----------------------------------------------------------------------------------

	BeginDrawing();

	ClearBackground(RAYWHITE);
	DrawTexureGrid();

	BeginMode3D(camera_);

}

void RayLibEngine::Render()
{
	if (states_ == UIStates::eLayerBeforeLoad || states_ == UIStates::eFilesNotFound)
	{
		GuiProgressBar({ (float)(GetScreenWidth() / 2 - 15), (float)(GetScreenHeight() / 2 - 15), 150, 30 }, "", "", 100, 0, 100);
	}
	else
	{
		if (bLoadAgain)
		{	
			{
				std::lock_guard<std::mutex> lk(layersCacheMut_);
				if (!layerFiles_.empty())
					SetNoOfLayers((--layerFiles_.end())->first);
			}
				
			bLoadAgain = false;
		}

		const RenderSettings rs{ bRChSelected, bGChSelected, bBChSelected, bAChSelected, bArrangeSeparately, zHeightScale_ };

		if (topThreadshold && bottomThreadshold)
		{
			layerViewRange_ = { 0, noOfLayers };	
		}
		else if (topThreadshold)
		{
			layerViewRange_ = { currentLayerIndex, noOfLayers };
		}
		else if (bottomThreadshold)
		{
			layerViewRange_ = { 0, currentLayerIndex };
		}
		else if (bShowOnlyLayer)
		{
			layerViewRange_ = { currentLayerIndex, currentLayerIndex };
		}
		else
		{
			layerViewRange_ = { 0, noOfLayers };
		}


		CreateModelInstances(layerViewRange_.fromLayer_, layerViewRange_.toLayer_);
		currentLayerIndex = layerViewRange_.toLayer_;

		cmap<int, SPtr<SingleLayer>> tempMap;
		{
			std::lock_guard<std::mutex> lk(layersMut_);
			tempMap = layers_;
		}

		int layerCount = layerViewRange_.toLayer_;
		if (layerCount != 0)
		{
			for (int i = layerViewRange_.fromLayer_; i <= layerCount; ++i)
			{
				if (bIsFileWatcherEventActivated) break;
				if (tempMap.find(i) != tempMap.end())
				{
					const auto& layer = tempMap[i];
					if (layer) layer->RenderAt(rs);
				}
			}
		}
	}

	DrawGrid(20, 10.0f);
}

void RayLibEngine::PostRender()
{

	EndMode3D();

	RenderImGUI();

	if (showBackText_)
	{
		DrawText("3DViewExplorer", 10, 10, 25, YELLOW);
		DrawFPS(10, 40);
	}


	EndDrawing();
}

void RayLibEngine::CreateModelInstances(int fromLayers, int tolayers)
{
	{
		if (tolayers > noOfLayers)
			return;
    
		cmap<int, std::string> layersCacheTemp;
		{
			std::lock_guard<std::mutex> lk(layersCacheMut_);
			layersCacheTemp = layerFiles_;
		}

		for (int i = fromLayers; i <= tolayers; i++)
		{
			if (bIsFileWatcherEventActivated) break;

			if (layersCacheTemp.find(i) == layersCacheTemp.end())
				continue;

			const auto& layerPath = layersCacheTemp[i];

			if (layerPath == "")
				continue;

			std::unique_lock<std::mutex> lk(layersMut_);

			if (layers_.find(i) == layers_.end())
			{
				lk.unlock();
				
				states_ = UIStates::eLayersLoading;

				std::shared_ptr<SingleLayer> singleLayerModel = std::make_shared<SingleLayer>(layerPath.c_str(), i);

				singleLayerModel->BuildLayer();
				singleLayerModel->TransformModel(Vector3{ 0,0, 0.1f * i });

				layers_.emplace(i, singleLayerModel);
			}
		}

		states_ = UIStates::eLayersLoaded;
	}
}

bool RayLibEngine::WindowClose()
{
	return WindowShouldClose();
}

void RayLibEngine::UpdateCameraPos()
{
	UpdateCamera(&camera_);
}

void RayLibEngine::HandleEvents()
{
	axes_.HandleView();
	if (IsKeyPressed(KEY_F1))
	{
		showBackText_ = !showBackText_;
	}

	if (IsWindowResized())
	{
		InitBackground();
		DrawTexture(textureGrid_, 0, 0, RAYWHITE);
	}
}

void RayLibEngine::TransformModel()
{
	cmap<int, SPtr<SingleLayer>> tempMap;
	{
		std::lock_guard<std::mutex> lk(layersMut_);
		tempMap = layers_;
	}

	for (auto& [index, model] : tempMap)
	{
		if (model)
			model->TransformModel(axes_.Transformer());
	}
}

void RayLibEngine::DrawTexureGrid()
{
	DrawTexture(textureGrid_, 0, 0, RAYWHITE);
}

void RayLibEngine::ShowOverlayInformation(bool* p_open)
{
	static bool layerDetailsActive = false;

	ImGuiIO& io = ImGui::GetIO();
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

	static ImVec2 buttonsSize = { 24, 24 };
	ImVec2 window_pos;
	window_pos.x = 20;
	window_pos.y = 20;
	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

	window_flags |= ImGuiWindowFlags_NoMove;

	if (ImGui::Begin("Layer Information", p_open, window_flags))
	{
		ImGuiWindow* window = ImGui::FindWindowByName("Layer Information");
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 1.5f));

		static int folderZipSwitch = 1;
		auto cachedFolderWatchSource = folderZipSwitch;
		ImGui::RadioButton("Zip", &folderZipSwitch, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Folder", &folderZipSwitch, 1);
		ImGui::SameLine();
		ImGui::SetCursorPosX(window->ContentSize.x - 20);


		if (folderZipSwitch != cachedFolderWatchSource)
		{
			Cleanup();
			if (folderZipSwitch)
			{
				getLayerImageInstance().StartFileWatcherService(fileWatcherPath_);
			}
			else
			{
				getLayerImageInstance().StopFileWatcherService();

				LoadFromZip();
			}		
		}
		

		if (ImGui::Button(ICON_MD_REFRESH, buttonsSize))
		{
			{
				std::lock_guard<std::mutex> lk(layersCacheMut_);
				if (!layerFiles_.empty())
					SetNoOfLayers((--layerFiles_.end())->first);
			}
			
			currentLayerIndex = noOfLayers;
			layerViewRange_ = { 0, noOfLayers };
		}

		if (states_ == UIStates::eLayersLoading)
		{
			std::string progressText = std::string(layersStartupText).append(" % c   ");
			ImGui::Text(progressText.c_str(), "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
		}
		else if (states_ == UIStates::eFilesNotFound || states_ == UIStates::eLayerBeforeLoad)
		{
			ImGui::Text(layersStartupText);
		}
		else if(states_ == UIStates::eLayersLoaded)
		{
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
			ImGui::Text("Total Layers: %d", noOfLayers);

			ImGui::SameLine();
			ImGui::SetCursorPosX(window->ContentSize.x - 20);
			if (ImGui::Button(ICON_MD_SETTINGS, buttonsSize))
			{
				settingsPageActive = !settingsPageActive;
			}

			if (settingsPageActive)
			{
				ShowThreshodingSettings(&settingsPageActive);
			}

			ImGui::PushItemWidth(50);
			int inputVal = currentLayerIndex;
			ImGui::InputInt("Current Layer", &inputVal, 0, noOfLayers);

			if (currentLayerIndex != inputVal)
				currentLayerIndex = inputVal;

			ImGui::PopItemWidth();

			ImGui::PushItemWidth(50);
			ImGui::InputDouble("Height", &zHeightScale_, 0, 0, "% .3f", ImGuiInputTextFlags_None);
			ImGui::PopItemWidth();
		}

		ImGui::Separator();
		if (ImGui::IsMousePosValid())
		{
			ImGui::PushItemWidth(150);
			ImGui::Text("Mouse Position: (%05.1f,%05.1f)", io.MousePos.x, io.MousePos.y);
			ImGui::PopItemWidth();
		}
		else
			ImGui::Text("Mouse Position: <invalid>");

		ImGui::PopStyleVar(2);
	}
	ImGui::End();
}

void RayLibEngine::Show3DViewExplorerControls(bool* p_open)
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

	float windowHeight = GetScreenHeight() - 250;
	static float windowWidth = 60;
	float sliderHeight = windowHeight - 85;
	static float sliderWidth = 18;
	static ImVec2 buttonsSize = { 30, 30 };

	ImVec2 window_pos, window_pos_pivot;
	window_pos.x = GetScreenWidth() - 80;
	window_pos.y = 240;
	window_pos_pivot.x = 0.0f;
	window_pos_pivot.y = 0.0f;

	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
	window_flags |= ImGuiWindowFlags_NoMove;

	ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

	if (ImGui::Begin("Slider UI", p_open, window_flags))
	{
		ImGui::PushStyleColor(ImGuiCol_Border, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor::HSV(0.138f, 0.10f, 0.19f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, (ImVec4)ImColor::HSV(0.138f, 0.10f, 0.19f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, (ImVec4)ImColor::HSV(0.138f, 0.10f, 0.19f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, (ImVec4)ImColor::HSV(4 / 7.0f, 0.5f, 0.5f));

		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.75f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));

		ImGui::SetCursorPosX((windowWidth / 2) - (buttonsSize.x / 2));
		ImGui::PushID(Buttons::eTopThreshold);
		if (topThreadshold)
		{
			if (ImGui::Button(ICON_MD_VISIBILITY, buttonsSize))
			{
				topThreadshold = false;
				bShowOnlyLayer = true;
			}
		}
		else
		{
			if (ImGui::Button(ICON_MD_VISIBILITY_OFF, buttonsSize))
			{
				topThreadshold = true;
				bShowOnlyLayer = false;
			}
		}
		ImGui::PopID();

		ImGui::SetCursorPosX((windowWidth / 2) - (sliderWidth / 2));
		if (ImGui::VSliderInt("##v", ImVec2(sliderWidth, sliderHeight), &currentLayerIndex, 0, noOfLayers, "%d", ImGuiSliderFlags_None))
		{
			//layerViewRange_ = { 0, currentLayerIndex };
		}

		ImGui::SetCursorPosX((windowWidth / 2) - (buttonsSize.x / 2));

		ImGui::PushID(Buttons::eBottomThreshold);
		if (bottomThreadshold)
		{
			if (ImGui::Button(ICON_MD_VISIBILITY, buttonsSize))
			{
				bottomThreadshold = false;
				bShowOnlyLayer = true;
			}
		}
		else
		{
			if (ImGui::Button(ICON_MD_VISIBILITY_OFF, buttonsSize))
			{
				bottomThreadshold = true;
				bShowOnlyLayer = false;
			}
		}
		ImGui::PopID();

		ImGui::PopStyleColor(5);
		ImGui::PopStyleVar(2);
	}
	ImGui::End();
}

void RayLibEngine::ShowRGBAControls(bool* p_open)
{
	float windowHeight = 220;
	static float windowWidth = 60;

	static ImVec2 buttonsSize = { 30, 30 };

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

	ImVec2 window_pos, window_pos_pivot;
	window_pos.x = GetScreenWidth() - 80;
	window_pos.y = 10;
	window_pos_pivot.x = 0.0f;
	window_pos_pivot.y = 0.0f;
	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
	window_flags |= ImGuiWindowFlags_NoMove;

	ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
	if (ImGui::Begin("RGBA Channels", p_open, window_flags))
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor::HSV(0.138f, 0.10f, 0.19f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, (ImVec4)ImColor::HSV(0.138f, 0.10f, 0.19f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, (ImVec4)ImColor::HSV(0.138f, 0.10f, 0.19f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, (ImVec4)ImColor::HSV(4 / 7.0f, 0.5f, 0.5f));

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.75f, 1.0f));

		ImGui::SetCursorPosY(15);
		ImGui::SetCursorPosX((windowWidth / 2) - (buttonsSize.x / 2));

		if (bArrangeSeparately)
		{
			if (ImGui::Button(ICON_MD_GRID_VIEW, buttonsSize))
			{
				bArrangeSeparately = false;
			}
		}
		else
		{
			if (ImGui::Button(ICON_MD_SELECT_ALL, buttonsSize))
			{
				bArrangeSeparately = true;
			}
		}

		ImGui::PopStyleVar();

		ImGui::Separator();
		ImGui::SetCursorPosX((windowWidth / 2) - (buttonsSize.x / 2));
		ImGui::PushID(Buttons::eRChVisible);
		if (bRChSelected)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
	
			if (ImGui::Button("R", buttonsSize))
			{
				bRChSelected = false;
			}
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.75f, 1.0f));
			if (ImGui::Button(ICON_MD_NO_PHOTOGRAPHY, buttonsSize))
			{
				bRChSelected = true;
			}
		}
		ImGui::PopID();

		ImGui::SetCursorPosX((windowWidth / 2) - (buttonsSize.x / 2));
		ImGui::PushID(Buttons::eGChVisible);
		if (bGChSelected)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
			if (ImGui::Button("G", buttonsSize))
			{
				bGChSelected = false;
			}
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.75f, 1.0f));
			if (ImGui::Button(ICON_MD_NO_PHOTOGRAPHY, buttonsSize))
			{
				bGChSelected = true;
			}
		}
		ImGui::PopID();

		ImGui::SetCursorPosX((windowWidth / 2) - (buttonsSize.x / 2));
		ImGui::PushID(Buttons::eBChVisible);
		if (bBChSelected)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
			if (ImGui::Button("B", buttonsSize))
			{
				bBChSelected = false;
			}
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.75f, 1.0f));
			if (ImGui::Button(ICON_MD_NO_PHOTOGRAPHY, buttonsSize))
			{
				bBChSelected = true;
			}
		}
		ImGui::PopID();

		ImGui::SetCursorPosX((windowWidth / 2) - (buttonsSize.x / 2));
		ImGui::PushID(Buttons::eAChVisible);
		if (bAChSelected)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
			if (ImGui::Button("A", buttonsSize))
			{
				bAChSelected = false;
			}
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.75f, 1.0f));
			if (ImGui::Button(ICON_MD_NO_PHOTOGRAPHY, buttonsSize))
			{
				bAChSelected = true;
			}
		}
		ImGui::PopID();

		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(5);
	}
	ImGui::End();
}

void RayLibEngine::ShowThreshodingSettings(bool* p_open)
{
	ImGui::SetNextWindowSize(ImVec2(375, 175), ImGuiCond_FirstUseEver);

	static ImVec2 buttonsSize = { 50, 30 };
	static ImVec2 theshButtonsSize = { 20, 20 };
	static ImVec2 flipButtonsSize = { 25, 20 };

	if (ImGui::Begin("Thresholding", p_open, ImGuiWindowFlags_NoResize))
	{
		ImGuiWindow* window = ImGui::FindWindowByName("Thresholding");

		int rangeWidth = 250;
		//----------------------------------- Red Channel----------------------------------------

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::ImColor(255, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::ImColor(255, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::ImColor(255, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::ImColor(255, 0, 0));
		ImGui::Button("", theshButtonsSize);
		ImGui::PopItemFlag();

		ImGui::SameLine();

		ImGui::PushItemWidth(rangeWidth);
		if (ImGui::DragFloatRange2("R", &ts_.ThresholdBegin_[0], &ts_.ThresholdEnd_[0], 0.001f, 0.0f, 1.0f, "Min: %.3f", "Max: %.3f", ImGuiSliderFlags_AlwaysClamp))
		{
			ts_.threshFlag_ = ts_.threshFlag_ | ThresholdingEnum::eRThreshChanged;
		}
		ImGui::PopItemWidth();

		ImGui::PopStyleColor(4);

		ImGui::SameLine();

		ImGui::PushID(101);
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(10.0f, 6.0f));

		if (bRedChannelFlipOn)
		{
			if (ImGui::Button(ICON_MD_FLIP_TO_FRONT, flipButtonsSize))
			{
				bRedChannelFlipOn = false;
				ts_.threshFlag_ = ts_.threshFlag_ | ThresholdingEnum::eRThreshChanged;
			}
		}
		else
		{
			if (ImGui::Button(ICON_MD_FLIP_TO_BACK, flipButtonsSize))
			{
				bRedChannelFlipOn = true;
				ts_.threshFlag_ = ts_.threshFlag_ | ThresholdingEnum::eRThreshChanged;
			}
		}
		ImGui::PopID();
		ImGui::PopStyleVar();

		//----------------------------------- Blue Channel----------------------------------------

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::ImColor(0, 0, 255));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::ImColor(0, 0, 255));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::ImColor(0, 0, 255));
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::ImColor(0, 0, 255));
		ImGui::Button("", theshButtonsSize);
		ImGui::PopItemFlag();

		ImGui::SameLine();
		ImGui::PushItemWidth(rangeWidth);
		if (ImGui::DragFloatRange2("G", &ts_.ThresholdBegin_[2], &ts_.ThresholdEnd_[2], 0.001f, 0.0f, 1.0f, "Min: %.3f", "Max: %.3f", ImGuiSliderFlags_AlwaysClamp))
		{
			ts_.threshFlag_ = ts_.threshFlag_ | ThresholdingEnum::eBThreshChanged;
		}
		ImGui::PopItemWidth();
		ImGui::PopStyleColor(4);

		ImGui::SameLine();
		ImGui::PushID(103);
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(10.0f, 6.0f));

		if (bBlueChannelFlipOn)
		{
			if (ImGui::Button(ICON_MD_FLIP_TO_FRONT, flipButtonsSize))
			{
				bBlueChannelFlipOn = false;
				ts_.threshFlag_ = ts_.threshFlag_ | ThresholdingEnum::eBThreshChanged;
			}
		}
		else
		{
			if (ImGui::Button(ICON_MD_FLIP_TO_BACK, flipButtonsSize))
			{
				bBlueChannelFlipOn = true;
				ts_.threshFlag_ = ts_.threshFlag_ | ThresholdingEnum::eBThreshChanged;
			}
		}
		ImGui::PopID();
		ImGui::PopStyleVar();

		//----------------------------------- Alpha Channel----------------------------------------

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::ImColor(255, 255, 255));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::ImColor(255, 255, 255));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::ImColor(255, 255, 255));
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::ImColor(255, 255, 255));
		ImGui::Button("", theshButtonsSize);

		ImGui::PopItemFlag();

		ImGui::SameLine();
		ImGui::PushItemWidth(rangeWidth);
		if (ImGui::DragFloatRange2("B", &ts_.ThresholdBegin_[3], &ts_.ThresholdEnd_[3], 0.001f, 0.0f, 1.0f, "Min: %.3f", "Max: %.3f", ImGuiSliderFlags_AlwaysClamp))
		{
			ts_.threshFlag_ = ts_.threshFlag_ | ThresholdingEnum::eAThreshChanged;
		}
		ImGui::PopItemWidth();
		ImGui::PopStyleColor(4);

		ImGui::SameLine();
		ImGui::PushID(104);
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(10.0f, 6.0f));

		if (bAlphaChannelFlipOn)
		{
			if (ImGui::Button(ICON_MD_FLIP_TO_FRONT, flipButtonsSize))
			{
				bAlphaChannelFlipOn = false;
				ts_.threshFlag_ = ts_.threshFlag_ | ThresholdingEnum::eAThreshChanged;
			}
		}
		else
		{
			if (ImGui::Button(ICON_MD_FLIP_TO_BACK, flipButtonsSize))
			{
				bAlphaChannelFlipOn = true;
				ts_.threshFlag_ = ts_.threshFlag_ | ThresholdingEnum::eAThreshChanged;
			}
		}

		ImGui::PopID();
		ImGui::PopStyleVar();

		//-----------------------------------------------------------------------------------------

		ts_.flipThreshold_[0] = bRedChannelFlipOn;
		ts_.flipThreshold_[1] = bGreenChannelFlipOn;
		ts_.flipThreshold_[2] = bBlueChannelFlipOn;
		ts_.flipThreshold_[3] = bAlphaChannelFlipOn;

		//----------------------------------- Apply Button ----------------------------------------
		ImGui::SetCursorPosX(window->ContentSize.x - 42);

		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.7f));
		if (ImGui::Button("APPLY", buttonsSize))
		{
			auto& ls = getLayerImageInstance();
			const auto& tsApplied = ls.Threshold();
			if (ts_ != tsApplied)
			{
				ls.Update(ts_);
				int i = 0;

				cmap<int, SPtr<SingleLayer>> tempMap;
				{	
					std::lock_guard<std::mutex> lk(layersMut_);
					tempMap = layers_;
				}
				
				for (auto& [index, layer] : tempMap)
				{
					if (!layer) continue;

					layer->Cleanup(ts_.threshFlag_);
					layer->BuildLayer();
					layer->TransformModel(Vector3{ 0,0, 0.1f * i++ });
				};
				ts_.threshFlag_ = 0;
			}
			else
			{
				std::cout << "No Changes detected!! Nothing to load in Thresholding" << std::endl;
			}		
		}
		
		ImGui::PopStyleVar();
	}
	ImGui::End();
}

void RayLibEngine::FolderWatcherCallback(const std::wstring& path, const int index, const FileWatcherEvents events, const LayersFileMap& layer) noexcept
{
	if (!folderWatchSource)
		return;

	std::lock_guard<std::mutex> lk(fileWatcherMutex_);
	bIsFileWatcherEventActivated = true;
	switch (events)
	{
		case FileWatcherEvents::initial:
		{ 
			if (layer.size() == 0)
				states_ = UIStates::eFilesNotFound;

			cmap<int, std::string> layersCacheTemp;
			{
				std::lock_guard<std::mutex> lk(layersCacheMut_);
				layerFiles_ = layer;
			}

			states_ = UIStates::eLayersLoading;
			break;
		}
		case FileWatcherEvents::added:
		{		
			states_ = UIStates::eLayersLoading;
			std::string value = std::string(path.begin(), path.end());

			{
				std::lock_guard<std::mutex> lk(layersCacheMut_);
				layerFiles_.emplace(index, value);
			}
			
			break;
		}
		case FileWatcherEvents::removed:
		{
			{
				std::lock_guard<std::mutex> lk(layersCacheMut_);
			
				layerFiles_.erase(index);
			}

			std::lock_guard<std::mutex> lk(layersMut_);
			layers_.erase(index);
			break;
		}
	}

	bIsFileWatcherEventActivated = false;
	topThreadshold = false;
	bottomThreadshold = true;
	{
		std::lock_guard<std::mutex> lk(layersCacheMut_);
		if (!layerFiles_.empty()) {
			int noOfLayer = (--layerFiles_.end())->first;
			SetNoOfLayers(noOfLayer);
			currentLayerIndex = noOfLayer;
		}	
		else
		{
			states_ = UIStates::eFilesNotFound;
		}
			

		layerViewRange_ = { 0, noOfLayers };
	}
}

void RayLibEngine::RenderImGUI()
{
	// start the GUI
	rlImGuiBegin();

	if (states_ == UIStates::eLayersLoading || states_ == UIStates::eLayersLoaded)
	{
		Show3DViewExplorerControls(NULL);

		ShowRGBAControls(NULL);
	}

	ShowOverlayInformation(NULL);

	// end ImGui
	rlImGuiEnd();
	//----------------------------------------------------------------------------------
}
