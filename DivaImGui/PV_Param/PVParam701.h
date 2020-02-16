#pragma once
#include <Windows.h>
#include <string>
#include <filesystem>
#include "parser.hpp"
#include "PVParam701.h"

namespace PVParam
{
		typedef struct {
			float Red, Green, Blue;
		} Light_Param_RGB;

		typedef struct {
			float Red, Green, Blue, Alpha;
		} Light_Param_RGBA;

		typedef struct {
			float X, Y, Z;
		} Light_Param_XYZ;

		typedef struct {
			float X, Y, Z, W;
		} Light_Param_XYZW;

		typedef struct {
			int32_t type;
			Light_Param_RGBA Ambient;
			Light_Param_RGBA Diffuse;
			Light_Param_RGBA Specular;
			Light_Param_XYZW Position;
			Light_Param_XYZW Spot;
			Light_Param_XYZW Spot2;
			byte Unk[64];
		} LightParam;

		typedef struct {
			LightParam LP[8];
		} LightParams;

		typedef struct {
			float time = -1;
			int type = -1;
			bool set = false;
			int32_t interp;
			LightParam data;
		} LightParam_Timed;

		typedef struct {
			int32_t ToneMap;
			float Exposure;
			float Gamma_1;
			int32_t AutoExposure;
			float Gamma_2;
			float Unk_1;
			int32_t SaturateLevel;
			float Gamma_3;
			float Unk_2;
			float FlareA;
			float ShaftA;
			float GhostA;
			Light_Param_XYZ Radius;
			Light_Param_XYZ Inten;
		} PostProcess;

		typedef struct {
			int32_t type;
			float Density;
			float Start;
			float End;
			int32_t Unk_1;
			Light_Param_RGB Color;
			float Unk_2;
			int32_t Unk_3;
		} Fog;

		typedef struct {
			Fog Data[3];
		} Fogs;

		typedef struct {
			float time = -1;
			int type = -1;
			bool set = false;
			int32_t interp;
			Fog data;
		} Fog_Timed;

		static LightParams* LTParam = (LightParams*)0x1411A00A0;
		static PostProcess* PPParam = (PostProcess*)0x1411AC4FC;
		static Fogs* FogParam = (Fogs*)0x1411974D0;

		static bool SaveInterpolate = false;
		static int LTParam_Pos = 0;
		static float* PVCurrentTime = (float*)0x140D0A9A4;

		static bool LTParamLoaded = false;
		static bool FGParamLoaded = false;
		static bool PPParamLoaded = false;

		static int LTData_Dbg_pos = 0;
		static int LTData_Current[8];
		static int LTData_Next[8];

		static int FGData_Current[3];
		static int FGData_Next[3];

		static struct LightParamsData {
			LightParam_Timed data[999];
			int count;
		} LPdata;

		static struct FogData {
			Fog_Timed data[999];
			int count;
		} FGdata;

		float lerp(float a, float b, float f)
		{
			return a + f * (b - a);
		}

		bool LPData_TimeComparer(LightParam_Timed i1, LightParam_Timed i2)
		{
			if (i1.type == -1 && (i2.type != -1))
				return false;

			if (i1.type != -1 && (i2.type == -1))
				return true;

			return (i1.time < i2.time);
		}

		bool LoadCurrentLTPParam()
		{
			LTParamLoaded = false;
			for (int i = 0; i < 8; i++)
			{
				LTData_Current[i] = -2;
				LTData_Next[i] = -1;
			}
			int pvid = *(int*)0x00000001418054C4;

			using namespace std;
			{
				char filename[128];
				auto path = "DivaImGui\\";
				auto path2 = "\\pv_param\\lt_";
				auto pvidstr = std::to_string(pvid);

				auto mdatastr = std::to_string(GetPrivateProfileIntW(L"mdata", L"id", 0, L".\\prjx.ini"));

				strncpy_s(filename, path, strlen(path));
				strncat_s(filename, mdatastr.c_str(), strlen(mdatastr.c_str()));
				strncat_s(filename, path2, strlen(path2));
				strncat_s(filename, pvidstr.c_str(), strlen(pvidstr.c_str()));

				//printf("[DivaImGui] Attempting to Load %s \n", filename);

				int counter = 0;

				std::ifstream csv(filename);
				if (csv.good())
				{
					printf("[DivaImGui] LTParam Load Start... %d\n", pvid);
					for (int i = 0; i < 999; i++)
						LPdata.data[i].type = -1;
					aria::csv::CsvParser parser(csv);
					int rowNum = 0;
					int fieldNum = 0;
					for (auto& row : parser) {
						for (auto& field : row) {
							switch (fieldNum)
							{
							case 0:
								LPdata.data[rowNum].time = std::stof(field);
								break;

							case 1:
								LPdata.data[rowNum].type = std::stoi(field);
								break;

							case 2:
								LPdata.data[rowNum].interp = std::stoi(field);
								break;

							case 4:
								LPdata.data[rowNum].data.type = std::stoi(field);
								break;

							case 5:
								LPdata.data[rowNum].data.Ambient.Red = std::stof(field);
								break;

							case 6:
								LPdata.data[rowNum].data.Ambient.Green = std::stof(field);
								break;

							case 7:
								LPdata.data[rowNum].data.Ambient.Blue = std::stof(field);
								break;

							case 8:
								LPdata.data[rowNum].data.Ambient.Alpha = std::stof(field);
								break;

							case 9:
								LPdata.data[rowNum].data.Diffuse.Red = std::stof(field);
								break;

							case 10:
								LPdata.data[rowNum].data.Diffuse.Green = std::stof(field);
								break;

							case 11:
								LPdata.data[rowNum].data.Diffuse.Blue = std::stof(field);
								break;

							case 12:
								LPdata.data[rowNum].data.Diffuse.Alpha = std::stof(field);
								break;

							case 13:
								LPdata.data[rowNum].data.Specular.Red = std::stof(field);
								break;

							case 14:
								LPdata.data[rowNum].data.Specular.Green = std::stof(field);
								break;

							case 15:
								LPdata.data[rowNum].data.Specular.Blue = std::stof(field);
								break;

							case 16:
								LPdata.data[rowNum].data.Specular.Alpha = std::stof(field);
								break;

							case 17:
								LPdata.data[rowNum].data.Position.X = std::stof(field);
								break;

							case 18:
								LPdata.data[rowNum].data.Position.Y = std::stof(field);
								break;

							case 19:
								LPdata.data[rowNum].data.Position.Z = std::stof(field);
								break;

							case 20:
								LPdata.data[rowNum].data.Position.W = std::stof(field);
								break;

							case 21:
								LPdata.data[rowNum].data.Spot.X = std::stof(field);
								break;

							case 22:
								LPdata.data[rowNum].data.Spot.Y = std::stof(field);
								break;

							case 23:
								LPdata.data[rowNum].data.Spot.Z = std::stof(field);
								break;

							case 24:
								LPdata.data[rowNum].data.Spot.W = std::stof(field);
								break;

							case 25:
								LPdata.data[rowNum].data.Spot2.X = std::stof(field);
								break;

							case 26:
								LPdata.data[rowNum].data.Spot2.Y = std::stof(field);
								break;

							case 27:
								LPdata.data[rowNum].data.Spot2.Z = std::stof(field);
								break;

							case 28:
								LPdata.data[rowNum].data.Spot2.W = std::stof(field);
								break;

							default:
								break;
							}
							fieldNum++;
						}
						fieldNum = 0;
						rowNum++;
					}
					LPdata.count = rowNum + 1;
					printf("[DivaImGui] Loaded %s Count=%d\n", filename, LPdata.count);
					LTParamLoaded = true;
					csv.close();
					int n = sizeof(LPdata.data) / sizeof(LPdata.data[0]);
					sort(LPdata.data, LPdata.data + n, LPData_TimeComparer);
					return true;
				}
				else return false;
			}
		}

		void SaveCurrentLTParam(int type, int interpolate = 0, float time = -1)
		{
			printf("[DivaImGui] LTParam Save Start...\n");
			int pvid = *(int*)0x00000001418054C4;

			using namespace std;
			{
				char filename[128];
				ofstream csv;
				auto path = "DivaImGui\\";
				auto path2 = "\\pv_param\\lt_";
				auto pvidstr = std::to_string(pvid);
				auto mdatastr = std::to_string(GetPrivateProfileIntW(L"mdata", L"id", 0, L".\\prjx.ini"));

				float curTime = *PVCurrentTime;
				if (time != -1)
					curTime = time;

				strncpy_s(filename, path, strlen(path));
				strncat_s(filename, mdatastr.c_str(), strlen(mdatastr.c_str()));
				strncat_s(filename, path2, strlen(path2));
				strncat_s(filename, pvidstr.c_str(), strlen(pvidstr.c_str()));

				csv.open(filename, ofstream::app);
				{
					auto Data = LTParam->LP[type];
					csv << curTime
						<< "," << type
						<< "," << interpolate
						<< "," << 0
						<< "," << Data.type
						<< "," << Data.Ambient.Red
						<< "," << Data.Ambient.Green
						<< "," << Data.Ambient.Blue
						<< "," << Data.Ambient.Alpha
						<< "," << Data.Diffuse.Red
						<< "," << Data.Diffuse.Green
						<< "," << Data.Diffuse.Blue
						<< "," << Data.Diffuse.Alpha
						<< "," << Data.Specular.Red
						<< "," << Data.Specular.Green
						<< "," << Data.Specular.Blue
						<< "," << Data.Specular.Alpha
						<< "," << Data.Position.X
						<< "," << Data.Position.Y
						<< "," << Data.Position.Z
						<< "," << Data.Position.W
						<< "," << Data.Spot.X
						<< "," << Data.Spot.Y
						<< "," << Data.Spot.Z
						<< "," << Data.Spot.W
						<< "," << Data.Spot2.X
						<< "," << Data.Spot2.Y
						<< "," << Data.Spot2.Z
						<< "," << Data.Spot2.W << "\n";
				}
				csv.close();
				printf("[DivaImGui] Saved to %s\n", filename);
			}
		}

		void SetCurrentLTParam()
		{
			for (int i = 0; i < 8; i++)
			{
				if (LTData_Current[i] == -1)
					continue;

				if (LTData_Next[i] != -1)
					if (LPdata.data[LTData_Next[i]].time < *PVCurrentTime)
					{
						LTData_Current[i] = LTData_Next[i];
						LTData_Next[i] = -1;
						if (LTData_Current[i] != -1)
						{
							for (int c = LTData_Current[i]; c < LPdata.count; c++)
							{
								if (LTData_Current[i] != c)
									if (LPdata.data[c].type == i)
									{
										//printf("[DivaImGui] Time Lookup found %d %d\n", LPdata.data[c].type, c);
										LTData_Next[i] = c;
										break;
									}
							}
						}
					}

				if (LTData_Current[i] == -2)
				{
					//printf("[DivaImGui] Lookup Start\n");
					LTData_Current[i] = -1;

					for (int c = 0; c < LPdata.count; c++)
					{
						if (LPdata.data[c].type == i)
						{
							//printf("[DivaImGui] Lookup found %d %d\n", LPdata.data[c].type, c);
							LTData_Current[i] = c;
							break;
						}
					}

					if (LTData_Current[i] != -1)
						for (int c = LTData_Current[i]; c < 999; c++)
						{
							if (LTData_Current[i] != c)
								if (LPdata.data[c].type == i)
								{
									LTData_Next[i] = c;
									//printf("[DivaImGui] Lookup found Next %d %d\n", LPdata.data[c].type, c);
									break;
								}
						}
				}

				if (*PVCurrentTime != 0)
					if (*PVCurrentTime > LPdata.data[LTData_Current[i]].time)
					{
						if (LPdata.data[LTData_Current[i]].set == false)
						{
							memcpy(LPdata.data[LTData_Current[i]].data.Unk, LTParam->LP[i].Unk, 64);
							LTParam->LP[i] = LPdata.data[LTData_Current[i]].data;
							LPdata.data[LTData_Current[i]].set = true;
						}
						if (LTData_Next[i] != -1)
							if (LPdata.data[LTData_Next[i]].interp == true)
							{
								float oldRange = LPdata.data[LTData_Next[i]].time - LPdata.data[LTData_Current[i]].time;
								float newRange = 1 - 0;
								float newValue = ((*PVCurrentTime - LPdata.data[LTData_Next[i]].time) * newRange / oldRange) + 1;

								LTParam->LP[i].Ambient.Red = lerp(LTParam->LP[i].Ambient.Red, LPdata.data[LTData_Next[i]].data.Ambient.Red, newValue);
								LTParam->LP[i].Ambient.Green = lerp(LTParam->LP[i].Ambient.Green, LPdata.data[LTData_Next[i]].data.Ambient.Green, newValue);
								LTParam->LP[i].Ambient.Blue = lerp(LTParam->LP[i].Ambient.Blue, LPdata.data[LTData_Next[i]].data.Ambient.Blue, newValue);
								LTParam->LP[i].Ambient.Alpha = lerp(LTParam->LP[i].Ambient.Alpha, LPdata.data[LTData_Next[i]].data.Ambient.Alpha, newValue);

								LTParam->LP[i].Diffuse.Red = lerp(LTParam->LP[i].Diffuse.Red, LPdata.data[LTData_Next[i]].data.Diffuse.Red, newValue);
								LTParam->LP[i].Diffuse.Green = lerp(LTParam->LP[i].Diffuse.Green, LPdata.data[LTData_Next[i]].data.Diffuse.Green, newValue);
								LTParam->LP[i].Diffuse.Blue = lerp(LTParam->LP[i].Diffuse.Blue, LPdata.data[LTData_Next[i]].data.Diffuse.Blue, newValue);
								LTParam->LP[i].Diffuse.Alpha = lerp(LTParam->LP[i].Diffuse.Alpha, LPdata.data[LTData_Next[i]].data.Diffuse.Alpha, newValue);

								LTParam->LP[i].Specular.Red = lerp(LTParam->LP[i].Specular.Red, LPdata.data[LTData_Next[i]].data.Specular.Red, newValue);
								LTParam->LP[i].Specular.Green = lerp(LTParam->LP[i].Specular.Green, LPdata.data[LTData_Next[i]].data.Specular.Green, newValue);
								LTParam->LP[i].Specular.Blue = lerp(LTParam->LP[i].Specular.Blue, LPdata.data[LTData_Next[i]].data.Specular.Blue, newValue);
								LTParam->LP[i].Specular.Alpha = lerp(LTParam->LP[i].Specular.Alpha, LPdata.data[LTData_Next[i]].data.Specular.Alpha, newValue);

								LTParam->LP[i].Spot.X = lerp(LTParam->LP[i].Spot.X, LPdata.data[LTData_Next[i]].data.Spot.X, newValue);
								LTParam->LP[i].Spot.Y = lerp(LTParam->LP[i].Spot.Y, LPdata.data[LTData_Next[i]].data.Spot.Y, newValue);
								LTParam->LP[i].Spot.Z = lerp(LTParam->LP[i].Spot.Z, LPdata.data[LTData_Next[i]].data.Spot.Z, newValue);
								LTParam->LP[i].Spot.W = lerp(LTParam->LP[i].Spot.W, LPdata.data[LTData_Next[i]].data.Spot.W, newValue);

								LTParam->LP[i].Spot2.X = lerp(LTParam->LP[i].Spot2.X, LPdata.data[LTData_Next[i]].data.Spot2.X, newValue);
								LTParam->LP[i].Spot2.Y = lerp(LTParam->LP[i].Spot2.Y, LPdata.data[LTData_Next[i]].data.Spot2.Y, newValue);
								LTParam->LP[i].Spot2.Z = lerp(LTParam->LP[i].Spot2.Z, LPdata.data[LTData_Next[i]].data.Spot2.Z, newValue);
								LTParam->LP[i].Spot2.W = lerp(LTParam->LP[i].Spot2.W, LPdata.data[LTData_Next[i]].data.Spot2.W, newValue);
							}
					}
			}
		}

		void SetCurrentFogParam()
		{
			for (int i = 0; i < 3; i++)
			{
				if (FGData_Current[i] == -1)
					continue;

				if (FGData_Next[i] != -1)
					if (FGdata.data[FGData_Next[i]].time < *PVCurrentTime)
					{
						FGData_Current[i] = FGData_Next[i];
						FGData_Next[i] = -1;
						if (FGData_Current[i] != -1)
						{
							for (int c = FGData_Current[i]; c < FGdata.count; c++)
							{
								if (FGData_Current[i] != c)
									if (FGdata.data[c].type == i)
									{
										FGData_Next[i] = c;
										break;
									}
							}
						}
					}

				if (FGData_Current[i] == -2)
				{
					FGData_Current[i] = -1;

					for (int c = 0; c < FGdata.count; c++)
					{
						if (FGdata.data[c].type == i)
						{
							FGData_Current[i] = c;
							break;
						}
					}

					if (FGData_Current[i] != -1)
						for (int c = FGData_Current[i]; c < 999; c++)
						{
							if (FGData_Current[i] != c)
								if (FGdata.data[c].type == i)
								{
									FGData_Next[i] = c;
									break;
								}
						}
				}

				if (*PVCurrentTime != 0)
					if (*PVCurrentTime > FGdata.data[FGData_Current[i]].time)
					{
						if (FGdata.data[FGData_Current[i]].set == false)
						{
							FogParam->Data[i].Unk_1 = FGdata.data[FGData_Current[i]].data.Unk_1;
							FogParam->Data[i].Unk_2 = FGdata.data[FGData_Current[i]].data.Unk_2;
							FogParam->Data[i].Unk_3 = FGdata.data[FGData_Current[i]].data.Unk_3;
							FogParam->Data[i] = FGdata.data[FGData_Current[i]].data;
							FGdata.data[FGData_Current[i]].set = true;
						}
						if (FGData_Next[i] != -1)
							if (FGdata.data[FGData_Next[i]].interp == true)
							{
								float oldRange = FGdata.data[FGData_Next[i]].time - FGdata.data[FGData_Current[i]].time;
								float newRange = 1 - 0;
								float newValue = ((*PVCurrentTime - FGdata.data[FGData_Next[i]].time) * newRange / oldRange) + 1;

								FogParam->Data[i].Density = lerp(FogParam->Data[i].Density, FGdata.data[FGData_Next[i]].data.Density, newValue);
								FogParam->Data[i].Start = lerp(FogParam->Data[i].Start, FGdata.data[FGData_Next[i]].data.Start, newValue);
								FogParam->Data[i].End = lerp(FogParam->Data[i].End, FGdata.data[FGData_Next[i]].data.End, newValue);

								FogParam->Data[i].Color.Red = lerp(FogParam->Data[i].Color.Red, FGdata.data[FGData_Next[i]].data.Color.Red, newValue);
								FogParam->Data[i].Color.Green = lerp(FogParam->Data[i].Color.Green, FGdata.data[FGData_Next[i]].data.Color.Green, newValue);
								FogParam->Data[i].Color.Blue = lerp(FogParam->Data[i].Color.Blue, FGdata.data[FGData_Next[i]].data.Color.Blue, newValue);
							}
					}
			}
		}

		bool LoadCurrentFogParam()
		{
			FGParamLoaded = false;
			for (int i = 0; i < 8; i++)
			{
				LTData_Current[i] = -2;
				LTData_Next[i] = -1;
			}
			int pvid = *(int*)0x00000001418054C4;

			using namespace std;
			{
				char filename[128];
				auto path = "DivaImGui\\";
				auto path2 = "\\pv_param\\fog_";
				auto pvidstr = std::to_string(pvid);
				auto mdatastr = std::to_string(GetPrivateProfileIntW(L"mdata", L"id", 0, L".\\prjx.ini"));

				strncpy_s(filename, path, strlen(path));
				strncat_s(filename, mdatastr.c_str(), strlen(mdatastr.c_str()));
				strncat_s(filename, path2, strlen(path2));
				strncat_s(filename, pvidstr.c_str(), strlen(pvidstr.c_str()));

				//printf("[DivaImGui] Attempting to Load %s \n", filename);

				int counter = 0;

				std::ifstream csv(filename);
				if (csv.good())
				{
					printf("[DivaImGui] FogParam Load Start... %d\n", pvid);
					for (int i = 0; i < 999; i++)
						FGdata.data[i].type = -1;
					aria::csv::CsvParser parser(csv);
					int rowNum = 0;
					int fieldNum = 0;
					for (auto& row : parser) {
						for (auto& field : row) {
							switch (fieldNum)
							{
							case 0:
								FGdata.data[rowNum].time = std::stof(field);
								break;

							case 1:
								FGdata.data[rowNum].type = std::stoi(field);
								break;

							case 2:
								FGdata.data[rowNum].interp = std::stoi(field);
								break;

							case 4:
								FGdata.data[rowNum].data.type = std::stoi(field);
								break;

							case 5:
								FGdata.data[rowNum].data.Density = std::stof(field);
								break;

							case 6:
								FGdata.data[rowNum].data.Start = std::stof(field);
								break;

							case 7:
								FGdata.data[rowNum].data.End = std::stof(field);
								break;

							case 8:
								FGdata.data[rowNum].data.Color.Red = std::stof(field);
								break;

							case 9:
								FGdata.data[rowNum].data.Color.Green = std::stof(field);
								break;

							case 10:
								FGdata.data[rowNum].data.Color.Blue = std::stof(field);
								break;

							default:
								break;
							}
							fieldNum++;
						}
						fieldNum = 0;
						rowNum++;
					}
					LPdata.count = rowNum + 1;
					printf("[DivaImGui] Loaded %s Count=%d\n", filename, LPdata.count);
					FGParamLoaded = true;
					csv.close();
					int n = sizeof(LPdata.data) / sizeof(LPdata.data[0]);
					sort(LPdata.data, LPdata.data + n, LPData_TimeComparer);
					return true;
				}
				else return false;
			}
		}

		void SaveCurrentFogParam(int type, int interpolate = 0, float time = -1)
		{
			printf("[DivaImGui] FOG Param Save Start...\n");
			int pvid = *(int*)0x00000001418054C4;

			using namespace std;
			{
				char filename[128];
				ofstream csv;
				auto path = "DivaImGui\\";
				auto path2 = "\\pv_param\\fog_";
				auto pvidstr = std::to_string(pvid);
				auto mdatastr = std::to_string(GetPrivateProfileIntW(L"mdata", L"id", 0, L".\\prjx.ini"));

				float curTime = *PVCurrentTime;
				if (time != -1)
					curTime = time;

				strncpy_s(filename, path, strlen(path));
				strncat_s(filename, mdatastr.c_str(), strlen(mdatastr.c_str()));
				strncat_s(filename, path2, strlen(path2));
				strncat_s(filename, pvidstr.c_str(), strlen(pvidstr.c_str()));

				csv.open(filename, ofstream::app);
				{
					auto Data = FogParam->Data[type];
					csv << curTime
						<< "," << type
						<< "," << interpolate
						<< "," << 0
						<< "," << Data.type
						<< "," << Data.Density
						<< "," << Data.Start
						<< "," << Data.End
						<< "," << Data.Color.Red
						<< "," << Data.Color.Green
						<< "," << Data.Color.Blue << "\n";
				}
				csv.close();
				printf("[DivaImGui] Saved to %s\n", filename);
			}
		}

		bool LoadCurrentPPParam()
		{

		}

		void SaveCurrentPPParam(int type, int interpolate = 0)
		{
			printf("[DivaImGui] PP Param Save Start...\n");
			int pvid = *(int*)0x00000001418054C4;

			using namespace std;
			{
				char filename[128];
				ofstream csv;
				auto path = "DivaImGui\\pv_param\\pp_";
				auto pvidstr = std::to_string(pvid);

				strncpy_s(filename, path, strlen(path));
				strncat_s(filename, pvidstr.c_str(), strlen(pvidstr.c_str()));

				csv.open(filename, ofstream::app);
				{

				}
				csv.close();
				printf("[DivaImGui] Saved to %s\n", filename);
			}
		}
}

