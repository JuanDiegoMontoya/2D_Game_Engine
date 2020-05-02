#include "stdafx.h"

#include <Pipeline.h>
#include "Renderer.h"
#include <camera.h>
#include <Frustum.h>

#include "vbo.h"
#include "vao.h"
#include "ibo.h"
#include "chunk.h"
#include "block.h"
#include "shader.h"
#include <Vertices.h>
#include <sstream>
#include "settings.h"
#include "misc_utils.h"


Chunk::Chunk()
{
}


Chunk::~Chunk()
{
}


Chunk::Chunk(const Chunk& other)
{
	*this = other;
}


// copy assignment operator for serialization
Chunk& Chunk::operator=(const Chunk& rhs)
{
	//this->pos_ = rhs.pos_;
	this->SetPos(rhs.pos_);
	std::copy(std::begin(rhs.blocks), std::end(rhs.blocks), std::begin(this->blocks));
	std::copy(std::begin(rhs.lightMap), std::end(rhs.lightMap), std::begin(this->lightMap));
	return *this;
}


void Chunk::Update()
{

	// in the future, make this function perform other tick update actions,
	// such as updating N random blocks (like in Minecraft)
}


void Chunk::Render()
{
	//ASSERT(vao_ && positions_ && normals_ && colors_);
	if (vao_)
	{
		vao_->Bind();
		//glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
		ibo_->Bind();
		glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, (void*)0);
	}
}


void Chunk::RenderWater()
{
	//if (active_ && wvao_)
	//{
	//	wvao_->Bind();
	//	//glDrawArrays(GL_TRIANGLES, 0, wvertexCount_);
	//	wibo_->Bind();
	//	glDrawElements(GL_TRIANGLES, windexCount_, GL_UNSIGNED_INT, (void*)0);
	//}
}


void Chunk::BuildBuffers()
{
	std::lock_guard<std::mutex> lock(vertex_buffer_mutex_);
	// generate various vertex buffers
	{
		if (!vao_)
			vao_ = std::make_unique<VAO>();

		ibo_ = std::make_unique<IBO>(&tIndices[0], tIndices.size());

		vao_->Bind();
		encodedStuffVbo_ = std::make_unique<VBO>(encodedStuffArr.data(), sizeof(GLfloat) * encodedStuffArr.size());
		encodedStuffVbo_->Bind();
		glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat), (void*)0); // encoded stuff
		glEnableVertexAttribArray(0);

		lightingVbo_ = std::make_unique<VBO>(lightingArr.data(), sizeof(GLfloat) * lightingArr.size());
		lightingVbo_->Bind();
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat), (void*)0); // encoded lighting
		glEnableVertexAttribArray(1);

		vertexCount_ = encodedStuffArr.size();
		indexCount_ = tIndices.size();
		tIndices.clear();
		encodedStuffArr.clear();
		lightingArr.clear();
	}
}


#include <iomanip>
void Chunk::BuildMesh()
{
	high_resolution_clock::time_point benchmark_clock_ = high_resolution_clock::now();

	for (int i = 0; i < fCount; i++)
	{
		nearChunks[i] = chunks[this->pos_ + faces[i]];
	}
	std::lock_guard<std::mutex> lock(vertex_buffer_mutex_);


	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		// precompute first flat index part
		int zcsq = z * CHUNK_SIZE_SQRED;
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			// precompute second flat index part
			int yczcsq = y * CHUNK_SIZE + zcsq;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				// this is what we would be doing every innermost iteration
				//int index = x + y * CHUNK_SIZE + z * CHUNK_SIZE_SQRED;
				// we only need to do addition
				int index = x + yczcsq;

				// skip fully transparent blocks
				const Block block = blocks[index];
				if (Block::PropertiesTable[block.GetTypei()].invisible)
					continue;

#if MARCHED_CUBES
				//buildBlockVertices_marched_cubes(pos, At(x, y, z));
				buildBlockVertices_marched_cubes({ x, y, z }, block);
#else
				//buildBlockVertices_normal(pos, Render::cube_norm_tex_vertices, 48, At(x, y, z));
				buildBlockVertices_normal({ x, y, z }, block);
#endif
			}
		}
	}

	duration<double> benchmark_duration_ = duration_cast<duration<double>>(high_resolution_clock::now() - benchmark_clock_);
	double milliseconds = benchmark_duration_.count() * 1000;
	if (accumcount > 1000)
	{
		accumcount = 0;
		accumtime = 0;
	}
	accumtime = accumtime + milliseconds;
	accumcount = accumcount + 1;
	//std::cout 
	//	<< std::setw(-2) << std::showpoint << std::setprecision(4) << accumtime / accumcount << " ms "
	//	<< "(" << milliseconds << ")"
	//	<< std::endl;
}


void Chunk::buildBlockVertices_normal(const glm::ivec3& pos, Block block)
{
	for (int f = Far; f < fCount; f++)
		buildBlockFace(f, pos, block);
}


void Chunk::buildBlockFace(
	int face,
	const glm::ivec3& blockPos,	// position of current block
	Block block)								// block-specific information)
{
	thread_local static localpos nearblock; // avoids unnecessary construction of vec3s
	glm::ivec3 nearFace = blockPos + faces[face];
	//glm::ivec3 chunkDiff = glm::floor(glm::vec3(nearFace) / 32.f);


	using namespace glm;
	ChunkPtr nearChunk = this;

	// if neighbor is out of this chunk, find which chunk it is in
	if (any(lessThan(nearFace, ivec3(0))) || any(greaterThanEqual(nearFace, ivec3(CHUNK_SIZE))))
	{
		fastWorldBlockToLocalPos(chunkBlockToWorldPos(nearFace), nearblock);
		nearChunk = nearChunks[face];
	}
	else
	{
		nearblock.block_pos = nearFace;
	}

	// for now, we won't make a mesh for faces adjacent to NULL chunks
	// in the future it may be wise to construct the mesh regardless
	if (nearChunk == nullptr)
		return;

	// neighboring block and light
	Block block2 = nearChunk->BlockAtCheap(nearblock.block_pos);
	Light light = nearChunk->LightAtCheap(nearblock.block_pos);

	// this block is water and other block isn't water and is above this block
	if (block2.GetType() != BlockType::bWater && block.GetType() == BlockType::bWater && (nearFace - blockPos).y > 0)
	{
		addQuad(blockPos, block, face, nearChunk, light);
		return;
	}
	// other block isn't air or water - don't add mesh
	if (block2.GetType() != BlockType::bAir && block2.GetType() != BlockType::bWater)
		return;
	// both blocks are water - don't add mesh
	if (block2.GetType() == BlockType::bWater && block.GetType() == BlockType::bWater)
		return;
	// this block is invisible - don't add mesh
	if (Block::PropertiesTable[int(block.GetType())].invisible)
		return;

	// if all tests are passed, generate this face of the block
	addQuad(blockPos, block, face, nearChunk, light);
}


void Chunk::addQuad(const glm::ivec3& lpos, Block block, int face, ChunkPtr nearChunk, Light light)
{
	int normalIdx = face;
	int texIdx = 100; // temp
	uint16_t lighting = light.Raw();
	light.SetS(15);

	// add 4 vertices representing a quad
	const GLfloat* data = Vertices::cube_light;
	int endQuad = (face + 1) * 12;
	for (int i = face * 12; i < endQuad; i += 3)
	{
		// transform vertices relative to chunk
		glm::vec3 vert(data[i + 0], data[i + 1], data[i + 2]);
		glm::uvec3 finalVert = glm::ceil(vert) + glm::vec3(lpos);// +0.5f;
		
		int cornerIdx = 2; // temp

		// compress attributes into 32 bits
		GLuint encoded = Encode(finalVert, normalIdx, texIdx, cornerIdx);

		float invOcclusion = 1;
		if (Settings::Graphics.blockAO)
			//invOcclusion = computeBlockAO(block, lpos, glm::vec3(vert), lpos + faces[face]);
			invOcclusion = vertexFaceAO(lpos, vert, faces[face]);
		//tColors.push_back(glm::vec4((glm::vec3(color.r, color.g, color.b)) * invOcclusion, color.a));
		auto tLight = light;
		tLight.Set(glm::vec4(tLight.Get()) * invOcclusion);
		lighting = tLight.Raw();

		// preserve bit ordering
		encodedStuffArr.push_back(glm::uintBitsToFloat(encoded));
		lightingArr.push_back(glm::uintBitsToFloat(lighting));
	}

	// add 6 indices defining 2 triangles from that quad
	int endIndices = (face + 1) * 6;
	for (int i = face * 6; i < endIndices; i++)
	{
		// refer to just placed vertices (4 of them)
		tIndices.push_back(Vertices::cube_indices_light_cw[i] + encodedStuffArr.size() - 4);
	}
}


// computes how many of the two adjacent faces to the corner (that aren't this block) are solid
// returns a brightness scalar
float Chunk::computeBlockAO(
	Block block,
	const glm::ivec3& blockPos,
	const glm::vec3& corner,
	const glm::ivec3& nearFace)
{
	glm::ivec3 normal = nearFace - blockPos;
	glm::ivec3 combined = glm::ivec3(corner * 2.f) + normal; // convert corner to -1 to 1 range
	glm::ivec3 c1(0);
	glm::ivec3 c2(0);
	for (int i = 0; i < 3; i++)
	{
		if (glm::abs(combined[i]) != 2)
		{
			if (combined[i] && c1 == glm::ivec3(0))
				c1[i] = combined[i];
			else if (c2 == glm::ivec3(0))
				c2[i] = combined[i];
		}
	}
	
	float occlusion = 1;
	const float occAmt = .2f;

	// block 1
	localpos nearblock1 = worldBlockToLocalPos(chunkBlockToWorldPos(blockPos + c1 + normal));
	auto findVal1 = chunks.find(nearblock1.chunk_pos);
	ChunkPtr near1 = findVal1 == chunks.end() ? nullptr : findVal1->second;
	if (!near1)
		goto B2AO;
	//if (!near1->active_)
	//	goto B2AO;
	if (near1->At(nearblock1.block_pos).GetType() != BlockType::bAir && near1->At(nearblock1.block_pos).GetType() != BlockType::bWater)
		occlusion -= occAmt;

B2AO:
	// block 2
	localpos nearblock2 = worldBlockToLocalPos(chunkBlockToWorldPos(blockPos + c2 + normal));
	auto findVal2 = chunks.find(nearblock2.chunk_pos);
	ChunkPtr near2 = findVal2 == chunks.end() ? nullptr : findVal2->second;
	if (!near2)
		goto BAO_END;
	//if (!near2->active_)
	//	goto BAO_END;
	if (near2->At(nearblock2.block_pos).GetType() != BlockType::bAir && near2->At(nearblock2.block_pos).GetType() != BlockType::bWater)
		occlusion -= occAmt;

	// check diagonal if not 'fully' occluded
	if (occlusion > 1 - 2.f * occAmt)
	{
		// block 3
		localpos nearblock3 = worldBlockToLocalPos(chunkBlockToWorldPos(blockPos + c1 + c2 + normal));
		auto findVal3 = chunks.find(nearblock3.chunk_pos);
		ChunkPtr near3 = findVal3 == chunks.end() ? nullptr : findVal3->second;
		if (!near3)
			goto BAO_END;
		//if (!near3->active_)
		//	goto BAO_END;
		if (near3->At(nearblock3.block_pos).GetType() != BlockType::bAir && near3->At(nearblock3.block_pos).GetType() != BlockType::bWater)
			occlusion -= occAmt;
	}

BAO_END:
	return occlusion;
}


// TODO: when making this function, use similar near chunk checking scheme to
// minimize amount of searching in the global chunk map
float Chunk::vertexFaceAO(const glm::vec3& lpos, const glm::vec3& cornerDir, const glm::vec3& norm)
{
	using namespace glm;

	int occluded = 0;

	// sides are components of the corner minus the normal direction
	vec3 sidesDir = cornerDir * 2.0f - norm;
	for (int i = 0; i < sidesDir.length(); i++)
	{
		if (sidesDir[i] != 0)
		{
			vec3 sideDir(0);
			sideDir[i] = sidesDir[i];
			vec3 sidePos = lpos + sideDir + norm;
			if (all(greaterThanEqual(sidePos, vec3(0))) && all(lessThan(sidePos, vec3(CHUNK_SIZE))))
				if (At(ivec3(sidePos)).GetType() != BlockType::bAir)
					occluded++;
		}
	}

	ASSERT(occluded <= 2);
	if (occluded == 2)
		return 0.0f;
	
	vec3 cornerPos = lpos + (cornerDir * 2.0f);
	if (all(greaterThanEqual(cornerPos, vec3(0))) && all(lessThan(cornerPos, vec3(CHUNK_SIZE))))
		if (At(ivec3(cornerPos)).GetType() != BlockType::bAir)
			occluded++;

	return 1.0f - ((1.0f / 3.0f) * occluded);
}


// prints to console if errors are found
void TestCoordinateStuff()
{
	using namespace std;
	for (auto& p : Chunk::chunks)
	{
		using namespace std;
		//cout << p.first << '\n';
		if (!p.second)
			continue;
		if (p.first != p.second->GetPos())
			cout << "Hash key " << p.first 
			<< " does not match chunk internal position" 
			<< p.second->GetPos() << '\n';

		for (int x = 0; x < Chunk::CHUNK_SIZE; x++)
		{
			for (int y = 0; y < Chunk::CHUNK_SIZE; y++)
			{
				for (int z = 0; z < Chunk::CHUNK_SIZE; z++)
				{
					glm::ivec3 posLocal(x, y, z);
					glm::ivec3 posActual = p.second->chunkBlockToWorldPos(posLocal);
					localpos posCalc = Chunk::worldBlockToLocalPos(posActual);

					if (posCalc.chunk_pos != p.first)
						cout << "Calculated chunk position " << 
						posCalc.chunk_pos << 
						" does not match hash key " << p.first;
					if (posCalc.block_pos != posLocal)
						cout << "Calculated block position " <<
						posCalc.block_pos <<
						" does not match local pos " << posLocal;
				}
			}
		}
	}
}