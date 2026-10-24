
#include "Precomp.h"
#include "SoftwareCommands.h"

void SoftwareCommands::Reset()
{
	States.clear();
	Draws.clear();
	Vertices.clear();
	Chunks.clear();
	ChunkTriangles = 0;
	ChunkStart = 0;
}

int SoftwareCommands::AddState(const SoftwareState& state)
{
	States.push_back(state);
	return (int)States.size() - 1;
}

SoftwareDraw& SoftwareCommands::AddDraw(SoftwareDrawType type, uint32_t vertexCount, SoftwareVertex*& vertices)
{
	uint32_t triangles = 1;
	if (type == SoftwareDrawType::Polygon)
		triangles = vertexCount - 2;
	else if (type == SoftwareDrawType::Triangles)
		triangles = vertexCount / 3;
	else if (type == SoftwareDrawType::Tile)
		triangles = 2;
	if (ChunkTriangles > 0 && ChunkTriangles + triangles > MaxTriangles)
		CloseChunk();

	Draws.emplace_back();
	SoftwareDraw& draw = Draws.back();
	draw.Type = type;
	draw.State = CurrentState();
	draw.FirstVertex = (uint32_t)Vertices.size();
	draw.VertexCount = vertexCount;
	Vertices.resize(Vertices.size() + vertexCount);
	vertices = Vertices.data() + draw.FirstVertex;
	ChunkTriangles += triangles;
	return draw;
}

void SoftwareCommands::CloseChunk()
{
	if (Draws.size() > ChunkStart)
	{
		SoftwareChunk chunk;
		chunk.FirstDraw = ChunkStart;
		chunk.LastDraw = (uint32_t)Draws.size();
		Chunks.push_back(chunk);
		ChunkStart = (uint32_t)Draws.size();
	}
	ChunkTriangles = 0;
}
