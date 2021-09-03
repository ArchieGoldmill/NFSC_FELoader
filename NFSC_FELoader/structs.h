#pragma once

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

struct VectorW
{
	float x;
	float y;
	float z;
	float w;
};

struct RotationMatrix
{
	VectorW x;
	VectorW y;
	VectorW z;
};

struct RotPosMatrix
{
	VectorW x;
	VectorW y;
	VectorW z;
	VectorW w;
};