#include "pch.h"
#include "IniReader.h"
#include "injector/injector.hpp"

float* WorldLighting = (float*)0x00A6C218;
float* CarZ = (float*)0x009F9AE8;
const float DefaultCarZ = 0.275f;
float* ShadowZ = (float*)0x007BE7E2;
const float DefaultShadowZ = -0.25;

struct Position
{
	char* IniSection;
	char* Name;
	int Presset;

	float X;
	float Y;
	float Z;
	float CarZ;
	int Rotation;
	bool Reflection;
	float WorldLighting;
};

Position Positions[10];

struct TrackPositionMarker
{
	TrackPositionMarker* Prev;
	TrackPositionMarker* Next;
	int Hash;
	int blank;
	float X;
	float Y;
	float Z;
	float W;
	unsigned short Rotation;
};

void ToggleReflections(bool enabled)
{
	injector::WriteMemory<BYTE>(0x0072E603, enabled ? 0x74 : 0xEB);
}

auto Game_GetTrackPositionMarker = (TrackPositionMarker * (__cdecl*)(const char* a1, int a2))0x0079DB20;

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

			pos.Rotation = iniReader.ReadInteger(pressetBuf, (char*)"R", -1);
			pos.X = iniReader.ReadFloat(pressetBuf, (char*)"X", 0);
			pos.Y = iniReader.ReadFloat(pressetBuf, (char*)"Y", 0);
			pos.Z = iniReader.ReadFloat(pressetBuf, (char*)"Z", 0);
			pos.CarZ = iniReader.ReadFloat(pressetBuf, (char*)"CarZ", 0);
			pos.Reflection = iniReader.ReadInteger(pressetBuf, (char*)"CarReflection", 1) == 1;
			pos.WorldLighting = iniReader.ReadFloat(pressetBuf, (char*)"WorldLighting", 2.0f);
		}
	}

	injector::MakeCALL(0x005BBE5B, GetTrackPositionMarker, true);
	injector::MakeCALL(0x007A9A03, GetTrackPositionMarker, true);
	injector::MakeCALL(0x0083F23D, GetTrackPositionMarker, true);
	injector::MakeCALL(0x0083F25E, GetTrackPositionMarker, true);

	injector::MakeJMP(0x0086A18F, CarRotationCave1, true);
	injector::MakeJMP(0x0086A187, CarRotationCave2, true);

	int loadMapInFE = iniReader.ReadInteger("GENERAL", "LoadMapInFE", 0);
	if (loadMapInFE == 1)
	{
		injector::WriteMemory(0x006B7E48 + 1, "TRACKS\\STREAML5RA.BUN", true);
		injector::WriteMemory(0x006B7E08 + 1, "TRACKS\\L5RA.BUN", true);
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
			MessageBoxA(NULL, "This .exe is not supported.\nPlease use v1.4 English nfsc.exe (6,88 MB (7.217.152 bytes)).", "NFSC - FE Loader 1.0", MB_ICONERROR);
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
