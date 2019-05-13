/*	Havok Plugin for Noesis
	Copyright(C) 2019 Lukas Cone

	This program is free software : you can redistribute it and / or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.If not, see <https://www.gnu.org/licenses/>.

	Havok Tool uses HavokLib 2016-2019 Lukas Cone
*/

#pragma pack(1)
#include "pluginshare.h"
#pragma pack()

#include "datas/esstring.h"
#include "datas/masterprinter.hpp"

#include "HavokApi.hpp"
#include "HavokXMLApi.hpp"

const char *g_pPluginName = "HavokNoesis";
const char *g_pPluginDesc = "Havok Noesis plugin by Lukas Cone, 2019";

#define hkMagic1	0x57e0e057
#define hkMagic2	0x10c0c010
#define hkHederTAG	0x30474154

static struct
{
	hkXMLToolsets *toolsetVersion;

	static bool ToolsetVersion(const char *arg, uchar *store, int storeSize)
	{
		hkXMLToolsets &_toolsetVersion = *reinterpret_cast<hkXMLToolsets*>(store);

		switch (*reinterpret_cast<const int*>(arg))
		{
		case CompileFourCC("550\0"):
			_toolsetVersion = HK550;
			break;

		case CompileFourCC("660\0"):
			_toolsetVersion = HK660;
			break;

		case CompileFourCC("710\0"):
			_toolsetVersion = HK710;
			break;

		case CompileFourCC("2010"):
			_toolsetVersion = HK2010_2;
			break;

		case CompileFourCC("2011"):
			_toolsetVersion = HK2011;
			break;

		case CompileFourCC("2012"):
			_toolsetVersion = HK2012_2;
			break;

		case CompileFourCC("2013"):
			_toolsetVersion = HK2013;
			break;

		case CompileFourCC("2014"):
			_toolsetVersion = HK2014;
			break;

		default:
			_toolsetVersion = HKUNKVER;
			break;
		}

		return true;
	}

	static void ToolsetVersion(uchar *store, int storeSize)
	{
		*store = 1;
	}

	void Register(int fHandle)
	{
		addOptParms_t optParms = {};
		optParms.optName = "-toolset";
		optParms.optDescr = "<arg>: 550, 660, 710, 2010, 2011, 2012, 2013, 2014";
		optParms.storeSize = 4;
		optParms.shareStore = reinterpret_cast<uchar *>(toolsetVersion);
		optParms.handler = ToolsetVersion;
		optParms.storeReset = ToolsetVersion;
		optParms.flags = OPTFLAG_WANTARG;
		HKSettings.toolsetVersion = static_cast<hkXMLToolsets *>(g_nfn->NPAPI_AddTypeOption(fHandle, &optParms));
	}

}HKSettings;

bool HavokCheck(BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi)
{
	typedef struct
	{
		int mag01;
		int mag02;
	}check_s;

	check_s *check = reinterpret_cast<check_s *>(fileBuffer);

	if ((check->mag01 == hkMagic1 && check->mag02 == hkMagic2) || check->mag02 == hkHederTAG)
		return true;

	return false;
}

modelBone_t *LoadSkeletons(const hkaAnimationContainer *cont, noeRAPI_t *rapi, int &numBones)
{
	int totalNumBones = 0;

	for (auto &s : cont->Skeletons())
		totalNumBones += s.GetNumBones();

	if (!totalNumBones)
		return nullptr;

	modelBone_t *bones = rapi->Noesis_AllocBones(totalNumBones);
	numBones = totalNumBones;
	totalNumBones = 0;
	int lastSkeletonBoneIndex = 0;

	for (auto &s : cont->Skeletons())
	{
		for (auto b : s.FullBones())
		{
			modelBone_t *cBone = bones + totalNumBones;

			strcpy_s(cBone->name, b.name);
			cBone->mat = g_identityMatrix;

			RichQuat cquat = reinterpret_cast<const RichQuat &>(b.transform->rotation);
			cBone->mat = cquat.ToMat43(true).m;
			memcpy_s(cBone->mat.o, sizeof(Vector), &b.transform->position, sizeof(Vector));

			if (b.parentID > -1)
			{
				cBone->eData.parent = bones + lastSkeletonBoneIndex + b.parentID;
				RichMat43 cMat(cBone->mat);
				RichMat43 pMat(cBone->eData.parent->mat);
				cBone->mat = (cMat * pMat).m;
			}

			totalNumBones++;
		}

		lastSkeletonBoneIndex += totalNumBones;
	}

	return bones;
}

noesisModel_t *HavokLoad(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi)
{
	const wchar_t *fileName = rapi->Noesis_GetLastCheckedNameW();
	IhkPackFile *pFile = IhkPackFile::Create(fileName);

	if (!pFile)
		return nullptr;

	const hkRootLevelContainer *rootCont = pFile->GetRootLevelContainer();
	void *rapiContext = rapi->rpgCreateContext();
	noesisAnim_t *anim = nullptr;

	for (auto &v : *rootCont)
		if (v == hkaAnimationContainer::HASH)
		{
			int numBones;
			modelBone_t *bones = LoadSkeletons(v, rapi, numBones);
			
			if (bones)
			{
				std::vector<modelMatrix_t> aniMats(numBones);

				for (int b = 0; b < numBones; b++)
				{
					if (bones[b].eData.parent)
					{
						RichMat43 cMat = bones[b].mat;
						RichMat43 pMat = bones[b].eData.parent->mat;
						pMat.Inverse();
						aniMats[b] = (cMat * pMat).m;
					}
					else
						aniMats[b] = bones[b].mat;
				}

				anim = rapi->rpgAnimFromBonesAndMatsFinish(bones, numBones, aniMats.data(), 1, 1.f);
			}

			break;
		}

	delete pFile;

	noesisModel_t *rModel = rapi->Noesis_AllocModelContainer(nullptr, anim, 1);

	if (rModel)
	{
		static float corRotated[3] = { 0.0f, -90.0f, 0.0f };
		rapi->SetPreviewAngOfs(corRotated);
	}

	rapi->rpgDestroyContext(rapiContext);
	numMdl = 1;
	return rModel;
}


struct xmlBoneNoe : xmlBone
{
	void *ref;
};

void HavokWrite(noesisAnim_t *ani, noeRAPI_t *rapi)
{
	xmlHavokFile hkFile = {};
	xmlRootLevelContainer *cont = hkFile.NewClass<xmlRootLevelContainer>();
	xmlAnimationContainer *aniCont = hkFile.NewClass<xmlAnimationContainer>();
	xmlSkeleton *skel = hkFile.NewClass<xmlSkeleton>();
	skel->name = "Reference";
	cont->AddVariant(aniCont);
	aniCont->skeletons.push_back(skel);

	for (int b = 0; b < ani->numBones; b++)
	{
		modelBone_t *cBone = ani->bones + b;

		xmlBoneNoe *currentNode = new xmlBoneNoe();
		currentNode->ID = cBone->index;
		currentNode->name = cBone->name;
		currentNode->ref = cBone;

		if (cBone->eData.parent)
			for (auto &b : skel->bones)
				if (static_cast<xmlBoneNoe *>(b)->ref == cBone->eData.parent)
				{
					currentNode->parent = b;
					break;
				}
		RichMat43 locMat = cBone->mat;

		if (currentNode->parent)
		{
			RichMat43 pMat = cBone->eData.parent->mat;
			pMat.Inverse();
			locMat *= pMat;
		}

		currentNode->transform.position = *reinterpret_cast<Vector*>(locMat.m.o);
		RichQuat rot = locMat.ToQuat().GetTranspose();
		currentNode->transform.rotation = reinterpret_cast<Vector4&>(rot);
		skel->bones.push_back(currentNode);
	}

	hkFile.ExportXML(esStringConvert<wchar_t>(ani->filename).c_str(), *HKSettings.toolsetVersion);
}

void PrintLog(TCHAR *msg)
{
	g_nfn->NPAPI_PopupDebugLog(0);
	g_nfn->NPAPI_DebugLogStr(const_cast<char *>(esStringConvert<char>(msg).c_str()));
}

bool NPAPI_InitLocal(void)
{
	int fHandle = g_nfn->NPAPI_Register("Havok", ".hkx;.hkt;.bsk");

	if (fHandle < 0)
		return false;

	g_nfn->NPAPI_SetTypeHandler_TypeCheck(fHandle, HavokCheck);
	g_nfn->NPAPI_SetTypeHandler_LoadModel(fHandle, HavokLoad);
	g_nfn->NPAPI_SetTypeHandler_WriteAnim(fHandle, HavokWrite);
	HKSettings.Register(fHandle);
	

	printer.AddPrinterFunction(PrintLog);

	return true;
}

void NPAPI_ShutdownLocal(void) {}