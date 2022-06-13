#include "IniReader.h"
#include "injector/injector.hpp"
#include <vector>
#include "structs.h"
#include <string>

const char* VERSION = "NFSC - FE Loader 2.1";

int* GameState = (int*)0x00A99BBC;
float* WorldLighting = (float*)0x00A6C218;
float* CarZ = (float*)0x009F9AE8;
const float DefaultCarZ = 0.275f;
float* ShadowZ = (float*)0x007BE7E2;
const float DefaultShadowZ = -0.25;

Position Positions[10];
VectorW CustomPlatformSettings;
std::string CustomPlatformPath;

void ToggleReflections(bool enabled)
{
	injector::WriteMemory<BYTE>(0x0072E603, enabled ? 0x74 : 0xEB);
}

auto Game_GetTrackPositionMarker = (TrackPositionMarker * (__cdecl*)(const char* a1, int a2))0x0079DB20;
auto Game_StringHash = (int(__cdecl*)(const char*))0x471050;
auto Game_eViewPlatInterface_Render = (int(__thiscall*)(void*, void*, RotPosMatrix*, RotPosMatrix*, int, int, int))0x00729320;
auto Game_eModel_Init = (void(__thiscall*)(void*, int))0x0055E7E0;
auto Game_LoadResourceFile = (void(__cdecl*)(const char* path, int type, int, int, int, int, int))0x006B5980;
auto Game_FindResourceFile = (int* (__cdecl*)(const char* path))0x006A8570;
auto Game_sub_79AFA0 = (int(__thiscall*)(int, int, int))0x0079AFA0;

bool CustomRotation;
TrackPositionMarker marker;
TrackPositionMarker* __cdecl GetTrackPositionMarker(const char* position, int a2)
{
	for (auto& pos : Positions)
	{
		if (pos.Presset != -1 && strcmp(pos.Name, position) == 0)
		{
			marker.X = pos.X;
			marker.Y = pos.Y;
			marker.Z = pos.Z;
			marker.Rotation = pos.Rotation;

			CustomRotation = pos.Rotation >= 0;
			ToggleReflections(pos.Reflection);
			*WorldLighting = pos.WorldLighting;
			*CarZ = DefaultCarZ - pos.CarZ;
			*ShadowZ = DefaultShadowZ + pos.CarZ;

			return &marker;
		}
	}

	return Game_GetTrackPositionMarker(position, a2);
}

void InitGamePositions()
{
	Positions[0].IniSection = (char*)"POS_CARLOT_MAZDA";
	Positions[0].Name = (char*)"CarPosition_CarLot_Mazda";

	Positions[1].IniSection = (char*)"POS_CARLOT_EXOTIC";
	Positions[1].Name = (char*)"CarPosition_CarLot_Exotic";

	Positions[2].IniSection = (char*)"POS_CARLOT_TUNER";
	Positions[2].Name = (char*)"CarPosition_CarLot_Tuner";

	Positions[3].IniSection = (char*)"POS_CARLOT_MUSCLE";
	Positions[3].Name = (char*)"CarPosition_CarLot_Muscle";

	Positions[4].IniSection = (char*)"POS_CARCLASS";
	Positions[4].Name = (char*)"CarPosition_CarClass";

	Positions[5].IniSection = (char*)"POS_EXOTIC";
	Positions[5].Name = (char*)"CarPosition_Exotic";

	Positions[6].IniSection = (char*)"POS_TUNER";
	Positions[6].Name = (char*)"CarPosition_Tuner";

	Positions[7].IniSection = (char*)"POS_MUSCLE";
	Positions[7].Name = (char*)"CarPosition_Muscle";

	Positions[8].IniSection = (char*)"POS_MAIN";
	Positions[8].Name = (char*)"CarPosition_Main";

	Positions[9].IniSection = (char*)"POS_GENERIC";
	Positions[9].Name = (char*)"CarPosition_Generic";
}

void __declspec(naked) CarRotationCave2()
{
	static constexpr auto cExit = 0x0086A18D;
	__asm
	{
		cmp CustomRotation, 1;
		jne Original2;
		jmp cExit;

	Original2:
		add[edi + 0x20], ax;
		xor edi, edi;
		jmp cExit;
	}
}

void __declspec(naked) CarRotationCave1()
{
	static constexpr auto cExit = 0x0086A196;
	__asm
	{
		cmp CustomRotation, 1;
		jne Original1;
		jmp cExit;

	Original1:
		mov eax, [esi + 0x48];
		mov[eax + 0x20], di;
		jmp cExit;
	}
}

std::vector<void*> GarageParts;
bool GarageInit = false;
RotPosMatrix m;
RotPosMatrix light;

struct PartEntry
{
	int hash;
	int* ptr;
};

void InitCustomGarage()
{
	if (!GarageInit)
	{
		int* recource = Game_FindResourceFile(CustomPlatformPath.c_str());
		if (recource)
		{
			int* geometry = (int*)recource[0xF];
			int src = 0;
			while (geometry[src] != 0x134003) src++;

			int size = geometry[src + 1] / 8;
			auto parts = (PartEntry*)(geometry + src + 2);

			for (int i = 0; i < size; i++)
			{
				auto part = parts[i];
				char* name = (char*)part.ptr + 0xA0;

				int* model = new int[6];
				Game_eModel_Init(model, Game_StringHash(name));
				GarageParts.push_back(model);
			}

			m.x.x = 1.0f;
			m.x.y = 0.0f;
			m.x.z = 0.0f;
			m.x.w = 0.0f;

			m.y.x = 0.0f;
			m.y.y = 1.0f;
			m.y.z = 0.0f;
			m.y.w = 0.0f;

			m.z.x = 0.0f;
			m.z.y = 0.0f;
			m.z.z = 1.0f;
			m.z.w = 0.0f;

			light = m;
			light.w.x = 0.0f;
			light.w.y = 0.0f;
			light.w.z = 0.0f;
			light.w.w = 0.0f;

			m.w = CustomPlatformSettings;
			GarageInit = true;
		}
	}
}

void __stdcall DrawGarage()
{
	InitCustomGarage();

	if (GarageInit)
	{
		for (auto model : GarageParts)
		{
			Game_eViewPlatInterface_Render((void*)0x00B4AF90, model, &m, &light, 0, 0, 0);
		}
	}
}

void _fastcall DrawGarageHook(int a1, int param, int a2, int a3)
{
	if (*GameState == 3)
	{
		DrawGarage();
	}

	Game_sub_79AFA0(a1, a2, a3);
}

void __stdcall GarageLoad()
{
	Game_LoadResourceFile(CustomPlatformPath.c_str(), 6, 0, 0, 0, 0, 0);
}

void __declspec(naked) GarageLoadCave()
{
	__asm
	{
		pushad;
		call GarageLoad;
		popad;

		ret;
	}
}

bool IsFileExist(std::string path)
{
	char buffer[MAX_PATH];
	HMODULE hm = NULL;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, NULL, &hm);
	GetModuleFileNameA(hm, buffer, sizeof(buffer));

	std::string modulePath = buffer;
	path = modulePath.substr(0, modulePath.rfind('\\') + 1) + path;

	std::ifstream f(path.c_str());
	bool isGood = f.good();
	f.close();

	return isGood;
}

void Init()
{
	InitGamePositions();
	CIniReader iniReader("NFSCFELoader.ini");

	for (auto& pos : Positions)
	{
		pos.Presset = iniReader.ReadInteger(pos.IniSection, "Presset", -1);
		if (pos.Presset >= 0)
		{
			char pressetBuf[24];
			sprintf(pressetBuf, "PRESSET_%d", pos.Presset);

			pos.X = iniReader.ReadFloat(pressetBuf, (char*)"X", 0);
			pos.Y = iniReader.ReadFloat(pressetBuf, (char*)"Y", 0);
			pos.Z = iniReader.ReadFloat(pressetBuf, (char*)"Z", 0);
			pos.CarZ = iniReader.ReadFloat(pressetBuf, (char*)"CarZ", 0);
			float rotation = iniReader.ReadFloat(pressetBuf, (char*)"CarR", -1);
			pos.Rotation = -1;
			pos.Reflection = iniReader.ReadInteger(pressetBuf, (char*)"CarReflection", 1) == 1;
			pos.WorldLighting = iniReader.ReadFloat(pressetBuf, (char*)"WorldLighting", 2.0f);

			if (rotation >= 0)
			{
				if (rotation > 360.0f)
				{
					rotation = rotation - floor(rotation / 360) * 360.0f;
				}
				pos.Rotation = rotation / 360.0f * 65535.0f;
			}
		}
	}

	injector::MakeCALL(0x007A9A03, GetTrackPositionMarker, true);
	injector::MakeCALL(0x0083F23D, GetTrackPositionMarker, true);
	injector::MakeCALL(0x0083F25E, GetTrackPositionMarker, true);

	injector::MakeJMP(0x0086A18F, CarRotationCave1, true);
	injector::MakeJMP(0x0086A187, CarRotationCave2, true);

	bool loadMapInFE = iniReader.ReadInteger("GENERAL", "LoadMapInFE", 0) == 1;

	if (loadMapInFE)
	{
		injector::WriteMemory(0x006B7E48 + 1, "TRACKS\\STREAML5RA.BUN", true);
		injector::WriteMemory(0x006B7E08 + 1, "TRACKS\\L5RA.BUN", true);
	}

	bool LoadCustomPlatform = iniReader.ReadInteger("GENERAL", "LoadCustomPlatform", 0) == 1;
	CustomPlatformPath = iniReader.ReadString("CUSTOM_PLATFORM", "Path", std::string(""));

	if (CustomPlatformPath.size() > 0 && LoadCustomPlatform)
	{
		if (IsFileExist(CustomPlatformPath))
		{
			injector::MakeCALL(0x0072E63A, DrawGarageHook, true);
			injector::MakeJMP(0x007B154A, GarageLoadCave, true);
			CustomPlatformSettings.x = iniReader.ReadFloat((char*)"CUSTOM_PLATFORM", (char*)"X", 0.0f);
			CustomPlatformSettings.y = iniReader.ReadFloat((char*)"CUSTOM_PLATFORM", (char*)"Y", 0.0f);
			CustomPlatformSettings.z = iniReader.ReadFloat((char*)"CUSTOM_PLATFORM", (char*)"Z", 0.0f);
			CustomPlatformSettings.w = 1.0f;
		}
		else
		{
			MessageBoxA(NULL, "Custom platform geometry not found!", VERSION, MB_ICONERROR);
		}
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
		IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)(base);
		IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);

		if ((base + nt->OptionalHeader.AddressOfEntryPoint + (0x400000 - base)) == 0x87E926) // Check if .exe file is compatible - Thanks to thelink2012 and MWisBest
		{
			Init();
		}
		else
		{
			MessageBoxA(NULL, "This .exe is not supported.\nPlease use v1.4 English nfsc.exe (6,88 MB (7.217.152 bytes)).", VERSION, MB_ICONERROR);
			return FALSE;
		}
	}
	break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
