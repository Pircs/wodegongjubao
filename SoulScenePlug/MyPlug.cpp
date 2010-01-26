#include "MyPlug.h"
#include "IORead.h"
#include "FileSystem.h"
#include "LumpFile.h"
#include "3DMapSceneObj.h"

CMyPlug::CMyPlug(void)
{

}

CMyPlug::~CMyPlug(void)
{

}

#define MAP_FILE_SIZE 65536*3+2
#define HEIGHT_HEAD_SIZE 1082
#define HEIGHT_BUFFER_SIZE 65536
#define ATT_FILE_SIZE_1 65536*2+4
#define ATT_FILE_SIZE_2 65536*1+4

inline void decrypt2(char* buffer, size_t size)
{
	const char xorKeys[] = {0xFC, 0xCF, 0xAB};
	for (size_t i=0; i<size; ++i)
	{
		*buffer ^= xorKeys[i%3];
		buffer++;
	}
}

inline void decrypt(char* buffer, size_t size)
{
	const char xorKeys[] = {
		0xd1, 0x73, 0x52, 0xf6,
		0xd2, 0x9a, 0xcb, 0x27,
		0x3e, 0xaf, 0x59, 0x31,
		0x37, 0xb3, 0xe7, 0xa2
	};
	char key = 0x5E;
	for (size_t i=0; i<size; ++i)
	{
		char encode = *buffer;
		*buffer ^= xorKeys[i%16];
		*buffer -= key;
		key = encode+0x3D;
		buffer++;
	}
}

inline void encrypt(char* buffer, size_t size)
{
	const char xorKeys[] = {
		0xd1, 0x73, 0x52, 0xf6,
		0xd2, 0x9a, 0xcb, 0x27,
		0x3e, 0xaf, 0x59, 0x31,
		0x37, 0xb3, 0xe7, 0xa2
	};
	char key = 0x5E;
	for (size_t i=0; i<size; ++i)
	{
		*buffer += key;
		*buffer ^= xorKeys[i%16];
		key = *buffer+0x3D;
		buffer++;
	}
}

int CMyPlug::Execute(iScene * pScene, bool bShowDlg, bool bSpecifyFileName)
{
	return -1;
}

bool CMyPlug::importTerrainData(iTerrainData * pTerrainData, const std::string& strFilename)
{
	pTerrainData->clear();
	CLumpFile lump;
	if (lump.LoadFile(strFilename))
	{
		int nWidth, nHeight, nCubeSize;
		lump.GetInt("width", nWidth);
		lump.GetInt("height", nHeight);
		lump.GetInt("cubesize", nCubeSize);
		if (pTerrainData->resize(nWidth,nHeight,nCubeSize))
		{
			const CLumpNode* pCellsNode = lump.firstChild("cells");
			if (pCellsNode)
			{
				pCellsNode->getVector("tile0",		pTerrainData->getTiles(0));
				pCellsNode->getVector("tile1",		pTerrainData->getTiles(1));
				pCellsNode->getVector("color",		pTerrainData->getColors());
				pCellsNode->getVector("height",		pTerrainData->getHeights());
				pCellsNode->getVector("attribute",	pTerrainData->getAttributes());
				pCellsNode->getVector("other",		pTerrainData->getOthers());
				//pCellsNode->getVector("uv",		pTerrainData->getEquableTexUV);
			}
		}
	}
	return true;
}
#include "CsvFile.h"
bool CMyPlug::importTiles(iTerrain * pTerrain, const std::string& strFilename, const std::string& strPath)
{
	pTerrain->clearAllTiles();
	CCsvFile csv;
	if (csv.Open(strFilename))
	{
		while (csv.SeekNextLine())
		{
			pTerrain->setTile(csv.GetInt("ID"), csv.GetStr("Name"),
				getRealFilename(strPath,csv.GetStr("Diffuse")),
				getRealFilename(strPath,csv.GetStr("Emissive")),
				getRealFilename(strPath,csv.GetStr("Specular")),
				getRealFilename(strPath,csv.GetStr("Normal")),
				getRealFilename(strPath,csv.GetStr("Environment")),
				csv.GetStr("Effect"),
				csv.GetInt("Channel"),
				csv.GetBool("AlphaBlend"),
				csv.GetBool("AlphaTest"),
				1.0f/(float)csv.GetInt("usize"),
				1.0f/(float)csv.GetInt("vsize"));
		}
		csv.Close();
	}
	return true;
}

bool CMyPlug::importObjectResources(iScene * pScene, const std::string& strFilename, const std::string& strPath)
{
	pScene->clearObjectResources();
	CCsvFile csvObject;
	if (csvObject.Open(strFilename))
	{
		while (csvObject.SeekNextLine())
		{
			pScene->setObjectResources(
				csvObject.GetInt("ID"),
				csvObject.GetStr("Name"),
				getRealFilename(strPath,csvObject.GetStr("Filename")));
				//Info.bbox				= 
				//Info.bIsGround			= csvObject.GetBool("IsGround");
				//Info.bHasShadow			= csvObject.GetBool("HasShadow");
				//Info.strFilename	= csvObject.GetStr("ModelFilename");
		}
		csvObject.Close();
	}
	return true;
}

#pragma pack(push,1)
struct ObjInfo
{
	int16 id;
	Vec3D p;
	Vec3D rotate;
	float fScale;
};
#pragma pack(pop)

bool CMyPlug::importObject(iScene * pScene, const std::string& strFilename)
{
	pScene->removeAllObjects();
	IOReadBase* pRead = IOReadBase::autoOpen(strFilename);
	if (pRead)
	{
		size_t fileSize = pRead->GetSize();
		char* buffer = new char[fileSize];
		pRead->Read(buffer, fileSize);
		decrypt(buffer,fileSize);

		int m_uUnknow = *((uint16*)(buffer));
		uint16 uObjCount = *((uint16*)(buffer+2));
		ObjInfo* pObjInfo = (ObjInfo*)(buffer+4);
		for (int i=0; i<uObjCount;++i)
		{
			Vec3D vPos = Vec3D(pObjInfo->p.x,pObjInfo->p.z,pObjInfo->p.y)*0.01f;
			Vec3D vRotate = Vec3D(pObjInfo->rotate.x,pObjInfo->rotate.z,pObjInfo->rotate.y)*PI/180.0f;

			if (false==pScene->add3DMapSceneObj(pObjInfo->id,vPos,vRotate,pObjInfo->fScale))
			{
				//MessageBoxA(NULL,"cannot find ID!","Error",0);
			}
			pObjInfo++;
		}
		delete buffer;
		IOReadBase::autoClose(pRead);
	}
	return true;
}

int CMyPlug::importData(iScene * pScene, const std::string& strFilename)
{
	{
		CLumpFile lumpFile;
		if (lumpFile.LoadFile(ChangeExtension(strFilename,".sce")))
		{
			Fog fog;
			DirectionalLight light;
			lumpFile.GetVal("fog",fog);
			lumpFile.GetVal("light",light);
			pScene->setFog(fog);
			pScene->setLight(light);
		}
	}
	importTerrainData(&pScene->getTerrain()->GetData(),ChangeExtension(strFilename,".map"));
	importTiles(pScene->getTerrain(),GetParentPath(strFilename)+"Tile.csv",GetParentPath(strFilename));
	//pScene->getTerrain()->setLightMapTexture(strFilename+"TerrainLight.OZJ");
	pScene->getTerrain()->setGrassTexture(GetParentPath(strFilename)+"TileGrass01.OZT");
	pScene->getTerrain()->setGrassShader("data\\fx\\TerrainGrass.fx");
	pScene->getTerrain()->create();

	importObjectResources(pScene,GetParentPath(strFilename)+"object.csv",GetParentPath(strFilename)); 
	BBox bboxObject;
	bboxObject.vMin = Vec3D(-20.0f,-100.0f,-20.0f);
	bboxObject.vMax = Vec3D(pScene->getTerrain()->GetData().GetWidth()+20.0f,100.0f,pScene->getTerrain()->GetData().GetHeight()+20.0f);
	pScene->createObjectTree(bboxObject,16);
	importObject(pScene,ChangeExtension(strFilename,".obj"));
	return true;
}

bool CMyPlug::exportTerrainData(iTerrainData * pTerrainData, const std::string& strFilename)
{
	CLumpFile lump;
	lump.SetName("terrain");
	lump.SetInt(1);
	lump.SetInt("width", pTerrainData->GetWidth());
	lump.SetInt("height", pTerrainData->GetHeight());
	lump.SetInt("cubesize", pTerrainData->GetCubeSize());
	CLumpNode* pCellsNode = lump.AddNode("cells");
	if (pCellsNode)
	{
		pCellsNode->SetVector("tile0",		pTerrainData->getTiles(0));
		pCellsNode->SetVector("tile1",		pTerrainData->getTiles(1));
		pCellsNode->SetVector("color",		pTerrainData->getColors());
		pCellsNode->SetVector("height",		pTerrainData->getHeights());
		pCellsNode->SetVector("attribute",	pTerrainData->getAttributes());
		pCellsNode->SetVector("other",		pTerrainData->getOthers());
		//pCellsNode->SetVector("uv",		pTerrainData->getEquableTexUV);
	}
	lump.SaveFile(strFilename);
	return true;
}

bool CMyPlug::exportTiles(iTerrain * pTerrain, const std::string& strFilename, const std::string& strPath)
{
	return true;
}

bool CMyPlug::exportObjectResources(iScene * pScene, const std::string& strFilename, const std::string& strPath)
{
	return true;
}

bool CMyPlug::exportObjectResourcesFormDir(iScene * pScene,const std::string& strPath)
{
	return true;
}

bool CMyPlug::exportObject(iScene * pScene, const std::string& strFilename)
{
	FILE* f=fopen(strFilename.c_str(),"wb");
	if (f)
	{
		std::vector<ObjInfo> setObjInfo;
		DEQUE_MAPOBJ setObject;
		pScene->getAllObjects(setObject);
		for (DEQUE_MAPOBJ::iterator it=setObject.begin();it!=setObject.end();it++)
		{
			ObjInfo objInfo;
			C3DMapSceneObj* pObj = (C3DMapSceneObj*)(*it);
			Vec3D vPos = pObj->getPos();
			vPos = Vec3D(vPos.x,vPos.z,vPos.y)*100.0f;
			Vec3D vRotate = pObj->getRotate();
			vRotate = Vec3D(vRotate.x,vRotate.z,vRotate.y)*180.0f/PI;

			objInfo.id = pObj->getObjectID();
			objInfo.p = vPos;
			objInfo.rotate = vRotate;
			objInfo.fScale = pObj->getScale();
			setObjInfo.push_back(objInfo);
		}
		size_t fileSize = setObjInfo.size()*sizeof(ObjInfo)+4;
		char* buffer = new char[fileSize];
		*((uint16*)buffer) = 1;//m_uUnknow;
		*((uint16*)(buffer+2)) = setObjInfo.size();
		if (setObjInfo.size()>0)
		{
			memcpy(buffer+4,&setObjInfo[0],setObjInfo.size()*sizeof(ObjInfo));
		}
		encrypt(buffer,fileSize);
		fwrite(buffer,fileSize,1,f);
		fclose(f);
		delete buffer;
	}
	return true;
}

int CMyPlug::exportData(iScene * pScene, const std::string& strFilename)
{
	{
		CLumpFile lumpFile;
		lumpFile.SetName("scene");
		lumpFile.SetVal("fog",pScene->getFog());
		lumpFile.SetVal("light",pScene->getLight());
		lumpFile.SaveFile(ChangeExtension(strFilename,".sce"));
	}
	exportTerrainData(&pScene->getTerrain()->GetData(),ChangeExtension(strFilename,".map"));
	exportObject(pScene,ChangeExtension(strFilename,".obj"));
	return true;
}

void CMyPlug::Release()
{
	delete this;
}