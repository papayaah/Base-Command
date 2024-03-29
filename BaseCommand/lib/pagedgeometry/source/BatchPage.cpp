/*-------------------------------------------------------------------------------------
Copyright (c) 2006 John Judnich

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
    1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
-------------------------------------------------------------------------------------*/

//BatchPage.cpp
//BatchPage is an extension to PagedGeometry which displays entities as static geometry.
//-------------------------------------------------------------------------------------

#include "BatchPage.h"
#include "BatchedGeometry.h"

#include <OgreRoot.h>
#include <OgreCamera.h>
#include <OgreVector3.h>
#include <OgreQuaternion.h>
#include <OgreEntity.h>
#include <OgreRenderSystem.h>
#include <OgreRenderSystemCapabilities.h>
#include <OgreHighLevelGpuProgram.h>
#include <OgreHighLevelGpuProgramManager.h>
using namespace Ogre;

namespace Forests {

//-------------------------------------------------------------------------------------


unsigned long BatchPage::refCount = 0;
unsigned long BatchPage::GUID = 0;

void BatchPage::init(PagedGeometry *geom)
{
	sceneMgr = geom->getSceneManager();
	batch = new BatchedGeometry(sceneMgr, geom->getSceneNode());

	fadeEnabled = false;

	const RenderSystemCapabilities *caps = Root::getSingleton().getRenderSystem()->getCapabilities();
	if (caps->hasCapability(RSC_VERTEX_PROGRAM))
		shadersSupported = true;
	else
		shadersSupported = false;

	++refCount;
}

BatchPage::~BatchPage()
{
	delete batch;

	//Delete unfaded material references
	unfadedMaterials.clear();
}


void BatchPage::addEntity(Entity *ent, const Vector3 &position, const Quaternion &rotation, const Vector3 &scale, const Ogre::ColourValue &color)
{
	batch->addEntity(ent, position, rotation, scale, color);
}

void BatchPage::build()
{
	batch->build();

	BatchedGeometry::SubBatchIterator it = batch->getSubBatchIterator();
	while (it.hasMoreElements()){
		BatchedGeometry::SubBatch *subBatch = it.getNext();
		MaterialPtr mat = subBatch->getMaterial();

		//Disable specular unless a custom shader is being used.
		//This is done because the default shader applied by BatchPage
		//doesn't support specular, and fixed-function needs to look
		//the same as the shader (for computers with no shader support)
		for (int t = 0; t < mat->getNumTechniques(); ++t){
			Technique *tech = mat->getTechnique(t);
			for (int p = 0; p < tech->getNumPasses(); ++p){
				Pass *pass = tech->getPass(p);
				if (pass->getVertexProgramName() == "")
					pass->setSpecular(0, 0, 0, 1);
			}
		}

		//Store the original materials
		unfadedMaterials.push_back(subBatch->getMaterial());
	}

	_updateShaders();
}

void BatchPage::removeEntities()
{
	batch->clear();

	unfadedMaterials.clear();
	fadeEnabled = false;
}


void BatchPage::setVisible(bool visible)
{
	batch->setVisible(visible);
}

void BatchPage::setFade(bool enabled, Real visibleDist, Real invisibleDist)
{
	if (!shadersSupported)
		return;

	//If fade status has changed...
	if (fadeEnabled != enabled){
		fadeEnabled = enabled;

// 		if (enabled) {
			//Transparent batches should render after impostors
			batch->setRenderQueueGroup(RENDER_QUEUE_6);
// 		} else {
// 			//Opaque batches should render in the normal render queue
// 			batch->setRenderQueueGroup(RENDER_QUEUE_MAIN);
// 		}

		this->visibleDist = visibleDist;
		this->invisibleDist = invisibleDist;
		_updateShaders();
	}
}

void BatchPage::_updateShaders()
{
	if (!shadersSupported)
		return;

	uint32 i = 0;
	BatchedGeometry::SubBatchIterator it = batch->getSubBatchIterator();
	while (it.hasMoreElements()){
		BatchedGeometry::SubBatch *subBatch = it.getNext();
		MaterialPtr mat = unfadedMaterials[i++];

		//Check if lighting should be enabled
		bool lightingEnabled = false;
		for (unsigned short t = 0; t < mat->getNumTechniques(); ++t){
			Technique *tech = mat->getTechnique(t);
			for (unsigned short p = 0; p < tech->getNumPasses(); ++p){
				Pass *pass = tech->getPass(p);
				if (pass->getLightingEnabled()) {
					lightingEnabled = true;
					break;
				}
			}
			if (lightingEnabled)
				break;
		}

		//Compile the CG shader script based on various material / fade options
		StringUtil::StrStreamType tmpName;
		tmpName << "BatchPage_";
		if (fadeEnabled)
			tmpName << "fade_";
		if (lightingEnabled)
			tmpName << "lit_";
		tmpName << "vp";

		const String vertexProgName = tmpName.str();

		//If the shader hasn't been created yet, create it
		if (HighLevelGpuProgramManager::getSingleton().getByName(vertexProgName).isNull()){
			String vertexProgSource =
				"void main( \n"
				"	float4 iPosition : POSITION, \n"
				"	float3 normal    : NORMAL,	\n"
				"	float2 iUV       : TEXCOORD0,	\n"
				"	float4 iColor    : COLOR, \n"

				"	out float4 oPosition : POSITION, \n"
				"	out float2 oUV       : TEXCOORD0,	\n"
				"	out float4 oColor : COLOR, \n"
				"	out float4 oFog : FOG,	\n";

			if (lightingEnabled) vertexProgSource +=
				"	uniform float4 objSpaceLight,	\n"
				"	uniform float4 lightDiffuse,	\n"
				"	uniform float4 lightAmbient,	\n";

			if (fadeEnabled) vertexProgSource +=
				"	uniform float3 camPos, \n";

			vertexProgSource +=
				"	uniform float4x4 worldViewProj,	\n"
				"	uniform float fadeGap, \n"
				"   uniform float invisibleDist )\n"
				"{	\n";

			if (lightingEnabled) vertexProgSource +=
				//Perform lighting calculations (no specular)
				"	float3 light = normalize(objSpaceLight.xyz - (iPosition.xyz * objSpaceLight.w)); \n"
				"	float diffuseFactor = max(dot(normal, light), 0); \n"
				"	oColor = (lightAmbient + diffuseFactor * lightDiffuse) * iColor; \n";
			else vertexProgSource +=
				"	oColor = iColor; \n";

			if (fadeEnabled) vertexProgSource +=
				//Fade out in the distance
				"	float dist = distance(camPos.xz, iPosition.xz);	\n"
				"	oColor.a *= (invisibleDist - dist) / fadeGap;   \n";

			vertexProgSource +=
				"	oUV = iUV;	\n"
				"	oPosition = mul(worldViewProj, iPosition);  \n"
				"	oFog.x = oPosition.z; \n"
				"}"; 

			HighLevelGpuProgramPtr vertexShader = HighLevelGpuProgramManager::getSingleton().createProgram(
				vertexProgName,
				ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				"cg", GPT_VERTEX_PROGRAM);

			vertexShader->setSource(vertexProgSource);
			vertexShader->setParameter("profiles", "vs_1_1 arbvp1");
			vertexShader->setParameter("entry_point", "main");
			vertexShader->load();
		}

		//Now that the shader is ready to be applied, apply it
		StringUtil::StrStreamType materialSignature;
		materialSignature << "BatchMat|";
		materialSignature << mat->getName() << "|";
		if (fadeEnabled){
			materialSignature << visibleDist << "|";
			materialSignature << invisibleDist << "|";
		}

		//Search for the desired material
		MaterialPtr generatedMaterial = MaterialManager::getSingleton().getByName(materialSignature.str());
		if (generatedMaterial.isNull()){
			//Clone the material
			generatedMaterial = mat->clone(materialSignature.str());

			//And apply the fade shader
			for (unsigned short t = 0; t < generatedMaterial->getNumTechniques(); ++t){
				Technique *tech = generatedMaterial->getTechnique(t);
				for (unsigned short p = 0; p < tech->getNumPasses(); ++p){
					Pass *pass = tech->getPass(p);

					//Setup vertex program
					if (pass->getVertexProgramName() == "")
						pass->setVertexProgram(vertexProgName);

					try{
						GpuProgramParametersSharedPtr params = pass->getVertexProgramParameters();

						if (lightingEnabled) {
							params->setNamedAutoConstant("objSpaceLight", GpuProgramParameters::ACT_LIGHT_POSITION_OBJECT_SPACE);
							params->setNamedAutoConstant("lightDiffuse", GpuProgramParameters::ACT_DERIVED_LIGHT_DIFFUSE_COLOUR);
							params->setNamedAutoConstant("lightAmbient", GpuProgramParameters::ACT_DERIVED_AMBIENT_LIGHT_COLOUR);
							//params->setNamedAutoConstant("matAmbient", GpuProgramParameters::ACT_SURFACE_AMBIENT_COLOUR);
						}

						params->setNamedAutoConstant("worldViewProj", GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);

						if (fadeEnabled){
							params->setNamedAutoConstant("camPos", GpuProgramParameters::ACT_CAMERA_POSITION_OBJECT_SPACE);

							//Set fade ranges
							params->setNamedAutoConstant("invisibleDist", GpuProgramParameters::ACT_CUSTOM);
							params->setNamedConstant("invisibleDist", invisibleDist);

							params->setNamedAutoConstant("fadeGap", GpuProgramParameters::ACT_CUSTOM);
							params->setNamedConstant("fadeGap", invisibleDist - visibleDist);

							if (pass->getAlphaRejectFunction() == CMPF_ALWAYS_PASS)
								pass->setSceneBlending(SBT_TRANSPARENT_ALPHA);
						}
					}
					catch (...) {
						OGRE_EXCEPT(Exception::ERR_INTERNAL_ERROR, "Error configuring batched geometry transitions. If you're using materials with custom vertex shaders, they will need to implement fade transitions to be compatible with BatchPage.", "BatchPage::_updateShaders()");
					}
				}
			}

		}

		//Apply the material
		subBatch->setMaterial(generatedMaterial);
	}

}
}
