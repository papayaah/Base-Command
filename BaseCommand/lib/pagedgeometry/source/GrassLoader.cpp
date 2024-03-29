/*-------------------------------------------------------------------------------------
Copyright (c) 2006 John Judnich

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
    1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
-------------------------------------------------------------------------------------*/

#include "GrassLoader.h"
#include "PagedGeometry.h"
#include "PropertyMaps.h"

#include "OgreRoot.h"
#include "OgreTimer.h"
#include "OgreCamera.h"
#include "OgreVector3.h"
#include "OgreQuaternion.h"
#include "OgreEntity.h"
#include "OgreString.h"
#include "OgreStringConverter.h"
#include "OgreMaterialManager.h"
#include "OgreMaterial.h"
#include "OgreHardwareBufferManager.h"
#include "OgreHardwareBuffer.h"
#include "OgreMeshManager.h"
#include "OgreMesh.h"
#include "OgreSubMesh.h"
#include "OgreLogManager.h"
#include "OgreTextureManager.h"
#include "OgreHardwarePixelBuffer.h"
#include "OgreRenderSystem.h"
#include "OgreRenderSystemCapabilities.h"
#include "OgreHighLevelGpuProgram.h"
#include "OgreHighLevelGpuProgramManager.h"
using namespace Ogre;


namespace Forests {

unsigned long GrassLoader::GUID = 0;

GrassLoader::GrassLoader(PagedGeometry *geom)
{
	GrassLoader::geom = geom;
	
	heightFunction = NULL;
	heightFunctionUserData = NULL;

	windDir = Vector3::UNIT_X;
	densityFactor = 1.0f;
	renderQueue = RENDER_QUEUE_6;

	windTimer.reset();
	lastTime = 0;
}

GrassLoader::~GrassLoader()
{
	std::list<GrassLayer*>::iterator it;
	for (it = layerList.begin(); it != layerList.end(); ++it){
		delete *it;
	}
	layerList.clear();
}

GrassLayer *GrassLoader::addLayer(const String &material)
{
	GrassLayer *layer = new GrassLayer(geom, this);
	layer->setMaterialName(material);
	layerList.push_back(layer);

	return layer;
}

void GrassLoader::deleteLayer(GrassLayer *layer)
{
	layerList.remove(layer);
	delete layer;
}

void GrassLoader::frameUpdate()
{
	unsigned long currentTime = windTimer.getMilliseconds();
	unsigned long ellapsedTime = currentTime - lastTime;
	lastTime = currentTime;

	float ellapsed = ellapsedTime / 1000.0f;
	
	//Update the vertex shader parameters
	std::list<GrassLayer*>::iterator it;
	for (it = layerList.begin(); it != layerList.end(); ++it){
		GrassLayer *layer = *it;

		layer->_updateShaders();

		GpuProgramParametersSharedPtr params = layer->material->getTechnique(0)->getPass(0)->getVertexProgramParameters();
		if (layer->animate){
			//Increment animation frame
			layer->waveCount += ellapsed * (layer->animSpeed * Math::PI);
			if (layer->waveCount > Math::PI*2) layer->waveCount -= Math::PI*2;

			//Set vertex shader parameters
			params->setNamedConstant("time", layer->waveCount);
			params->setNamedConstant("frequency", layer->animFreq);

			Vector3 direction = windDir * layer->animMag;
			params->setNamedConstant("direction", Vector4(direction.x, direction.y, direction.z, 0));

		}
	}
}

void GrassLoader::loadPage(PageInfo &page)
{
	//Seed random number generator based on page indexes
	uint16 xSeed = static_cast<uint16>(page.xIndex % 0xFFFF);
	uint16 zSeed = static_cast<uint16>(page.zIndex % 0xFFFF);
	uint32 seed = (xSeed << 16) | zSeed;
	srand(seed);

	//Keep a list of a generated meshes
	std::vector<Mesh*> *meshList = new std::vector<Mesh*>();
	page.userData = (void*)meshList;

	//Generate meshes
	std::list<GrassLayer*>::iterator it;
	for (it = layerList.begin(); it != layerList.end(); ++it){
		GrassLayer *layer = *it;

		//Calculate how much grass needs to be added
		float volume = page.bounds.width() * page.bounds.height();
		unsigned int grassCount = layer->density * densityFactor * volume;

		//The vertex buffer can't be allocated until the exact number of polygons is known,
		//so the locations of all grasses in this page must be precalculated.

		//Precompute grass locations into an array of floats. A plain array is used for speed;
		//there's no need to use a dynamic sized array since a maximum size is known.
		float *position = new float[grassCount*2];
		if (layer->densityMap){
			if (layer->densityMap->getFilter() == MAPFILTER_NONE)
				grassCount = layer->_populateGrassList_UnfilteredDM(page, position, grassCount);
			else if (layer->densityMap->getFilter() == MAPFILTER_BILINEAR)
				grassCount = layer->_populateGrassList_BilinearDM(page, position, grassCount);
		} else {
			grassCount = layer->_populateGrassList_Uniform(page, position, grassCount);
		}

		//Don't build a mesh unless it contains something
		if (grassCount != 0){
			Mesh *mesh = NULL;
			switch (layer->renderTechnique){
				case GRASSTECH_QUAD:
					mesh = generateGrass_QUAD(page, layer, position, grassCount);
					break;
				case GRASSTECH_CROSSQUADS:
					mesh = generateGrass_CROSSQUADS(page, layer, position, grassCount);
					break;
				case GRASSTECH_SPRITE:
					mesh = generateGrass_SPRITE(page, layer, position, grassCount);
					break;
			}
			assert(mesh);

			//Add the mesh to PagedGeometry
			Entity *entity = geom->getCamera()->getSceneManager()->createEntity(getUniqueID(), mesh->getName());
			entity->setRenderQueueGroup(renderQueue);
			entity->setCastShadows(false);
			addEntity(entity, page.centerPoint, Quaternion::IDENTITY, Vector3::UNIT_SCALE);
			geom->getSceneManager()->destroyEntity(entity);

			//Store the mesh pointer
			meshList->push_back(mesh);
		}

		//Delete the position list
		delete[] position;
	}
}

void GrassLoader::unloadPage(const PageInfo &page)
{
	//Unload meshes
	std::vector<Mesh*> *meshList = (std::vector<Mesh*>*)page.userData;
	std::vector<Mesh*>::iterator i;
	for (i = meshList->begin(); i != meshList->end(); ++i) {
		Mesh *mesh = *i;
		MeshManager::getSingleton().remove(mesh->getName());
	}
	meshList->clear();
	delete meshList;
}

Mesh *GrassLoader::generateGrass_QUAD(PageInfo &page, GrassLayer *layer, float *grassPositions, unsigned int grassCount)
{
	//Calculate the number of quads to be added
	unsigned int quadCount;
	quadCount = grassCount;

	//Create manual mesh to store grass quads
	MeshPtr mesh = MeshManager::getSingleton().createManual(getUniqueID(), ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
	SubMesh *subMesh = mesh->createSubMesh();
	subMesh->useSharedVertices = false;

	//Setup vertex format information
	subMesh->vertexData = new VertexData;
	subMesh->vertexData->vertexStart = 0;
	subMesh->vertexData->vertexCount = 4 * quadCount;

	VertexDeclaration* dcl = subMesh->vertexData->vertexDeclaration;
	size_t offset = 0;
	dcl->addElement(0, offset, VET_FLOAT3, VES_POSITION);
	offset += VertexElement::getTypeSize(VET_FLOAT3);
	dcl->addElement(0, offset, VET_COLOUR, VES_DIFFUSE);
	offset += VertexElement::getTypeSize(VET_COLOUR);
	dcl->addElement(0, offset, VET_FLOAT2, VES_TEXTURE_COORDINATES);
	offset += VertexElement::getTypeSize(VET_FLOAT2);

	//Populate a new vertex buffer with grass
	HardwareVertexBufferSharedPtr vbuf = HardwareBufferManager::getSingleton()
		.createVertexBuffer(offset, subMesh->vertexData->vertexCount, HardwareBuffer::HBU_STATIC_WRITE_ONLY, false);
	float* pReal = static_cast<float*>(vbuf->lock(HardwareBuffer::HBL_DISCARD));

	//Calculate size variance
	float rndWidth = layer->maxWidth - layer->minWidth;
	float rndHeight = layer->maxHeight - layer->minHeight;

	float minY = Math::POS_INFINITY, maxY = Math::NEG_INFINITY;
	float *posPtr = grassPositions;	//Position array "iterator"
	for (uint16 i = 0; i < grassCount; ++i)
	{
		//Get the x and z positions from the position array
		float x = *posPtr++;
		float z = *posPtr++;

		//Get the color at the grass position
		uint32 color;
		if (layer->colorMap)
			color = layer->colorMap->getColorAt(x, z);
		else
			color = 0xFFFFFFFF;

		//Calculate size
		float rnd = Math::UnitRandom();	//The same rnd value is used for width and height to maintain aspect ratio
		float halfScaleX = (layer->minWidth + rndWidth * rnd) * 0.5f;
		float scaleY = (layer->minHeight + rndHeight * rnd);

		//Calculate rotation
		float angle = Math::RangeRandom(0, Math::TWO_PI);
		float xTrans = Math::Cos(angle) * halfScaleX;
		float zTrans = Math::Sin(angle) * halfScaleX;

		//Calculate heights and edge positions
		float x1 = x - xTrans, z1 = z - zTrans;
		float x2 = x + xTrans, z2 = z + zTrans;

		float y1, y2;
		if (heightFunction){
			y1 = heightFunction(x1, z1, heightFunctionUserData);
			y2 = heightFunction(x2, z2, heightFunctionUserData);
		} else {
			y1 = 0;
			y2 = 0;
		}

		//Add vertices
		*pReal++ = (x1 - page.centerPoint.x); *pReal++ = (y1 + scaleY); *pReal++ = (z1 - page.centerPoint.z);	//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 0; *pReal++ = 0;								//uv

		*pReal++ = (x2 - page.centerPoint.x); *pReal++ = (y2 + scaleY); *pReal++ = (z2 - page.centerPoint.z);	//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 1; *pReal++ = 0;								//uv

		*pReal++ = (x1 - page.centerPoint.x); *pReal++ = (y1); *pReal++ = (z1 - page.centerPoint.z);			//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 0; *pReal++ = 1;								//uv

		*pReal++ = (x2 - page.centerPoint.x); *pReal++ = (y2); *pReal++ = (z2 - page.centerPoint.z);			//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 1; *pReal++ = 1;								//uv

		//Update bounds
		if (y1 < minY) minY = y1;
		if (y2 < minY) minY = y2;
		if (y1 + scaleY > maxY) maxY = y1 + scaleY;
		if (y2 + scaleY > maxY) maxY = y2 + scaleY;
	}

	vbuf->unlock();
	subMesh->vertexData->vertexBufferBinding->setBinding(0, vbuf);

	//Populate index buffer
	subMesh->indexData->indexStart = 0;
	subMesh->indexData->indexCount = 6 * quadCount;
	subMesh->indexData->indexBuffer = HardwareBufferManager::getSingleton()
		.createIndexBuffer(HardwareIndexBuffer::IT_16BIT, subMesh->indexData->indexCount, HardwareBuffer::HBU_STATIC_WRITE_ONLY);
	uint16* pI = static_cast<uint16*>(subMesh->indexData->indexBuffer->lock(HardwareBuffer::HBL_DISCARD));
	for (uint16 i = 0; i < quadCount; ++i)
	{
		uint16 offset = i * 4;

		*pI++ = 0 + offset;
		*pI++ = 2 + offset;
		*pI++ = 1 + offset;

		*pI++ = 1 + offset;
		*pI++ = 2 + offset;
		*pI++ = 3 + offset;
	}

	subMesh->indexData->indexBuffer->unlock();

	//Finish up mesh
	AxisAlignedBox bounds(page.bounds.left - page.centerPoint.x, minY, page.bounds.top - page.centerPoint.z,
		page.bounds.right - page.centerPoint.x, maxY, page.bounds.bottom - page.centerPoint.z);
	mesh->_setBounds(bounds);
	Vector3 temp = bounds.getMaximum() - bounds.getMinimum();
	mesh->_setBoundingSphereRadius(temp.length() * 0.5f);

	LogManager::getSingleton().setLogDetail(static_cast<LoggingLevel>(0));
	mesh->load();
	LogManager::getSingleton().setLogDetail(LL_NORMAL);

	//Apply grass material to mesh
	subMesh->setMaterialName(layer->material->getName());

	//Return the mesh
	return mesh.getPointer();
}

Mesh *GrassLoader::generateGrass_CROSSQUADS(PageInfo &page, GrassLayer *layer, float *grassPositions, unsigned int grassCount)
{
	//Calculate the number of quads to be added
	unsigned int quadCount;
	quadCount = grassCount * 2;

	//Create manual mesh to store grass quads
	MeshPtr mesh = MeshManager::getSingleton().createManual(getUniqueID(), ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
	SubMesh *subMesh = mesh->createSubMesh();
	subMesh->useSharedVertices = false;

	//Setup vertex format information
	subMesh->vertexData = new VertexData;
	subMesh->vertexData->vertexStart = 0;
	subMesh->vertexData->vertexCount = 4 * quadCount;

	VertexDeclaration* dcl = subMesh->vertexData->vertexDeclaration;
	size_t offset = 0;
	dcl->addElement(0, offset, VET_FLOAT3, VES_POSITION);
	offset += VertexElement::getTypeSize(VET_FLOAT3);
	dcl->addElement(0, offset, VET_COLOUR, VES_DIFFUSE);
	offset += VertexElement::getTypeSize(VET_COLOUR);
	dcl->addElement(0, offset, VET_FLOAT2, VES_TEXTURE_COORDINATES);
	offset += VertexElement::getTypeSize(VET_FLOAT2);

	//Populate a new vertex buffer with grass
	HardwareVertexBufferSharedPtr vbuf = HardwareBufferManager::getSingleton()
		.createVertexBuffer(offset, subMesh->vertexData->vertexCount, HardwareBuffer::HBU_STATIC_WRITE_ONLY, false);
	float* pReal = static_cast<float*>(vbuf->lock(HardwareBuffer::HBL_DISCARD));

	//Calculate size variance
	float rndWidth = layer->maxWidth - layer->minWidth;
	float rndHeight = layer->maxHeight - layer->minHeight;

	float minY = Math::POS_INFINITY, maxY = Math::NEG_INFINITY;
	float *posPtr = grassPositions;	//Position array "iterator"
	for (uint16 i = 0; i < grassCount; ++i)
	{
		//Get the x and z positions from the position array
		float x = *posPtr++;
		float z = *posPtr++;

		//Get the color at the grass position
		uint32 color;
		if (layer->colorMap)
			color = layer->colorMap->getColorAt(x, z);
		else
			color = 0xFFFFFFFF;

		//Calculate size
		float rnd = Math::UnitRandom();	//The same rnd value is used for width and height to maintain aspect ratio
		float halfScaleX = (layer->minWidth + rndWidth * rnd) * 0.5f;
		float scaleY = (layer->minHeight + rndHeight * rnd);

		//Calculate rotation
		float angle = Math::RangeRandom(0, Math::TWO_PI);
		float xTrans = Math::Cos(angle) * halfScaleX;
		float zTrans = Math::Sin(angle) * halfScaleX;

		//Calculate heights and edge positions
		float x1 = x - xTrans, z1 = z - zTrans;
		float x2 = x + xTrans, z2 = z + zTrans;

		float y1, y2;
		if (heightFunction){
			y1 = heightFunction(x1, z1, heightFunctionUserData);
			y2 = heightFunction(x2, z2, heightFunctionUserData);
		} else {
			y1 = 0;
			y2 = 0;
		}

		//Add vertices
		*pReal++ = (x1 - page.centerPoint.x); *pReal++ = (y1 + scaleY); *pReal++ = (z1 - page.centerPoint.z);	//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 0; *pReal++ = 0;								//uv

		*pReal++ = (x2 - page.centerPoint.x); *pReal++ = (y2 + scaleY); *pReal++ = (z2 - page.centerPoint.z);	//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 1; *pReal++ = 0;								//uv

		*pReal++ = (x1 - page.centerPoint.x); *pReal++ = (y1); *pReal++ = (z1 - page.centerPoint.z);			//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 0; *pReal++ = 1;								//uv

		*pReal++ = (x2 - page.centerPoint.x); *pReal++ = (y2); *pReal++ = (z2 - page.centerPoint.z);			//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 1; *pReal++ = 1;								//uv

		//Update bounds
		if (y1 < minY) minY = y1;
		if (y2 < minY) minY = y2;
		if (y1 + scaleY > maxY) maxY = y1 + scaleY;
		if (y2 + scaleY > maxY) maxY = y2 + scaleY;

		//Calculate heights and edge positions
		float x3 = x + zTrans, z3 = z - xTrans;
		float x4 = x - zTrans, z4 = z + xTrans;

		float y3, y4;
		if (heightFunction){
			y3 = heightFunction(x3, z3, heightFunctionUserData);
			y4 = heightFunction(x4, z4, heightFunctionUserData);
		} else {
			y3 = 0;
			y4 = 0;
		}

		//Add vertices
		*pReal++ = (x3 - page.centerPoint.x); *pReal++ = (y3 + scaleY); *pReal++ = (z3 - page.centerPoint.z);	//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 0; *pReal++ = 0;								//uv

		*pReal++ = (x4 - page.centerPoint.x); *pReal++ = (y4 + scaleY); *pReal++ = (z4 - page.centerPoint.z);	//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 1; *pReal++ = 0;								//uv

		*pReal++ = (x3 - page.centerPoint.x); *pReal++ = (y3); *pReal++ = (z3 - page.centerPoint.z);			//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 0; *pReal++ = 1;								//uv

		*pReal++ = (x4 - page.centerPoint.x); *pReal++ = (y4); *pReal++ = (z4 - page.centerPoint.z);			//pos
		*((uint32*)pReal++) = color;							//color
		*pReal++ = 1; *pReal++ = 1;								//uv

		//Update bounds
		if (y3 < minY) minY = y1;
		if (y4 < minY) minY = y2;
		if (y3 + scaleY > maxY) maxY = y3 + scaleY;
		if (y4 + scaleY > maxY) maxY = y4 + scaleY;
	}

	vbuf->unlock();
	subMesh->vertexData->vertexBufferBinding->setBinding(0, vbuf);

	//Populate index buffer
	subMesh->indexData->indexStart = 0;
	subMesh->indexData->indexCount = 6 * quadCount;
	subMesh->indexData->indexBuffer = HardwareBufferManager::getSingleton()
		.createIndexBuffer(HardwareIndexBuffer::IT_16BIT, subMesh->indexData->indexCount, HardwareBuffer::HBU_STATIC_WRITE_ONLY);
	uint16* pI = static_cast<uint16*>(subMesh->indexData->indexBuffer->lock(HardwareBuffer::HBL_DISCARD));
	for (uint16 i = 0; i < quadCount; ++i)
	{
		uint16 offset = i * 4;

		*pI++ = 0 + offset;
		*pI++ = 2 + offset;
		*pI++ = 1 + offset;

		*pI++ = 1 + offset;
		*pI++ = 2 + offset;
		*pI++ = 3 + offset;
	}

	subMesh->indexData->indexBuffer->unlock();

	//Finish up mesh
	AxisAlignedBox bounds(page.bounds.left - page.centerPoint.x, minY, page.bounds.top - page.centerPoint.z,
		page.bounds.right - page.centerPoint.x, maxY, page.bounds.bottom - page.centerPoint.z);
	mesh->_setBounds(bounds);
	Vector3 temp = bounds.getMaximum() - bounds.getMinimum();
	mesh->_setBoundingSphereRadius(temp.length() * 0.5f);

	LogManager::getSingleton().setLogDetail(static_cast<LoggingLevel>(0));
	mesh->load();
	LogManager::getSingleton().setLogDetail(LL_NORMAL);

	//Apply grass material to mesh
	subMesh->setMaterialName(layer->material->getName());

	//Return the mesh
	return mesh.getPointer();
}

Mesh *GrassLoader::generateGrass_SPRITE(PageInfo &page, GrassLayer *layer, float *grassPositions, unsigned int grassCount)
{
	//Calculate the number of quads to be added
	unsigned int quadCount;
	quadCount = grassCount;

	//Create manual mesh to store grass quads
	MeshPtr mesh = MeshManager::getSingleton().createManual(getUniqueID(), ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
	SubMesh *subMesh = mesh->createSubMesh();
	subMesh->useSharedVertices = false;

	//Setup vertex format information
	subMesh->vertexData = new VertexData;
	subMesh->vertexData->vertexStart = 0;
	subMesh->vertexData->vertexCount = 4 * quadCount;

	VertexDeclaration* dcl = subMesh->vertexData->vertexDeclaration;
	size_t offset = 0;
	dcl->addElement(0, offset, VET_FLOAT3, VES_POSITION);
	offset += VertexElement::getTypeSize(VET_FLOAT3);
	dcl->addElement(0, offset, VET_FLOAT4, VES_NORMAL);
	offset += VertexElement::getTypeSize(VET_FLOAT4);
	dcl->addElement(0, offset, VET_COLOUR, VES_DIFFUSE);
	offset += VertexElement::getTypeSize(VET_COLOUR);
	dcl->addElement(0, offset, VET_FLOAT2, VES_TEXTURE_COORDINATES);
	offset += VertexElement::getTypeSize(VET_FLOAT2);

	//Populate a new vertex buffer with grass
	HardwareVertexBufferSharedPtr vbuf = HardwareBufferManager::getSingleton()
		.createVertexBuffer(offset, subMesh->vertexData->vertexCount, HardwareBuffer::HBU_STATIC_WRITE_ONLY, false);
	float* pReal = static_cast<float*>(vbuf->lock(HardwareBuffer::HBL_DISCARD));

	//Calculate size variance
	float rndWidth = layer->maxWidth - layer->minWidth;
	float rndHeight = layer->maxHeight - layer->minHeight;

	float minY = Math::POS_INFINITY, maxY = Math::NEG_INFINITY;
	float *posPtr = grassPositions;	//Position array "iterator"
	for (uint16 i = 0; i < grassCount; ++i)
	{
		//Get the x and z positions from the position array
		float x = *posPtr++;
		float z = *posPtr++;

		//Calculate height
		float y;
		if (heightFunction){
			y = heightFunction(x, z, heightFunctionUserData);
		} else {
			y = 0;
		}

		float x1 = (x - page.centerPoint.x);
		float z1 = (z - page.centerPoint.z);

		//Get the color at the grass position
		uint32 color;
		if (layer->colorMap)
			color = layer->colorMap->getColorAt(x, z);
		else
			color = 0xFFFFFFFF;

		//Calculate size
		float rnd = Math::UnitRandom();	//The same rnd value is used for width and height to maintain aspect ratio
		float halfXScale = (layer->minWidth + rndWidth * rnd) * 0.5f;
		float scaleY = (layer->minHeight + rndHeight * rnd);

		//Randomly mirror grass textures
		float uvLeft, uvRight;
		if (Math::UnitRandom() > 0.5f){
			uvLeft = 0;
			uvRight = 1;
		} else {
			uvLeft = 1;
			uvRight = 0;
		}

		//Add vertices
		*pReal++ = x1; *pReal++ = y; *pReal++ = z1;					//center position
		*pReal++ = -halfXScale; *pReal++ = scaleY; *pReal++ = 0; *pReal++ = 0;	//normal (used to store relative corner positions)
		*((uint32*)pReal++) = color;								//color
		*pReal++ = uvLeft; *pReal++ = 0;							//uv

		*pReal++ = x1; *pReal++ = y; *pReal++ = z1;					//center position
		*pReal++ = +halfXScale; *pReal++ = scaleY; *pReal++ = 0; *pReal++ = 0;	//normal (used to store relative corner positions)
		*((uint32*)pReal++) = color;								//color
		*pReal++ = uvRight; *pReal++ = 0;							//uv

		*pReal++ = x1; *pReal++ = y; *pReal++ = z1;					//center position
		*pReal++ = -halfXScale; *pReal++ = 0.0f; *pReal++ = 0; *pReal++ = 0;		//normal (used to store relative corner positions)
		*((uint32*)pReal++) = color;								//color
		*pReal++ = uvLeft; *pReal++ = 1;							//uv

		*pReal++ = x1; *pReal++ = y; *pReal++ = z1;					//center position
		*pReal++ = +halfXScale; *pReal++ = 0.0f; *pReal++ = 0; *pReal++ = 0;		//normal (used to store relative corner positions)
		*((uint32*)pReal++) = color;								//color
		*pReal++ = uvRight; *pReal++ = 1;							//uv

		//Update bounds
		if (y < minY) minY = y;
		if (y + scaleY > maxY) maxY = y + scaleY;
	}

	vbuf->unlock();
	subMesh->vertexData->vertexBufferBinding->setBinding(0, vbuf);

	//Populate index buffer
	subMesh->indexData->indexStart = 0;
	subMesh->indexData->indexCount = 6 * quadCount;
	subMesh->indexData->indexBuffer = HardwareBufferManager::getSingleton()
		.createIndexBuffer(HardwareIndexBuffer::IT_16BIT, subMesh->indexData->indexCount, HardwareBuffer::HBU_STATIC_WRITE_ONLY);
	uint16* pI = static_cast<uint16*>(subMesh->indexData->indexBuffer->lock(HardwareBuffer::HBL_DISCARD));
	for (uint16 i = 0; i < quadCount; ++i)
	{
		uint16 offset = i * 4;

		*pI++ = 0 + offset;
		*pI++ = 2 + offset;
		*pI++ = 1 + offset;

		*pI++ = 1 + offset;
		*pI++ = 2 + offset;
		*pI++ = 3 + offset;
	}

	subMesh->indexData->indexBuffer->unlock();

	//Finish up mesh
	AxisAlignedBox bounds(page.bounds.left - page.centerPoint.x, minY, page.bounds.top - page.centerPoint.z,
		page.bounds.right - page.centerPoint.x, maxY, page.bounds.bottom - page.centerPoint.z);
	mesh->_setBounds(bounds);
	Vector3 temp = bounds.getMaximum() - bounds.getMinimum();
	mesh->_setBoundingSphereRadius(temp.length() * 0.5f);

	LogManager::getSingleton().setLogDetail(static_cast<LoggingLevel>(0));
	mesh->load();
	LogManager::getSingleton().setLogDetail(LL_NORMAL);

	//Apply grass material to mesh
	subMesh->setMaterialName(layer->material->getName());

	//Return the mesh
	return mesh.getPointer();
}

GrassLayer::GrassLayer(PagedGeometry *geom, GrassLoader *ldr)
{
	GrassLayer::geom = geom;
	GrassLayer::parent = ldr;

	density = 1.0f;
	minWidth = 1.0f; maxWidth = 1.0f;
	minHeight = 1.0f; maxHeight = 1.0f;
	minY = 0; maxY = 0;
	renderTechnique = GRASSTECH_QUAD;
	fadeTechnique = FADETECH_ALPHA;
	animMag = 1.0f;
	animSpeed = 1.0f;
	animFreq = 1.0f;
	waveCount = 0.0f;
	animate = false;
	blend = false;
	shaderNeedsUpdate = true;

	densityMap = NULL;
	densityMapFilter = MAPFILTER_BILINEAR;
	colorMap = NULL;
	colorMapFilter = MAPFILTER_BILINEAR;
}

GrassLayer::~GrassLayer()
{
	if (densityMap)
		densityMap->unload();
	if (colorMap)
		colorMap->unload();
}

void GrassLayer::setMaterialName(const String &matName)
{
	if (material.isNull() || matName != material->getName()){
		material = MaterialManager::getSingleton().getByName(matName);
		if (material.isNull())
			OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "The specified grass material does not exist", "GrassLayer::setMaterialName()");
		shaderNeedsUpdate = true;
	}
}

void GrassLayer::setMinimumSize(float width, float height)
{
	minWidth = width;
	minHeight = height;
}

void GrassLayer::setMaximumSize(float width, float height)
{
	maxWidth = width;
	if (maxHeight != height){
		maxHeight = height;
		shaderNeedsUpdate = true;
	}
}

void GrassLayer::setRenderTechnique(GrassTechnique style, bool blendBase)
{
	if (blend != blendBase || renderTechnique != style){
		blend = blendBase;
		renderTechnique = style;
		shaderNeedsUpdate = true;
	}
}

void GrassLayer::setFadeTechnique(FadeTechnique style)
{
	if (fadeTechnique != style){
		fadeTechnique = style;
		shaderNeedsUpdate = true;
	}
}

void GrassLayer::setAnimationEnabled(bool enabled)
{
	if (animate != enabled){
		animate = enabled;
		shaderNeedsUpdate = true;
	}
}

void GrassLayer::setDensityMap(const String &mapFile, MapChannel channel)
{
	if (densityMap){
		densityMap->unload();
		densityMap = NULL;
	}
	if (mapFile != ""){
		densityMap = DensityMap::load(mapFile, channel);
		densityMap->setMapBounds(mapBounds);
		densityMap->setFilter(densityMapFilter);
	}
}
void GrassLayer::setDensityMap(Texture *map, MapChannel channel)
{
	if (densityMap){
		densityMap->unload();
		densityMap = NULL;
	}
	if (map){
		densityMap = DensityMap::load(map, channel);
		densityMap->setMapBounds(mapBounds);
		densityMap->setFilter(densityMapFilter);
	}
}

void GrassLayer::setDensityMapFilter(MapFilter filter)
{
	densityMapFilter = filter;
	if (densityMap)
		densityMap->setFilter(densityMapFilter);
}

unsigned int GrassLayer::_populateGrassList_Uniform(PageInfo page, float *posBuff, unsigned int grassCount)
{
	float *posPtr = posBuff;

	//No density map
	if (!minY && !maxY){
		//No height range
		for (unsigned int i = 0; i < grassCount; ++i){
			//Pick a random position
			float x = Math::RangeRandom(page.bounds.left, page.bounds.right);
			float z = Math::RangeRandom(page.bounds.top, page.bounds.bottom);

			//Add to list in within bounds
			if (!colorMap){
				*posPtr++ = x;
				*posPtr++ = z;
			} else if (x >= mapBounds.left && x <= mapBounds.right && z >= mapBounds.top && z <= mapBounds.bottom){
				*posPtr++ = x;
				*posPtr++ = z;
			}
		}
	} else {
		//Height range
		Real min, max;
		if (minY) min = minY; else min = Math::NEG_INFINITY;
		if (maxY) max = maxY; else max = Math::POS_INFINITY;

		for (unsigned int i = 0; i < grassCount; ++i){
			//Pick a random position
			float x = Math::RangeRandom(page.bounds.left, page.bounds.right);
			float z = Math::RangeRandom(page.bounds.top, page.bounds.bottom);

			//Calculate height
			float y = parent->heightFunction(x, z, parent->heightFunctionUserData);

			//Add to list if in range
			if (y >= min && y <= max){
				//Add to list in within bounds
				if (!colorMap){
					*posPtr++ = x;
					*posPtr++ = z;
				} else if (x >= mapBounds.left && x <= mapBounds.right && z >= mapBounds.top && z <= mapBounds.bottom){
					*posPtr++ = x;
					*posPtr++ = z;
				}
			}
		}
	}

	grassCount = (posPtr - posBuff) / 2;
	return grassCount;
}

unsigned int GrassLayer::_populateGrassList_UnfilteredDM(PageInfo page, float *posBuff, unsigned int grassCount)
{
	float *posPtr = posBuff;

	//Use density map
	if (!minY && !maxY){
		//No height range
		for (unsigned int i = 0; i < grassCount; ++i){
			//Pick a random position
			float x = Math::RangeRandom(page.bounds.left, page.bounds.right);
			float z = Math::RangeRandom(page.bounds.top, page.bounds.bottom);

			//Determine whether this grass will be added based on the local density.
			//For example, if localDensity is .32, grasses will be added 32% of the time.
			if (Math::UnitRandom() < densityMap->_getDensityAt_Unfiltered(x, z)){
				//Add to list
				*posPtr++ = x;
				*posPtr++ = z;
			}
		}
	} else {
		//Height range
		Real min, max;
		if (minY) min = minY; else min = Math::NEG_INFINITY;
		if (maxY) max = maxY; else max = Math::POS_INFINITY;

		for (unsigned int i = 0; i < grassCount; ++i){
			//Pick a random position
			float x = Math::RangeRandom(page.bounds.left, page.bounds.right);
			float z = Math::RangeRandom(page.bounds.top, page.bounds.bottom);

			//Determine whether this grass will be added based on the local density.
			//For example, if localDensity is .32, grasses will be added 32% of the time.
			if (Math::UnitRandom() < densityMap->_getDensityAt_Unfiltered(x, z)){
				//Calculate height
				float y = parent->heightFunction(x, z, parent->heightFunctionUserData);

				//Add to list if in range
				if (y >= min && y <= max){
					//Add to list
					*posPtr++ = x;
					*posPtr++ = z;
				}
			}
		}
	}

	grassCount = (posPtr - posBuff) / 2;
	return grassCount;
}

unsigned int GrassLayer::_populateGrassList_BilinearDM(PageInfo page, float *posBuff, unsigned int grassCount)
{
	float *posPtr = posBuff;

	if (!minY && !maxY){
		//No height range
		for (unsigned int i = 0; i < grassCount; ++i){
			//Pick a random position
			float x = Math::RangeRandom(page.bounds.left, page.bounds.right);
			float z = Math::RangeRandom(page.bounds.top, page.bounds.bottom);

			//Determine whether this grass will be added based on the local density.
			//For example, if localDensity is .32, grasses will be added 32% of the time.
			if (Math::UnitRandom() < densityMap->_getDensityAt_Bilinear(x, z)){
				//Add to list
				*posPtr++ = x;
				*posPtr++ = z;
			}
		}
	} else {
		//Height range
		Real min, max;
		if (minY) min = minY; else min = Math::NEG_INFINITY;
		if (maxY) max = maxY; else max = Math::POS_INFINITY;

		for (unsigned int i = 0; i < grassCount; ++i){
			//Pick a random position
			float x = Math::RangeRandom(page.bounds.left, page.bounds.right);
			float z = Math::RangeRandom(page.bounds.top, page.bounds.bottom);

			//Determine whether this grass will be added based on the local density.
			//For example, if localDensity is .32, grasses will be added 32% of the time.
			if (Math::UnitRandom() < densityMap->_getDensityAt_Bilinear(x, z)){
				//Calculate height
				float y = parent->heightFunction(x, z, parent->heightFunctionUserData);

				//Add to list if in range
				if (y >= min && y <= max){
					//Add to list
					*posPtr++ = x;
					*posPtr++ = z;
				}
			}
		}
	}

	grassCount = (posPtr - posBuff) / 2;
	return grassCount;
}

void GrassLayer::setColorMap(const String &mapFile, MapChannel channel)
{
	if (colorMap){
		colorMap->unload();
		colorMap = NULL;
	}
	if (mapFile != ""){
		colorMap = ColorMap::load(mapFile, channel);
		colorMap->setMapBounds(mapBounds);
		colorMap->setFilter(colorMapFilter);
	}
}

void GrassLayer::setColorMap(Texture *map, MapChannel channel)
{
	if (colorMap){
		colorMap->unload();
		colorMap = NULL;
	}
	if (map){
		colorMap = ColorMap::load(map, channel);
		colorMap->setMapBounds(mapBounds);
		colorMap->setFilter(colorMapFilter);
	}
}

void GrassLayer::setColorMapFilter(MapFilter filter)
{
	colorMapFilter = filter;
	if (colorMap)
		colorMap->setFilter(colorMapFilter);
}

void GrassLayer::_updateShaders()
{
	if (shaderNeedsUpdate){
		shaderNeedsUpdate = false;

		//Proceed only if there is no custom vertex shader and the user's computer supports vertex shaders
		const RenderSystemCapabilities *caps = Root::getSingleton().getRenderSystem()->getCapabilities();
		if (caps->hasCapability(RSC_VERTEX_PROGRAM)){
			//Generate a string ID that identifies the current set of vertex shader options
			StringUtil::StrStreamType tmpName;
			tmpName << "GrassVS_";
			if (animate)
				tmpName << "anim_";
			if (blend)
				tmpName << "blend_";
			tmpName << renderTechnique << "_";
			tmpName << fadeTechnique << "_";
			if (fadeTechnique == FADETECH_GROW || fadeTechnique == FADETECH_ALPHAGROW)
				tmpName << maxHeight << "_";
			tmpName << "vp";
			const String vsName = tmpName.str();
			
			//Generate a string ID that identifies the material combined with the vertex shader
			const String matName = material->getName() + "_" + vsName;

			//Check if the desired material already exists (if not, create it)
			MaterialPtr tmpMat = MaterialManager::getSingleton().getByName(matName);
			if (tmpMat.isNull()){
				//Clone the original material
				tmpMat = material->clone(matName);

				//Disable lighting
				tmpMat->setLightingEnabled(false);
				//tmpMat->setReceiveShadows(false);

				//Check if the desired shader already exists (if not, compile it)
				HighLevelGpuProgramPtr vertexShader = HighLevelGpuProgramManager::getSingleton().getByName(vsName);
				if (vertexShader.isNull()){
					//Generate the grass shader
					String vertexProgSource;
					vertexProgSource = 
						"void main( \n"
						"	float4 iPosition : POSITION, \n"
						"	float4 iColor : COLOR, \n"
						"	float2 iUV       : TEXCOORD0,	\n"
						"	out float4 oPosition : POSITION, \n"
						"	out float4 oColor : COLOR, \n"
						"	out float2 oUV       : TEXCOORD0,	\n";

					if (animate) vertexProgSource +=
						"	uniform float time,	\n"
						"	uniform float frequency,	\n"
						"	uniform float4 direction,	\n";

					if (fadeTechnique == FADETECH_GROW || fadeTechnique == FADETECH_ALPHAGROW) vertexProgSource +=
						"	uniform float grassHeight,	\n";

					if (renderTechnique == GRASSTECH_SPRITE) vertexProgSource +=
						"	float4 iNormal : NORMAL, \n";

					vertexProgSource +=
						"	uniform float4x4 worldViewProj,	\n"
						"	uniform float3 camPos, \n"
						"	uniform float fadeRange ) \n"
						"{	\n"
						"	oColor.rgb = iColor.rgb;   \n"
						"	float4 position = iPosition;	\n"
						"	float dist = distance(camPos.xz, position.xz);	\n";

					if (fadeTechnique == FADETECH_ALPHA || fadeTechnique == FADETECH_ALPHAGROW) vertexProgSource +=
						//Fade out in the distance
						"	oColor.a = 2.0f - (2.0f * dist / fadeRange);   \n";
					else vertexProgSource +=
						"	oColor.a = 1.0f;   \n";

					vertexProgSource +=
						"	float oldposx = position.x;	\n";

					if (renderTechnique == GRASSTECH_SPRITE) vertexProgSource +=
						//Face the camera
						"	float3 dirVec = (float3)position - (float3)camPos;		\n"
						"	float3 p = normalize(cross(float4(0,1,0,0), dirVec));	\n"
						"	position += float4(p.x * iNormal.x, iNormal.y, p.z * iNormal.x, 0);	\n";

					if (animate) vertexProgSource +=
						"	if (iUV.y == 0.0f){	\n"
						//Wave grass in breeze
						"		float offset = sin(time + oldposx * frequency);	\n"
						"		position += direction * offset;	\n"
						"	}	\n";

					if (blend && animate) vertexProgSource +=
						"	else {	\n";
					else if (blend) vertexProgSource +=
						"	if (iUV.y != 0.0f){	\n";

					if (blend) vertexProgSource +=
						//Blend the base of nearby grass into the terrain
						"		if (oColor.a >= 1.0f) \n"
						"			oColor.a = 4.0f * ((dist / fadeRange) - 0.1f);	\n"
						"	}	\n";

					if (fadeTechnique == FADETECH_GROW || fadeTechnique == FADETECH_ALPHAGROW) vertexProgSource +=
						"	float offset = (2.0f * dist / fadeRange) - 1.0f; \n"
						"	position.y -= grassHeight * clamp(offset, 0, 1); ";

					vertexProgSource +=
						"	oPosition = mul(worldViewProj, position);  \n";

					vertexProgSource +=
						"	oUV = iUV;\n"
						"}"; 

					vertexShader = HighLevelGpuProgramManager::getSingleton().createProgram(
						vsName,
						ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
						"cg", GPT_VERTEX_PROGRAM);

					vertexShader->setSource(vertexProgSource);
					vertexShader->setParameter("profiles", "vs_1_1 arbvp1");
					vertexShader->setParameter("entry_point", "main");
					vertexShader->load();
				}
				//Now the vertex shader (vertexShader) has either been found or just generated
				//(depending on whether or not it was already generated). 

				//Apply the shader to the material
				Pass *pass = tmpMat->getTechnique(0)->getPass(0);
				pass->setVertexProgram(vsName);
				GpuProgramParametersSharedPtr params = pass->getVertexProgramParameters();

				params->setNamedAutoConstant("worldViewProj", GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
				params->setNamedAutoConstant("camPos", GpuProgramParameters::ACT_CAMERA_POSITION_OBJECT_SPACE);
				params->setNamedAutoConstant("fadeRange", GpuProgramParameters::ACT_CUSTOM, 1);

				if (animate){
					params->setNamedAutoConstant("time", GpuProgramParameters::ACT_CUSTOM, 1);
					params->setNamedAutoConstant("frequency", GpuProgramParameters::ACT_CUSTOM, 1);
					params->setNamedAutoConstant("direction", GpuProgramParameters::ACT_CUSTOM, 4);
				}

				if (fadeTechnique == FADETECH_GROW || fadeTechnique == FADETECH_ALPHAGROW){
					params->setNamedAutoConstant("grassHeight", GpuProgramParameters::ACT_CUSTOM, 1);
					params->setNamedConstant("grassHeight", maxHeight * 1.05f);
				}

				float farViewDist = geom->getDetailLevels().front()->getFarRange();
				pass->getVertexProgramParameters()->setNamedConstant("fadeRange", farViewDist / 1.225f);
				//Note: 1.225 ~= sqrt(1.5), which is necessary since the far view distance is measured from the centers
				//of pages, while the vertex shader needs to fade grass completely out (including the closest corner)
				//before the page center is out of range.
			}
			//Now the material (tmpMat) has either been found or just created (depending on whether or not it was already
			//created). The appropriate vertex shader should be applied and the material is ready for use.

			//Apply the new material
			material = tmpMat;
		}
	}
}


unsigned long GrassPage::GUID = 0;

void GrassPage::init(PagedGeometry *geom)
{
	sceneMgr = geom->getSceneManager();
	rootNode = geom->getSceneNode();
}

GrassPage::~GrassPage()
{
	removeEntities();
}

void GrassPage::addEntity(Entity *entity, const Vector3 &position, const Quaternion &rotation, const Vector3 &scale, const Ogre::ColourValue &color)
{
	SceneNode *node = rootNode->createChildSceneNode();
	node->setPosition(position);
	nodeList.push_back(node);

	Entity *ent = entity->clone(getUniqueID());
	ent->setCastShadows(false);
	ent->setRenderQueueGroup(entity->getRenderQueueGroup());
	node->attachObject(ent);
}

void GrassPage::removeEntities()
{
	std::list<SceneNode*>::iterator i;
	for (i = nodeList.begin(); i != nodeList.end(); ++i){
		SceneNode *node = *i;
		sceneMgr->destroyEntity(static_cast<Entity*>(node->getAttachedObject(0)));
		sceneMgr->destroySceneNode(node->getName());
	}
	nodeList.clear();
}

void GrassPage::setVisible(bool visible)
{
	std::list<SceneNode*>::iterator i;
	for (i = nodeList.begin(); i != nodeList.end(); ++i){
		SceneNode *node = *i;
		node->setVisible(visible);
	}
}


}
