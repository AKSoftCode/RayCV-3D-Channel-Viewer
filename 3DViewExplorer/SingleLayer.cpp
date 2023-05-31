#include "SingleLayer.h"

#include "ILayersCompession.h"
#include <iostream>

using namespace ViewExplorer;
using namespace LayersMetaData;


void SingleLayer::LoadImageCallback(int index, int size, RawData data) noexcept
{
	Image chImage = LoadImageFromMemory(".qoi", (unsigned char*)data, size);
	
	if (data)
	{
		free(data);
		data = NULL;
	}

	if (chImage.data == NULL)
		return;

	switch (index)
	{
	case 0:
		rChImage_ = chImage;

		break;
	case 1:
		gChImage_ = chImage;

		break;
	case 2:
		bChImage_ = chImage;

		break;
	case 3:
		aChImage_ = chImage;

		break;
	default:
		break;
	}
}

void SingleLayer::BuildLayer(bool bIsAsync)
{
	auto& lc = getLayerImageInstance();

	if (bIsAsync)
	{
		getImageData = std::async(std::launch::async, 
			[&]() { 
				lc.SplitImageIntoChannels(fileName_,
				[&](int index, int size, RawData data)
				{
					LoadImageCallback(index, size, data);
				});
			});
	}
	else
	{
		lc.SplitImageIntoChannels(fileName_,
			[&](int index, int size, RawData data)
			{
				LoadImageCallback(index, size, data);
			});
	}
}

void SingleLayer::UnloadChannel(Model model) noexcept
{
	if (model.materialCount > 0)
	{
		UnloadTexture(model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture);
	}	
}

void SingleLayer::Cleanup(int cleanupChannel)
{
	if (cleanupChannel == -1)
	{
		UnloadChannel(rChModel_);
		UnloadChannel(gChModel_);
		UnloadChannel(bChModel_);
		UnloadChannel(aChModel_);

		bFirstLoad_ = true;
	}
	else
	{
		if (cleanupChannel & ThresholdingEnum::eRThreshChanged)
		{
			UnloadChannel(rChModel_);

			bLoadRChOnly_ = true;
		}
		if (cleanupChannel & ThresholdingEnum::eGThreshChanged)
		{
			UnloadChannel(gChModel_);

			bLoadGChOnly_ = true;
		}
		if (cleanupChannel & ThresholdingEnum::eBThreshChanged)
		{
			UnloadChannel(bChModel_);

			bLoadBChOnly_ = true;
		}
		if (cleanupChannel & ThresholdingEnum::eAThreshChanged)
		{
			UnloadChannel(aChModel_);

			bLoadAChOnly_ = true;
		}
	}
}

SingleLayer::~SingleLayer()
{
	Cleanup();
}

void SingleLayer::LayerFirstLoad() noexcept
{
	if (!getImageData._Is_ready()) return;
	LoadChannel(rChImage_, rChModel_);
	LoadChannel(gChImage_, gChModel_);
	LoadChannel(bChImage_, bChModel_);
	LoadChannel(aChImage_, aChModel_);
	bFirstLoad_ = false;
}

void SingleLayer::LoadChannel(Image & channel, Model & model) noexcept
{
	try
	{
		static Mesh layerPlane = GenMeshPlane(15, 15, 5, 5);
		if (channel.data != NULL)
		{
			auto rchTexture = LoadTextureFromImage(channel);
			model = LoadModelFromMesh(layerPlane);
			model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rchTexture;
		}
	}
	catch (...)
	{
		std::cout << "Error in Loading Channel!!!" << std::endl;
	}
}

void SingleLayer::LoadEachChannel() noexcept
{
	if (!getImageData._Is_ready()) return;

	if (bLoadRChOnly_)
	{
		LoadChannel(rChImage_, rChModel_);
		bLoadRChOnly_ = false;
	}

	if (bLoadGChOnly_)
	{
		LoadChannel(gChImage_, gChModel_);
		bLoadGChOnly_ = false;
	}

	if (bLoadBChOnly_)
	{
		LoadChannel(bChImage_, bChModel_);
		bLoadBChOnly_ = false;
	}

	if (bLoadAChOnly_)
	{
		LoadChannel(aChImage_, aChModel_);
		bLoadAChOnly_ = false;
	}
}

void SingleLayer::UnloadImages()
{
	if (!getImageData._Is_ready()) return;

	if (rChImage_.data)
	{
		UnloadImage(rChImage_);
		rChImage_.data = NULL;
	}
	if (gChImage_.data)
	{
		UnloadImage(gChImage_);
		gChImage_.data = NULL;
	}
	if (bChImage_.data)
	{
		UnloadImage(bChImage_);
		bChImage_.data = NULL;
	}
	if (aChImage_.data)
	{
		UnloadImage(aChImage_);
		aChImage_.data = NULL;
	}
}

void SingleLayer::RenderAt(const RenderSettings & rs)
{
	if (!getImageData._Is_ready()) return;

	if (bFirstLoad_) LayerFirstLoad();

	LoadEachChannel();	

	UnloadImages();

	auto drawModel = [](const Model & model, Vector3 position, float zHeight)
	{
		if (model.materialCount > 0)
			DrawModel(model, position, 1.0f, RAYWHITE);
	};

	float zHeight = layerIndex_ * rs.zHeightScale;

	if (rs.bArrangeSeparate)
	{
		float xyShift = 10.0f;
		for (int z = 0; z < 4; ++z)
		{
			if(rs.bRChSelected) drawModel(rChModel_, { -xyShift, zHeight, -xyShift }, zHeight);
			if(rs.bGChSelected) drawModel(gChModel_, { xyShift, zHeight, xyShift }, zHeight);
			if(rs.bBChSelected) drawModel(bChModel_, { xyShift, zHeight, -xyShift }, zHeight);
			if(rs.bAChSelected) drawModel(aChModel_, { -xyShift, zHeight, xyShift }, zHeight);
		}
	}
	else
	{
		float zShift = 0.1 * rs.zHeightScale;
		if(rs.bGChSelected) drawModel(gChModel_, { 0.0f, zHeight - 2 * zShift, 0.0f }, zHeight); // Transparent first
		if(rs.bRChSelected) drawModel(rChModel_, { 0.0f, zHeight - zShift, 0.0f }, zHeight);
		if(rs.bBChSelected) drawModel(bChModel_, { 0.0f, zHeight + zShift, 0.0f }, zHeight);
		if(rs.bAChSelected) drawModel(aChModel_, { 0.0f, zHeight + 2 * zShift, 0.0f }, zHeight);
	}
}

SingleLayer::SingleLayer(const std::string filePath, int layerIndex)
{
	layerIndex_ = layerIndex;
	fileName_ = filePath;

	rChImage_.data = NULL;
	gChImage_.data = NULL;
	bChImage_.data = NULL;
	aChImage_.data = NULL;
}

void SingleLayer::TransformModel(Vector3 ang)
{
	if (!getImageData._Is_ready()) return;

	if (rChModel_.materialCount > 0)
		rChModel_.transform = MatrixRotateXYZ(ang);

	if (rChModel_.materialCount > 0)
		gChModel_.transform = MatrixRotateXYZ(ang);

	if (rChModel_.materialCount > 0)
		bChModel_.transform = MatrixRotateXYZ(ang);

	if (rChModel_.materialCount > 0)
		aChModel_.transform = MatrixRotateXYZ(ang);
}