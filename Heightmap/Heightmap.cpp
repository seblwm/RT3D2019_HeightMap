#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

#include "CommonApp.h"

#include <stdio.h>
#include <vector>
#include <DirectXMath.h>
using namespace DirectX;
using namespace std;

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

class HeightMapApplication : public CommonApp
{
  public:
  protected:
	bool HandleStart();
	void HandleStop();
	void HandleUpdate();
	void HandleRender();
	bool LoadHeightMap(char* filename, float gridSize);

  private:
	ID3D11Buffer* m_pHeightMapBuffer;
	float m_rotationAngle;
	int m_HeightMapWidth;
	int m_HeightMapLength;
	int m_HeightMapVtxCount;
	int m_HeightMapQuadCountWidth;
	int m_HeightMapQuadCountLength;
	XMFLOAT3* m_pHeightMap;
	vector<XMFLOAT3> vertexList4Triangles;
	Vertex_Pos3fColour4ubNormal3f* m_pMapVtxs;
	float m_cameraZ;
	void cubeVertices(VertexColour);
	void mapTiles(VertexColour);
	void oddRow(vector<XMFLOAT3>&, int);
	void evenRow(vector<XMFLOAT3>&, int);
	void lastRow(vector<XMFLOAT3>&, int);
	void calcFaceNormal(vector<XMFLOAT3>&, int, XMFLOAT3&, bool&);
	XMFLOAT3 calcNormalToPlane(XMFLOAT3, XMFLOAT3, XMFLOAT3);
	void normaliseVector(XMFLOAT3&);
	XMFLOAT3 calcXProduct(XMFLOAT3,XMFLOAT3);
	XMFLOAT3 calcVectorA_B(XMFLOAT3, XMFLOAT3);
	
};

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

bool HeightMapApplication::HandleStart()
{
	this->SetWindowTitle("HeightMap");

	LoadHeightMap("Heightmap.bmp", 1.0f);

	m_cameraZ = 50.0f;

	m_pHeightMapBuffer = NULL;

	m_rotationAngle = 0.f;

	if(!this->CommonApp::HandleStart())
		return false;

	static const VertexColour MAP_COLOUR(200, 255, 255, 255);

	/////////////////////////////////////////////////////////////////
	// Clearly this code will need changing to render the heightmap
	/////////////////////////////////////////////////////////////////

	//m_HeightMapVtxCount = 6 * 6;
	m_HeightMapVtxCount = (m_HeightMapLength - 1) * m_HeightMapWidth * 2;
	m_HeightMapQuadCountWidth = m_HeightMapWidth - 1;
	m_HeightMapQuadCountLength = m_HeightMapLength - 1;
	m_pMapVtxs = new Vertex_Pos3fColour4ubNormal3f[m_HeightMapVtxCount];
	XMFLOAT3 v0, v1, v2, v3, v4, v5;// = 1 quad
	XMFLOAT3 normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

	mapTiles(MAP_COLOUR);//get heightmap vertices in order for trianglestrip
	bool clckWise = true;
	for (size_t vIndex = 0; vIndex < m_HeightMapVtxCount; vIndex++)
	{
		calcFaceNormal(vertexList4Triangles, vIndex, normal, clckWise);
		m_pMapVtxs[vIndex] = Vertex_Pos3fColour4ubNormal3f(vertexList4Triangles.at(vIndex), MAP_COLOUR, normal);
	}

	/////////////////////////////////////////////////////////////////
	// Down to here
	/////////////////////////////////////////////////////////////////
	

	m_pHeightMapBuffer = CreateImmutableVertexBuffer(m_pD3DDevice, sizeof Vertex_Pos3fColour4ubNormal3f * m_HeightMapVtxCount, m_pMapVtxs);

	delete m_pMapVtxs;

	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void HeightMapApplication::HandleStop()
{
	Release(m_pHeightMapBuffer);

	this->CommonApp::HandleStop();
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void HeightMapApplication::HandleUpdate()
{
	m_rotationAngle += .01f;

	if(this->IsKeyPressed('Q'))
	{
		if(m_cameraZ > 20.0f)
			m_cameraZ -= 2.0f;
	}

	if(this->IsKeyPressed('A'))
	{
		m_cameraZ += 2.0f;
	}
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void HeightMapApplication::HandleRender()
{
	XMFLOAT3 vCamera(sin(m_rotationAngle) * m_cameraZ, m_cameraZ / 2, cos(m_rotationAngle) * m_cameraZ);
	XMFLOAT3 vLookat(0.0f, 0.0f, 0.0f);
	XMFLOAT3 vUpVector(0.0f, 1.0f, 0.0f);

	XMMATRIX matView;
	matView = XMMatrixLookAtLH(XMLoadFloat3(&vCamera), XMLoadFloat3(&vLookat), XMLoadFloat3(&vUpVector));

	XMMATRIX matProj;
	matProj = XMMatrixPerspectiveFovLH(float(XM_PI / 4), 2, 1.5f, 5000.0f);

	this->SetViewMatrix(matView);
	this->SetProjectionMatrix(matProj);

	this->EnablePointLight(0, XMFLOAT3(100.0f, 100.f, -100.f), XMFLOAT3(1.f, 1.f, 1.f));

	this->Clear(XMFLOAT4(.2f, .2f, .6f, 1.f));

	this->DrawUntexturedLit(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, m_pHeightMapBuffer, NULL, m_HeightMapVtxCount);
}

//////////////////////////////////////////////////////////////////////
// LoadHeightMap
// Original code sourced from rastertek.com
//////////////////////////////////////////////////////////////////////
bool HeightMapApplication::LoadHeightMap(char* filename, float gridSize)
{
	FILE* filePtr;
	int error;
	unsigned int count;
	BITMAPFILEHEADER bitmapFileHeader;
	BITMAPINFOHEADER bitmapInfoHeader;
	int imageSize, i, j, k, index;
	unsigned char* bitmapImage;
	unsigned char height;

	// Open the height map file in binary.
	error = fopen_s(&filePtr, filename, "rb");
	if(error != 0)
	{
		return false;
	}

	// Read in the file header.
	count = fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, filePtr);
	if(count != 1)
	{
		return false;
	}

	// Read in the bitmap info header.
	count = fread(&bitmapInfoHeader, sizeof(BITMAPINFOHEADER), 1, filePtr);
	if(count != 1)
	{
		return false;
	}

	// Save the dimensions of the terrain.
	m_HeightMapWidth = bitmapInfoHeader.biWidth;
	m_HeightMapLength = bitmapInfoHeader.biHeight;

	// Calculate the size of the bitmap image data.
	imageSize = m_HeightMapWidth * m_HeightMapLength * 3;

	// Allocate memory for the bitmap image data.
	bitmapImage = new unsigned char[imageSize];
	if(!bitmapImage)
	{
		return false;
	}

	// Move to the beginning of the bitmap data.
	fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);

	// Read in the bitmap image data.
	count = fread(bitmapImage, 1, imageSize, filePtr);
	if(count != imageSize)
	{
		return false;
	}

	// Close the file.
	error = fclose(filePtr);
	if(error != 0)
	{
		return false;
	}

	// Create the structure to hold the height map data.
	m_pHeightMap = new XMFLOAT3[m_HeightMapWidth * m_HeightMapLength];
	if(!m_pHeightMap)
	{
		return false;
	}

	// Initialize the position in the image data buffer.
	k = 0;

	// Read the image data into the height map.
	//for(j = 0; j < m_HeightMapLength; j++)
	//{
	//	for(i = 0; i < m_HeightMapWidth; i++)
	//	{
	//		height = bitmapImage[k];

	//		index = (m_HeightMapLength * j) + i;

	//		m_pHeightMap[index].x = (float)(i - (m_HeightMapWidth / 2)) * gridSize;
	//		m_pHeightMap[index].y = (float)height / 16 * gridSize;
	//		m_pHeightMap[index].z = (float)(j - (m_HeightMapLength / 2)) * gridSize;

	//		k += 3;
	//	}
	//}
	
	for (j = m_HeightMapLength - 1; j >= 0; j--)
		for (i = 0; i < m_HeightMapWidth; i++) {
			height = bitmapImage[k];
			index = (m_HeightMapLength * j) + i;
			m_pHeightMap[index].x = (float)(i - (m_HeightMapWidth / 2)) * gridSize;
			m_pHeightMap[index].y = (float)height / 16 * gridSize;
			m_pHeightMap[index].z = (float)((m_HeightMapLength / 2) - j) * gridSize;
			k += 3;
		}
	

	

	// Release the bitmap image data.
	delete[] bitmapImage;
	bitmapImage = 0;

	return true;
}

void HeightMapApplication::cubeVertices(VertexColour MAP_COLOUR)
{

	// Side 1 - Front face
	m_pMapVtxs[0] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 0.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, -1.0f));
	m_pMapVtxs[1] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, -1.0f));
	m_pMapVtxs[2] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 0.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, -1.0f));
	m_pMapVtxs[3] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 0.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, -1.0f));
	m_pMapVtxs[4] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, -1.0f));
	m_pMapVtxs[5] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, -1.0f));
	// Side 2 - Right face
	m_pMapVtxs[6] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 0.0f, 0.0f), MAP_COLOUR, XMFLOAT3(1.0f, 0.0f, 0.0f));
	m_pMapVtxs[7] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(1.0f, 0.0f, 0.0f));
	m_pMapVtxs[8] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 0.0f, 10.0f), MAP_COLOUR, XMFLOAT3(1.0f, 0.0f, 0.0f));
	m_pMapVtxs[9] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 0.0f, 10.0f), MAP_COLOUR, XMFLOAT3(1.0f, 0.0f, 0.0f));
	m_pMapVtxs[10] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(1.0f, 0.0f, 0.0f));
	m_pMapVtxs[11] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 10.0f, 10.0f), MAP_COLOUR, XMFLOAT3(1.0f, 0.0f, 0.0f));
	// Side 3 - Rear face
	m_pMapVtxs[12] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 0.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, 1.0f));
	m_pMapVtxs[13] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 10.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, 1.0f));
	m_pMapVtxs[14] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 0.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, 1.0f));
	m_pMapVtxs[15] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 0.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, 1.0f));
	m_pMapVtxs[16] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 10.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, 1.0f));
	m_pMapVtxs[17] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, 0.0f, 1.0f));
	// Side 4 - Left face
	m_pMapVtxs[18] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 0.0f, 0.0f), MAP_COLOUR, XMFLOAT3(-1.0f, 0.0f, 0.0f));
	m_pMapVtxs[19] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 10.0f), MAP_COLOUR, XMFLOAT3(-1.0f, 0.0f, 0.0f));
	m_pMapVtxs[20] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(-1.0f, 0.0f, 0.0f));
	m_pMapVtxs[21] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 0.0f, 0.0f), MAP_COLOUR, XMFLOAT3(-1.0f, 0.0f, 0.0f));
	m_pMapVtxs[22] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 0.0f, 10.0f), MAP_COLOUR, XMFLOAT3(-1.0f, 0.0f, 0.0f));
	m_pMapVtxs[23] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 10.0f), MAP_COLOUR, XMFLOAT3(-1.0f, 0.0f, 0.0f));
	// Side 5 - Top face
	m_pMapVtxs[24] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 1.0f, 0.0f));
	m_pMapVtxs[25] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, 1.0f, 0.0f));
	m_pMapVtxs[26] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 1.0f, 0.0f));
	m_pMapVtxs[27] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 1.0f, 0.0f));
	m_pMapVtxs[28] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, 1.0f, 0.0f));
	m_pMapVtxs[29] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 10.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, 1.0f, 0.0f));
	// Side 6 - Bottom face
	m_pMapVtxs[30] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 0.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, -1.0f, 0.0f));
	m_pMapVtxs[31] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 0.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, -1.0f, 0.0f));
	m_pMapVtxs[32] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 0.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, -1.0f, 0.0f));
	m_pMapVtxs[33] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 0.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, -1.0f, 0.0f));
	m_pMapVtxs[34] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(10.0f, 0.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, -1.0f, 0.0f));
	m_pMapVtxs[35] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 0.0f, 10.0f), MAP_COLOUR, XMFLOAT3(0.0f, -1.0f, 0.0f));
}

void HeightMapApplication::mapTiles(VertexColour MAP_COLOUR)
{
	XMFLOAT3 normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	//m_pMapVtxs[0] = Vertex_Pos3fColour4ubNormal3f(XMFLOAT3(0.0f, 10.0f, 0.0f), MAP_COLOUR, XMFLOAT3(0.0f, 1.0f, 0.0f));
	for (size_t rowNum = 1; rowNum < m_HeightMapLength; rowNum++)
	{
		//if (rowNum == m_HeightMapLength - 1)//last row
		//{
		//	lastRow(vertexList4Triangles, rowNum);
		//}
		//else
		//{
			if (rowNum % 2 != 0)
			{
				oddRow(vertexList4Triangles, rowNum);
			}
			else
			{
				evenRow(vertexList4Triangles, rowNum);
			}
			
		//}
	}
}



void HeightMapApplication::oddRow(vector<XMFLOAT3>& vertices, int rowNum)
{
	//rowNum cannot be 0.
	//vertices.push_back(m_pHeightMap[(rowNum * m_HeightMapWidth)]);//first vertex for beginning of row -> 0th
	for (size_t vSelector = 0; vSelector < m_HeightMapWidth; vSelector++)
	{
		//push bottom -> 1st
		vertices.push_back(m_pHeightMap[((rowNum * m_HeightMapWidth))+(vSelector)]);
		//push top -> 2nd
		vertices.push_back(m_pHeightMap[((rowNum - 1) * m_HeightMapWidth) + (vSelector)]);
	}
}

void HeightMapApplication::evenRow(vector<XMFLOAT3>& vertices, int rowNum)
{
	for (size_t vSelector = 0; vSelector < m_HeightMapWidth; vSelector++)
	{
			//push top
			vertices.push_back(m_pHeightMap[(((rowNum - 1) * m_HeightMapWidth) + (m_HeightMapWidth - 1)) - vSelector]);//vertex at end of row to start

			//push bottom
			vertices.push_back(m_pHeightMap[(((rowNum)* m_HeightMapWidth) + (m_HeightMapWidth - 1)) - vSelector]);
	}
}

void HeightMapApplication::lastRow(vector<XMFLOAT3>& vertices, int rowNum)
{

	if (rowNum % 2 == 0)//if even go left to right
	{
		// if vSelector = heightmapwidth - 1
		for (size_t vSelector = 0; vSelector < m_HeightMapWidth; vSelector++)
		{
			if (vSelector < m_HeightMapWidth - 1)
			{
				//push top
				vertices.push_back(m_pHeightMap[(((rowNum - 1) * m_HeightMapWidth) + (m_HeightMapWidth - 1)) - vSelector]);//vertex at end of row to start

				//push bottom
				vertices.push_back(m_pHeightMap[(((rowNum)* m_HeightMapWidth) + (m_HeightMapWidth - 1)) - vSelector]);
			}
			else
			{
				//push bottom
				vertices.push_back(m_pHeightMap[(((rowNum)* m_HeightMapWidth) + (m_HeightMapWidth - 1)) - vSelector]);
				//push top
				vertices.push_back(m_pHeightMap[(((rowNum - 1) * m_HeightMapWidth) + (m_HeightMapWidth - 1)) - vSelector]);//vertex at end of row to start
			}
		}
	}
	else
	{
		for (size_t vSelector = 0; vSelector < m_HeightMapWidth; vSelector++)
		{
			if (vSelector < m_HeightMapWidth - 1) {
				//push bottom -> 1st
				vertices.push_back(m_pHeightMap[((rowNum * m_HeightMapWidth)) + (vSelector)]);
				//push top -> 2nd
				vertices.push_back(m_pHeightMap[vSelector]);
			}
			else
			{
				//push top
				vertices.push_back(m_pHeightMap[vSelector]);
				//push bottom
				vertices.push_back(m_pHeightMap[((rowNum * m_HeightMapWidth)) + (vSelector)]);
			}
		}
	}
}

void HeightMapApplication::calcFaceNormal(vector<XMFLOAT3>& verticesInFace, int vIndex, XMFLOAT3& normal, bool& clckWise)
{
	if ((vIndex % 3 == 0) && (vIndex < m_HeightMapVtxCount - 3))
	{
		//calc face normal
		//vertexList4Triangles.at(vIndex) = 1st vertex in triangle
		//vertexList4Triangles.at(vIndex + 1) = 2nd vertex in triangle
		//vertexList4Triangles.at(vIndex + 2) = 3rd vertex in triangle
		if (clckWise)
		{
			normal = calcNormalToPlane(vertexList4Triangles.at(vIndex), vertexList4Triangles.at(vIndex + 1), vertexList4Triangles.at(vIndex + 2));
			clckWise = false;
		}
		else
		{	//to adjust for the winding order
			normal = calcNormalToPlane(vertexList4Triangles.at(vIndex + 2), vertexList4Triangles.at(vIndex + 1), vertexList4Triangles.at(vIndex));
			clckWise = true;
		}
		normaliseVector(normal);
	}
}

XMFLOAT3 HeightMapApplication::calcNormalToPlane(XMFLOAT3 vA, XMFLOAT3 vB, XMFLOAT3 vC)// didn't realise an equivalent was included in the framework
{
	XMFLOAT3 XProd = calcXProduct(calcVectorA_B(vA,vB),calcVectorA_B(vB,vC));
	return XProd;
}

void HeightMapApplication::normaliseVector(XMFLOAT3 &vIn)
{
	double length = sqrt(vIn.x*vIn.x + vIn.y*vIn.y + vIn.z*vIn.z);
	vIn.x = vIn.x / length;
	vIn.y = vIn.y / length;
	vIn.z = vIn.z / length;
}

XMFLOAT3 HeightMapApplication::calcXProduct(XMFLOAT3 v1, XMFLOAT3 v2)
{
	float x, y, z;
	x = (v1.y*v2.z) - (v1.z*v2.y);
	y = (v1.z*v2.x) - (v1.x*v2.z);
	z = (v1.x*v2.y) - (v1.y*v2.x);
	return XMFLOAT3(x, y, z);
}
XMFLOAT3 HeightMapApplication::calcVectorA_B(XMFLOAT3 vA, XMFLOAT3 vB)
{
	float x, y, z;
	x = vA.x - vB.x;
	y = vA.y - vB.y;
	z = vA.z - vB.z;
	return XMFLOAT3(x,y,z);
}
//XMFLOAT3 XMFLOAT3::operator%(const XMFLOAT3 &vIn)
//{
//}
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	HeightMapApplication application;

	Run(&application);

	return 0;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
