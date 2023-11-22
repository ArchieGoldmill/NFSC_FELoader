// TODO: fix small difference in speed rotation between car and disc, and make it play disc sound using bass.dll
#include <string>
#include <D3dx9math.h>
#include <vector>
#include <windows.h>
#include "IniReader/IniReader.h"
#include "Injector/injector.hpp"


#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")


struct Position
{
	const char* IniSection;
	const char* Name;
	int Presset;

	float X;
	float Y;
	float Z;
	float CarZ;
	int Rotation;
	bool Reflection;
	bool CustomPlatform;
};

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

const char* VERSION = "NFSC - FE Loader 3.0";
const float DefaultCarZ = 0.275f;
const float DefaultShadowZ = -0.25;

Position Positions[10];
D3DXVECTOR4 CustomPlatformPosition;
std::string CustomPlatformPath;
bool DrawCustomPlatform = false;

float DiscRotationAngle = 0.0f;
float CarRotationSpeed = 40.0f;
float CarRotationReturningSpeed = 68.0f;
int CarRotationSendSpeed = 0;
bool ReturnDisc = 0;
int HK_Left, HK_Right;

void ToggleReflections(bool enabled)
{
	injector::WriteMemory<BYTE>(0x0072E603, enabled ? 0x74 : 0xEB);
}

namespace Game
{
	float* ShadowZ = (float*)0x007BE7E2;
	int* GameState = (int*)0x00A99BBC;
	float* CarZ = (float*)0x009F9AE8;
	float* DeltaTime = (float*)0x00A99A5C;
	float* FrontSteerAngle = (float*)0x00A7B668;

	auto GetTrackPositionMarker = (TrackPositionMarker * (__cdecl*)(const char* a1, int a2))0x0079DB20;
	auto StringHash = (int(__cdecl*)(const char*))0x471050;
	auto eViewPlatInterface_Render = (int(__thiscall*)(void*, void*, D3DXMATRIX*, D3DXMATRIX*, int, int, int))0x00729320;
	auto eModel_Init = (void(__thiscall*)(void*, int))0x0055E7E0;
	auto eModel_GetBoundingBox = (void(__thiscall*)(void*, D3DXVECTOR3*, D3DXVECTOR3*))0x005589C0;
	auto LoadResourceFile = (void(__cdecl*)(const char* path, int type, int, int, int, int, int))0x006B5980;
	auto FindResourceFile = (int* (__cdecl*)(const char* path))0x006A8570;
	auto RenderWorld = (int(__thiscall*)(void*, void*, void*))0x0079AFA0;
	auto FeCustomizeParts_Ctor = (void(__thiscall*)(void*, void*))0x0085A5A0;
	auto FeCustomizeParts_Dtor = (void(__thiscall*)(void*))0x0083FF50;
}

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
			*Game::CarZ = DefaultCarZ - pos.CarZ;
			*Game::ShadowZ = DefaultShadowZ + pos.CarZ;

			DrawCustomPlatform = pos.CustomPlatform;


			return &marker;
		}
	}

	return Game::GetTrackPositionMarker(position, a2);
}

void InitGamePositions()
{
	Positions[0].IniSection = "POS_CARLOT_MAZDA";
	Positions[0].Name = "CarPosition_CarLot_Mazda";

	Positions[1].IniSection = "POS_CARLOT_EXOTIC";
	Positions[1].Name = "CarPosition_CarLot_Exotic";

	Positions[2].IniSection = "POS_CARLOT_TUNER";
	Positions[2].Name = "CarPosition_CarLot_Tuner";

	Positions[3].IniSection = "POS_CARLOT_MUSCLE";
	Positions[3].Name = "CarPosition_CarLot_Muscle";

	Positions[4].IniSection = "POS_CARCLASS";
	Positions[4].Name = "CarPosition_CarClass";

	Positions[5].IniSection = "POS_EXOTIC";
	Positions[5].Name = "CarPosition_Exotic";

	Positions[6].IniSection = "POS_TUNER";
	Positions[6].Name = "CarPosition_Tuner";

	Positions[7].IniSection = "POS_MUSCLE";
	Positions[7].Name = "CarPosition_Muscle";

	Positions[8].IniSection = "POS_MAIN";
	Positions[8].Name = "CarPosition_Main";

	Positions[9].IniSection = "POS_GENERIC";
	Positions[9].Name = "CarPosition_Generic";
}

void __declspec(naked) CarRotationCave2()
{
	static constexpr auto cExit = 0x0086A18D;
	__asm
	{
		cmp CustomRotation, 1;
		jne Original2;


		mov edi, [CarRotationSendSpeed];
		add[edi + 0x20], edi;
		xor edi, edi;
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


		mov eax, [esi + 0x48];
		mov edi, [CarRotationSendSpeed];
		add[eax + 0x20], edi;
		jmp cExit;

	Original1:
		mov eax, [esi + 0x48];
		mov[eax + 0x20], di;
		jmp cExit;
	}
}

std::vector<void*> GarageParts;
void* GarageScrollPart = NULL;
void* GarageDiscPart = NULL;
bool GarageInit = false;
D3DXMATRIX m;

struct PartEntry
{
	int hash;
	int* ptr;
};

bool StartsWith(char* str, const char* prefix)
{
	int slen = strlen(str);
	int plen = strlen(prefix);
	if (slen < plen)
	{
		return false;
	}

	for (int i = 0; i < plen; i++)
	{
		if (str[i] != prefix[i])
		{
			return false;
		}
	}

	return true;
}

float ScrollOffset = 0;
float ScrollLen = 0;
float ScrollAngle = 0;
int ScrollItems;
int DiscItems;
D3DXMATRIX* ScrollMatrises;
D3DXMATRIX* RotateMatrises;
float ScrollSpeed, TargetScrollSpeed, ScrollSpeedMin, ScrollSpeedMax;
bool InitCustomGarage()
{
	if (!GarageInit)
	{
		int* recource = Game::FindResourceFile(CustomPlatformPath.c_str());
		if (recource)
		{
			D3DXMatrixScaling(&m, 1, 1, 1);
			m._41 = CustomPlatformPosition.x;
			m._42 = CustomPlatformPosition.y;
			m._43 = CustomPlatformPosition.z;
			m._44 = 1;

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
				Game::eModel_Init(model, Game::StringHash(name));
				if (StartsWith(name, "SCROLL_"))
				{
					GarageScrollPart = model;
					D3DXVECTOR3 a, b;
					Game::eModel_GetBoundingBox(GarageScrollPart, &a, &b);
					ScrollLen = abs(a.x) + abs(b.x) - 0.02f;
					ScrollMatrises = new D3DXMATRIX[ScrollItems * 2];
				}
				if (StartsWith(name, "DISC01"))
				{
					GarageDiscPart = model;
					RotateMatrises = new D3DXMATRIX;

				}
				else
				{
					GarageParts.push_back(model);
				}
			}

			GarageInit = true;
		}
	}

	return GarageInit;
}

void Render(void* plat, void* model, D3DXMATRIX& matrix)
{
	Game::eViewPlatInterface_Render(plat, model, &matrix, 0, 0, 0, 0);
}

void InitScrollMatrices()
{
	for (int i = 0; i < ScrollItems; i++)
	{
		ScrollMatrises[i] = m;
		ScrollMatrises[i]._41 += ScrollLen * i;
	}

	for (int i = ScrollItems, j = 1; i < ScrollItems * 2; i++, j++)
	{
		ScrollMatrises[i] = m;
		ScrollMatrises[i]._41 += -ScrollLen * j;
	}
}

void __stdcall DrawGarage(void* plat)
{
	if (InitCustomGarage())
	{
		for (auto model : GarageParts)
		{
			Render(plat, model, m);
		}

		if (GarageScrollPart)
		{
			for (int i = 0; i < ScrollItems * 2; i++)
			{
				Render(plat, GarageScrollPart, ScrollMatrises[i]);
			}
		}
		if (GarageDiscPart)
		{

			Render(plat, GarageDiscPart, *RotateMatrises);

		}

	}
}

void MoveTowards(float& a, float b, float step)
{
	if (a < b)
	{
		a += step;
		if (a > b)
		{
			a = b;
		}
	}

	if (a > b)
	{
		a -= step;
		if (a < b)
		{
			a = b;
		}
	}
}

void InitDisc() {
	// a bit messy part with moving disc part to 0 coordinates, rotating it and returning it back
	D3DXMATRIX rotationMatrix;
	*RotateMatrises = m;
	RotateMatrises[0]._41 -= CustomPlatformPosition.x;
	RotateMatrises[0]._42 -= CustomPlatformPosition.y;
	RotateMatrises[0]._43 -= CustomPlatformPosition.z;
	D3DXMatrixRotationZ(&rotationMatrix, DiscRotationAngle);
	rotationMatrix._41 += CustomPlatformPosition.x;
	rotationMatrix._42 += CustomPlatformPosition.y;
	rotationMatrix._43 += CustomPlatformPosition.z;
	*RotateMatrises *= rotationMatrix;
}

void Update()
{
	if (GarageScrollPart)
	{
		ScrollAngle += *Game::DeltaTime * ScrollSpeed * 1.5;
		if (ScrollAngle > 6.28f)
		{
			ScrollAngle -= 6.28f;
		}

		ScrollOffset += *Game::DeltaTime * ScrollSpeed;
		if (ScrollOffset > ScrollLen)
		{
			ScrollOffset -= ScrollLen;
		}

		InitScrollMatrices();

		for (int i = 0; i < ScrollItems * 2; i++)
		{
			ScrollMatrises[i]._41 -= ScrollOffset;
		}

		MoveTowards(ScrollSpeed, TargetScrollSpeed, *Game::DeltaTime * 5);
	}


	if (GarageDiscPart) {
		//#define SOUND_FILE_PATH L"G:/Programms/Need for Speed Carbon/scripts/3.wav"

		InitDisc();
		if (ReturnDisc == false) {
			if (GetAsyncKeyState(HK_Left))
			{
				DiscRotationAngle -= *Game::DeltaTime * 0.7f;
				//PlaySound(SOUND_FILE_PATH, NULL, SND_ASYNC | SND_NOSTOP | SND_NODEFAULT);
				CarRotationSendSpeed = -1 * CarRotationSpeed / 360.0f * 65535.0f * *Game::DeltaTime;
			}
			else if (GetAsyncKeyState(HK_Right))
			{
				DiscRotationAngle += *Game::DeltaTime * 0.7f;
				//PlaySound(SOUND_FILE_PATH, NULL, SND_ASYNC | SND_NOSTOP | SND_NODEFAULT);
				CarRotationSendSpeed = CarRotationSpeed / 360.0f * 65535.0f * *Game::DeltaTime;
			}
			else
			{
				//PlaySound(NULL, NULL, SND_FILENAME | SND_ASYNC);
				CarRotationSendSpeed = 0;
			}


		}
		else
		{
			float rotationSpeed = 1.2f;
			if (DiscRotationAngle > 0.0f)
			{
				DiscRotationAngle -= *Game::DeltaTime * rotationSpeed;
				CarRotationSendSpeed = -1 * CarRotationReturningSpeed / 360.0f * 65535.0f * *Game::DeltaTime;
				//PlaySound(SOUND_FILE_PATH, NULL, SND_ASYNC | SND_NOSTOP | SND_NODEFAULT);

				if (DiscRotationAngle < 0.0f)
				{
					DiscRotationAngle = 0.0f;
					CarRotationSendSpeed = 0 / 360.0f * 65535.0f * *Game::DeltaTime;
					//PlaySound(NULL, NULL, SND_FILENAME | SND_ASYNC);
				}
			}
			else if (DiscRotationAngle < 0.0f)
			{
				DiscRotationAngle += *Game::DeltaTime * rotationSpeed;
				CarRotationSendSpeed = CarRotationReturningSpeed / 360.0f * 65535.0f * *Game::DeltaTime;
				//PlaySound(SOUND_FILE_PATH, NULL, SND_ASYNC | SND_NOSTOP | SND_NODEFAULT);
				if (DiscRotationAngle > 0.0f)
				{
					DiscRotationAngle = 0.0f;
					CarRotationSendSpeed = 0 / 360.0f * 65535.0f * *Game::DeltaTime;
					//PlaySound(NULL, NULL, SND_FILENAME | SND_ASYNC);
				}
			}

		}

	}

}


void __fastcall DrawGarageMain(void* a1, int, void* a2, void* a3)
{
	if (*Game::GameState == 3 && DrawCustomPlatform) {

		Update();
		DrawGarage(a1);
	}
	else
	{
		DiscRotationAngle = 0;
	}

	Game::RenderWorld(a1, a2, a3);
}

void _fastcall DrawGarageReflection(void* a1, int, void* a2, void* a3)
{
	if (*Game::GameState == 3 && DrawCustomPlatform)
	{
		DrawGarage(a1);
	}

	Game::RenderWorld(a1, a2, a3);
}

void __stdcall GarageLoad()
{
	Game::LoadResourceFile(CustomPlatformPath.c_str(), 6, 0, 0, 0, 0, 0);
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

void __stdcall RotateFEWheels(int num, D3DXMATRIX* wheel, D3DXMATRIX* brake)
{
	if (GarageScrollPart && DrawCustomPlatform)
	{
		D3DXMATRIX rot;
		D3DXMatrixRotationY(&rot, ScrollAngle);
		D3DXMatrixMultiply(wheel, wheel, &rot);
	}
	else if (num <= 0x40)
	{
		D3DXMATRIX rot;
		D3DXMatrixRotationZ(&rot, *Game::FrontSteerAngle * 3.14 / 180.0);
		D3DXMatrixMultiply(wheel, wheel, &rot);
		D3DXMatrixMultiply(brake, brake, &rot);
	}
}

void __declspec(naked) UpdateRenderingCarParametersCave()
{
	static constexpr auto cExit = 0x0084F82F;
	__asm
	{
		pushad;
		push eax;
		lea eax, [esp + esi + 0xA4];
		push eax;
		push esi;
		call RotateFEWheels;
		popad;

		jmp cExit;
	}
}

void __fastcall FeCustomizeParts_Ctor(void* _this, int, void* data)
{
	TargetScrollSpeed = ScrollSpeedMin;
	ReturnDisc = true;
	Game::FeCustomizeParts_Ctor(_this, data);
}

void __fastcall FeCustomizeParts_Dtor(void* _this)
{
	TargetScrollSpeed = ScrollSpeedMax;
	ReturnDisc = false;
	Game::FeCustomizeParts_Dtor(_this);
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
			char PressetIni[24];
			sprintf(PressetIni, "PRESSET_%d", pos.Presset);

			pos.X = iniReader.ReadFloat(PressetIni, (char*)"X", 0);
			pos.Y = iniReader.ReadFloat(PressetIni, (char*)"Y", 0);
			pos.Z = iniReader.ReadFloat(PressetIni, (char*)"Z", 0);

			pos.CarZ = iniReader.ReadFloat(PressetIni, (char*)"CarZ", 0);

			pos.CustomPlatform = iniReader.ReadInteger(PressetIni, (char*)"CustomPlatform", 0) == 1;

			pos.Reflection = iniReader.ReadInteger(PressetIni, (char*)"CarReflection", 1) == 1;

			float rotation = iniReader.ReadFloat(PressetIni, (char*)"CarRotation", -1);
			pos.Rotation = -1;
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

	HK_Left = iniReader.ReadInteger("GENERAL", "RotateLeftKey", 0);
	HK_Right = iniReader.ReadInteger("GENERAL", "RotateRightKey", 0);

	bool loadMapInFE = iniReader.ReadInteger("GENERAL", "LoadMapInFE", 0) == 1;
	if (loadMapInFE)
	{
		injector::WriteMemory(0x006B7E49, "TRACKS\\STREAML5RA.BUN", true);
		injector::WriteMemory(0x006B7E09, "TRACKS\\L5RA.BUN", true);
	}

	bool LoadCustomPlatform = iniReader.ReadInteger("GENERAL", "LoadCustomPlatform", 0) == 1;
	CustomPlatformPath = iniReader.ReadString("CUSTOM_PLATFORM", "Path", std::string(""));
	if (!CustomPlatformPath.empty() && LoadCustomPlatform)
	{
		if (IsFileExist(CustomPlatformPath.c_str()))
		{
			injector::MakeCALL(0x0072E63A, DrawGarageMain, true);
			CIniReader hdReflections("NFSCHDReflections.ini");
			if (hdReflections.ReadInteger("GENERAL", "RealFrontEndReflections", 0) == 1)
			{
				injector::MakeCALL(0x0072E535, DrawGarageReflection, true);
			}

			injector::MakeJMP(0x007B154A, GarageLoadCave, true);
			injector::MakeJMP(0x0084F80B, UpdateRenderingCarParametersCave, true);

			injector::MakeCALL(0x005748D9, FeCustomizeParts_Ctor);
			injector::MakeCALL(0x00850BC3, FeCustomizeParts_Dtor);

			CustomPlatformPosition.x = iniReader.ReadFloat("CUSTOM_PLATFORM", "X", 0.0f);
			CustomPlatformPosition.y = iniReader.ReadFloat("CUSTOM_PLATFORM", "Y", 0.0f);
			CustomPlatformPosition.z = iniReader.ReadFloat("CUSTOM_PLATFORM", "Z", 0.0f);
			CustomPlatformPosition.w = 1.0f;

			ScrollItems = iniReader.ReadInteger("CUSTOM_PLATFORM", "ScrollItems", 25);

			ScrollSpeedMin = iniReader.ReadFloat("CUSTOM_PLATFORM", "ScrollSpeedMin", 1.0f);
			ScrollSpeedMax = iniReader.ReadFloat("CUSTOM_PLATFORM", "ScrollSpeedMax", 10.0f);

			ScrollSpeed = ScrollSpeedMax;
			TargetScrollSpeed = ScrollSpeedMax;
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