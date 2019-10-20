#include "stdafx.h"
//#include "block.h"
#include "camera.h"
#include "generation.h"
#include "level.h"
#include "pipeline.h"
#include "mesh.h"
#include "texture.h"
#include "frustum.h"
#include "transform.h"
#include "mesh_comp.h"
#include "unlit_mesh.h"
#include "render_data.h"
#include "chunk.h"
#include "sun.h"
#include <chrono>
#include <execution>
#include "vendor/ctpl_stl.h"
#include "input.h"
#include "pick.h"
#include "settings.h"
#include <functional>
#include "editor.h"

#include "collision_shapes.h"

using namespace std::chrono;

Level::Level(std::string name)
{
	name_ = name;
}

Level::~Level()
{
	for (auto& obj : objects_)
		delete obj;

	for (auto& cam : cameras_)
		delete cam;
}

// for now this function is where we declare objects
void Level::Init()
{
	cameras_.push_back(new Camera(kControlCam));
	//cameras_.push_back(new Camera(kPhysicsCam));
	Render::SetCamera(cameras_[0]);

	high_resolution_clock::time_point benchmark_clock_ = high_resolution_clock::now();
	
	WorldGen::InitNoiseFuncs();
	Editor::level = this;
	Editor::chunkManager = &chunkManager_;
	Editor::renderer = &renderer_;
	PrefabManager::InitPrefabs();
	BiomeManager::InitializeBiomes();
	chunkManager_.SetCurrentLevel(this);
	chunkManager_.SetLoadDistance(100.f);
	chunkManager_.SetUnloadLeniency(100.f);
	chunkManager_.SetMaxLoadPerFrame(1);
	renderer_.Init();
	renderer_.chunkManager_ = &chunkManager_;
	
	duration<double> benchmark_duration_ = duration_cast<duration<double>>(high_resolution_clock::now() - benchmark_clock_);
	std::cout << benchmark_duration_.count() << std::endl;
}

// update every object in the level
void Level::Update(float dt)
{
	if (Input::Keyboard().pressed[GLFW_KEY_GRAVE_ACCENT])
	{
		activeCursor = !activeCursor;
	}
	glfwSetInputMode(game_->GetWindow(), GLFW_CURSOR, activeCursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);

	// update each camera
	if (!activeCursor)
		for (auto& cam : cameras_)
			cam->Update(dt);

	chunkManager_.Update(this);
	CheckInteraction();
	sun_.Update();
	renderer_.SetDirLight(&sun_.GetDirLight());
	renderer_.SetSun(&sun_);

	renderer_.DrawAll();
	Editor::Update();
	hud_.Update();
	DrawImGui();

	CheckCollision();
}

void Level::DrawImGui()
{
	{
		ImGui::SetNextWindowPos(ImVec2(20, 20));
		ImGui::SetNextWindowSize(ImVec2(350, 600));
		ImGui::Begin("Sun");

		glm::vec3 pos = sun_.GetPos();
		if (ImGui::DragFloat3("Sun Pos", &pos[0], 1, -500, 500, "%.0f"))
			sun_.SetPos(pos);

		glm::vec3 dir = sun_.GetDir();
		if (ImGui::SliderFloat3("Sun Dir", &dir[0], -1, 1, "%.3f"))
			sun_.SetDir(dir);

		ImGui::Checkbox("Orbit Pos", &sun_.orbits);
		ImGui::SameLine();
		ImGui::DragFloat3("##Orbitee", &sun_.orbitPos[0], 2.f, -500, 500, "%.0f");
		ImGui::Checkbox("Follow Cam", &sun_.followCam);
		ImGui::SliderFloat("Follow Distance", &sun_.followDist, 0, 500, "%.0f");

		//int shadow = sun_.GetShadowSize().x;
		//if (ImGui::InputInt("Shadow Scale", &shadow, 1024, 1024))
		//{
		//	glm::clamp(shadow, 0, 16384);
		//	sun_.SetShadowSize(glm::ivec2(shadow));
		//}
		if (ImGui::Button("Recompile Water Shader"))
		{
			delete Shader::shaders["chunk_water"];
			Shader::shaders["chunk_water"] = new Shader("chunk_water.vs", "chunk_water.fs");
		}
		if (ImGui::Button("Recompile Debug Map"))
		{
			delete Shader::shaders["debug_map3"];
			Shader::shaders["debug_map3"] = new Shader("debug_map.vs", "debug_map.fs");
		}
		if (ImGui::Button("Recompile Postprocess Shader"))
		{
			//delete Shader::shaders["postprocess"];
			Shader::shaders["postprocess"] = new Shader("postprocess.vs", "postprocess.fs");
		}

		ImGui::End();
	}

	{
		ImGui::SetNextWindowPos(ImVec2(1500, 20));
		ImGui::SetNextWindowSize(ImVec2(300, 600));
		ImGui::Begin("Info");

		ImGui::Text("FPS: %.0f (%.1f ms)", 1.f / game_->GetDT(), 1000 * game_->GetDT());
		ImGui::NewLine();
		glm::vec3 pos = Render::GetCamera()->GetPos();
		//ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
		if (ImGui::InputFloat3("Camera Position", &pos[0], 2))
			Render::GetCamera()->SetPos(pos);
		pos = Render::GetCamera()->front;
		ImGui::Text("Camera Direction: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
		pos = pos * .5f + .5f;
		ImGui::SameLine();
		ImGui::ColorButton("visualization", ImVec4(pos.x, pos.y, pos.z, 1.f));

		localpos local = Chunk::worldBlockToLocalPos(Render::GetCamera()->GetPos());
		ImGui::Text("In chunk pos: (%d, %d, %d)", local.chunk_pos.x, local.chunk_pos.y, local.chunk_pos.z);
		ImGui::Text("In block pos: (%d, %d, %d)", local.block_pos.x, local.block_pos.y, local.block_pos.z);

		ImGui::NewLine();
		ImGui::Text("Chunk count: %d", Chunk::chunks.size());
		ImGui::Text("Chunk size: %d", Chunk::CHUNK_SIZE);
		int cnt = 0;
		for (auto& p : Chunk::chunks)
			if (p.second) cnt++;
		ImGui::Text("Non-null chunks: %d", cnt);

		ImGui::NewLine();
		ImGui::Text("Flying: %s", activeCursor ? "False" : "True");

		static bool init = true;
		if (!init)
		{
			ImGui::NewLine();
			const glm::vec3 camPos = Render::GetCamera()->GetPos();
			float t = WorldGen::GetTemperature(camPos.x, camPos.y, camPos.z);
			float h = WorldGen::GetHumidity(camPos.x, camPos.z);
			WorldGen::TerrainType tt = WorldGen::GetTerrainType(camPos);
			ImGui::Text("Biome info: ");
			ImGui::Text("Temperature: %.2f", t);
			ImGui::Text("Humidity: %.2f", h);
			ImGui::Text("Terrain: %d", (unsigned)tt);
			ImGui::Text("Biome name: %s", BiomeManager::GetBiome(t, h, tt));
			ImGui::NewLine();
		}
		init = false;

		int dist = 5;
		ImGui::Text("Voxel raycast information:");
		ImGui::Text("Ray length: %d", dist);
		raycast(
			Render::GetCamera()->GetPos(),
			Render::GetCamera()->front,
			dist,
			std::function<bool(float, float, float, BlockPtr, glm::vec3)>
			([&](float x, float y, float z, BlockPtr block, glm::vec3 side)->bool
		{
			if (!block || block->GetType() == Block::bAir)
				return false;

			ImGui::Text("Block Type: %d", (unsigned)block->GetType());
			ImGui::Text("Write Strength: %d", block->WriteStrength());
			ImGui::Text("Light Value: %d", block->LightValue());
			ImGui::Text("Block pos:  (%.2f, %.2f, %.2f)", x, y, z);
			ImGui::Text("Block side: (%.2f, %.2f, %.2f)", side.x, side.y, side.z);
			//glm::vec3 color = Block::PropertiesTable[block->GetType()].color;
			//ImGui::ColorPicker3("colorr", )

			ShaderPtr curr = Shader::shaders["flat_color"];
			curr->Use();
			curr->setMat4("u_model", glm::translate(glm::mat4(1), glm::vec3(x, y, z) + .5f));
			curr->setMat4("u_view", Render::GetCamera()->GetView());
			curr->setMat4("u_proj", Render::GetCamera()->GetProj());
			curr->setVec4("u_color", glm::vec4(1, 1, 1, .4f));
			glLineWidth(2);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			renderer_.DrawCube();

			return true;
		}
		));

		ImGui::End();
	}

	// TODO: make toggling shadows/reflections ACTUALLY disable them completely
	{
		ImGui::Begin("Global Settings");
		if (ImGui::Checkbox("Compute baked AO", &Settings::GFX::blockAO))
			chunkManager_.ReloadAllChunks();
		if (ImGui::Checkbox("Shadows", &renderer_.renderShadows))
			renderer_.ClearCSM();
		if (ImGui::Checkbox("Reflections", &renderer_.doGeometryPass))
			renderer_.ClearGGuffer();
		ImGui::NewLine();
		ImGui::Text("Post processing:");
		ImGui::Checkbox("Sharpen Filter", &renderer_.ppSharpenFilter);
		ImGui::Checkbox("Blur Filter", &renderer_.ppBlurFilter);
		ImGui::Checkbox("Edge detection", &renderer_.ppEdgeDetection);
		ImGui::Checkbox("Chromatic Aberration", &renderer_.ppChromaticAberration);
		ImGui::End();
	}
}

// TODO: fix this
void Level::CheckCollision()
{
	auto cam = Render::GetCamera();
	//constexpr glm::vec3 dirs[] =
	//{
	//	// cardinal directions
	//	{ 0, 0, 0 }, // center
	//	{ 0, 0, 1 }, // front
	//	{ 0, 0,-1 }, // back
	//	{ 1, 0, 0 }, // right
	//	{-1, 0, 0 }, // left
	//	{ 0, 1, 0 }, // up
	//	{ 0,-1, 0 }, // down
	//	// +y corners
	//	{-1, 1, 1 }, // left top front
	//	{ 1, 1, 1 }, // right top front
	//	{-1, 1,-1 }, // left top back
	//	{ 1, 1,-1 }, // right top back
	//	// -y corners
	//	{-1,-1, 1 }, // left bottom front
	//	{ 1,-1, 1 }, // right bottom front
	//	{-1,-1,-1 }, // left bottom back
	//	{ 1,-1,-1 }, // right bottom back
	//	// neutral y corners
	//	{-1, 0, 1 }, // left middle front
	//	{ 1, 0, 1 }, // right middle front
	//	{-1, 0,-1 }, // left middle back
	//	{ 1, 0,-1 }, // right middle back
	//};
	//ImGui::Begin("Colliding?");
	//bool isreallycolliding = false;
	//for (const auto& dir : dirs)
	//{
	//	bool iscolliding = false;
	//	auto pos = glm::ivec3(glm::round(cam->GetPos() + dir - 0.5f)) ;
	//	//auto pos = glm::ivec3((cam->GetPos() + dir + .5f)) ;
	//	if (GetBlockAt(pos).GetType() != Block::bAir)
	//		iscolliding = Box(*cam).IsColliding(Box(pos));
	//	if (iscolliding)
	//		isreallycolliding = true;
	//	//ImGui::Text("Dir: (%d, %d, %d)", dir.x, dir.y, dir.z);
	//	ImGui::Text("iPos: (%d, %d, %d)", pos.x, pos.y, pos.z);
	//	//ImGui::Text("c = %s", iscolliding ? "true" : "false");
	//	//ImGui::NewLine();
	//}
	//ImGui::Text("fPos: (%.2f, %.2f, %.2f)", cam->GetPos().x, cam->GetPos().y, cam->GetPos().z);
	//ImGui::Text("%d", isreallycolliding);
	//ImGui::End();

	ImGui::Begin("Collision");
	Box camBox(*cam);
	auto min = glm::ivec3(glm::floor(camBox.min));
	auto max = glm::ivec3(glm::ceil(camBox.max));
	for (int x = min.x; x < max.x; x++)
	{
		for (int y = min.y; y < max.y; y++)
		{
			for (int z = min.z; z < max.z; z++)
			{
				glm::ivec3 pos(x, y, z);
				ImGui::Text("Checking (%d, %d, %d)", x, y, z);
				if (GetBlockAt(pos).GetType() != Block::bAir)
				{
					Box blockBox(pos);
					if (camBox.IsColliding(blockBox))
					{
						ImGui::Text("Colliding with (%d, %d, %d)", x, y, z);
						auto diff = (camBox.GetPosition() - blockBox.GetPosition());
						auto newPos = cam->GetPos();
						//if (diff.x > diff.y && diff.x > diff.z)
						//	newPos.x -= diff.x - .5f;
						//else if (diff.y > diff.z)
						//	newPos.y -= diff.y - .5f;
						//else
						//	newPos.z -= diff.z - .5f;
						//cam->SetPos(newPos);
					}
				}
				//ImGui::NewLine();
			}
		}
	}
	ImGui::End();
}

void Level::CheckInteraction()
{
	checkBlockPlacement();
	checkBlockDestruction();
}

// force updates a block in a location
void Level::UpdateBlockAt(glm::ivec3 wpos, Block::BlockType ty)
{
	chunkManager_.UpdateBlock(wpos, ty, std::numeric_limits<unsigned char>::max() / 2);
}

// updates a block in a location IFF the new block has a sufficiently high write strength
void Level::GenerateBlockAt(glm::ivec3 wpos, Block b)
{
	chunkManager_.UpdateBlock(wpos, b.GetType(), b.WriteStrength());
}

void Level::GenerateBlockAtCheap(glm::ivec3 wpos, Block b)
{
	chunkManager_.UpdateBlockCheap(wpos, b);
}

void Level::UpdatedChunk(ChunkPtr chunk)
{
	chunkManager_.UpdatedChunk(chunk);
}

Block Level::GetBlockAt(glm::ivec3 wpos)
{
	return chunkManager_.GetBlock(wpos);
}

void Level::checkBlockPlacement()
{
	if (Input::Mouse().pressed[GLFW_MOUSE_BUTTON_2])
	{
		raycast(
			Render::GetCamera()->GetPos(),
			Render::GetCamera()->front,
			5,
			std::function<bool(float, float, float, BlockPtr, glm::vec3)>
			([&](float x, float y, float z, BlockPtr block, glm::vec3 side)->bool
		{
			if (!block || block->GetType() == Block::bAir)
				return false;

			UpdateBlockAt(glm::ivec3(x, y, z) + glm::ivec3(side), hud_.selected_);

			//Chunk::AtWorld(glm::ivec3(x, y, z) + glm::ivec3(side))->SetType(Block::bStone);
			//for (auto& chunk : updatedChunks_)
			//{
			//	if (chunk == Chunk::chunks[Chunk::worldBlockToLocalPos(glm::ivec3(x, y, z)).chunk_pos])
			//		return false;
			//}
			//updatedChunks_.push_back(Chunk::chunks[Chunk::worldBlockToLocalPos(glm::ivec3(x, y, z)).chunk_pos]);
			
			return true;
		}
		));
	}
}

void Level::checkBlockDestruction()
{
	if (Input::Mouse().pressed[GLFW_MOUSE_BUTTON_1] && 
		!ImGui::IsAnyItemHovered() && 
		!ImGui::IsAnyItemActive() && 
		!ImGui::IsAnyItemFocused())
	{
		raycast(
			Render::GetCamera()->GetPos(),
			Render::GetCamera()->front,
			5,
			std::function<bool(float, float, float, BlockPtr, glm::vec3)>
			([&](float x, float y, float z, BlockPtr block, glm::vec3 side)->bool
		{
			if (!block || block->GetType() == Block::bAir)
				return false;

			UpdateBlockAt(glm::ivec3(x, y, z), Block::bAir);

			//block->SetType(Block::bAir);
			//for (auto& chunk : updatedChunks_)
			//{
			//	if (chunk == Chunk::chunks[Chunk::worldBlockToLocalPos(glm::ivec3(x, y, z)).chunk_pos])
			//		return false;
			//}
			//updatedChunks_.push_back(Chunk::chunks[Chunk::worldBlockToLocalPos(glm::ivec3(x, y, z)).chunk_pos]);
			
			return true;
		}
		));
	}
}
