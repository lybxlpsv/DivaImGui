#include "stdafx.h"
#include "GLComponent.h"
#include <MinHook.h>

#include <stdio.h>

#include <chrono>
#include <thread>

#include <Windows.h>

#include "MainModule.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl2.h"
#include "imgui/imgui_impl_win32.h"
#include "GL/glew.h"
#include "GL/gl.h"
#include "GL/wglext.h"
#include "parser.hpp"
#include "FileSystem/ConfigFile.h"
#include "Constants.h"

#include "Keyboard/Keyboard.h"
#include "detours/detours.h"

namespace DivaImGui
{
	using namespace std::chrono;
	using dsec = duration<double>;

	typedef chrono::time_point<chrono::steady_clock> steady_clock;
	typedef chrono::high_resolution_clock high_resolution_clock;

	steady_clock start;
	steady_clock end;

	// Function prototype

	typedef BOOL(__stdcall* GLSwapBuffers)(HDC);
	typedef void(__stdcall* GLShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
	typedef void(__stdcall* GLShaderSourceARB)(GLhandleARB, GLsizei, const GLcharARB**, const GLint*);
	typedef void(__stdcall* GLProgramStringARB)(GLenum, GLenum, GLsizei, const void*);
	typedef void(__stdcall* GLBindProgramARB)(GLenum, GLuint);
	typedef void(__stdcall* GLBindTexture)(GLenum, GLuint);
	typedef GLenum(__stdcall* GLGetError)();
	typedef PROC(__stdcall* WGlGetProcAddress)(LPCSTR);
	typedef int(__stdcall* PDGetFramerate)(void);
	typedef void(__stdcall* PDSetFramerate)(int);
	typedef int(__stdcall* DNInitialize)(void);
	typedef int(__stdcall* DNRefreshShaders)(void);
	typedef int(__stdcall* DNProcessShader)(int, int, int, int, int);
	typedef int(__stdcall* ReshadeRender)();
	//typedef std::chrono::nanoseconds*(__stdcall* PDGetFrameratePtr)(void);
	// Function pointer
	GLSwapBuffers fnGLSwapBuffers;
	PDGetFramerate getFrameratePD;
	PDSetFramerate setFrameratePD;
	GLShaderSource fnGLShaderSource;
	GLShaderSourceARB fnGLShaderSourceARB;
	GLProgramStringARB fnGLProgramStringARB;
	GLBindProgramARB fnGLBindProgramARB;
	DNInitialize fnDNInitialize;
	DNProcessShader fnDNProcessShader;
	DNRefreshShaders fnDNRefreshShaders;
	WGlGetProcAddress wGlGetProcAddress;
	GLBindTexture fnGLBindTexture;
	GLGetError FNGlGetError;
	ReshadeRender fnReshadeRender;
	//PDGetFrameratePtr getFrameratePtrPD;

	bool usePDFrameLimit = false;
	//std::chrono::nanoseconds* PDFrameLimit;
	constexpr uint64_t FB_WIDTH_ADDRESS = 0x00000001411ABCA8;
	constexpr uint64_t FB_HEIGHT_ADDRESS = 0x00000001411ABCAC;
	constexpr uint64_t FB1_WIDTH_ADDRESS = 0x00000001411AD5F8;
	constexpr uint64_t FB1_HEIGHT_ADDRESS = 0x00000001411AD5FC;
	constexpr uint64_t FB2_WIDTH_ADDRESS = 0x0000000140EDA8E4;
	constexpr uint64_t FB2_HEIGHT_ADDRESS = 0x0000000140EDA8E8;

	constexpr uint64_t FB_RESOLUTION_WIDTH_ADDRESS = 0x00000001411ABB50;
	constexpr uint64_t FB_RESOLUTION_HEIGHT_ADDRESS = 0x00000001411ABB54;

	constexpr uint64_t UI_WIDTH_ADDRESS = 0x000000014CC621E4;
	constexpr uint64_t UI_HEIGHT_ADDRESS = 0x000000014CC621E8;

	constexpr uint64_t FB_ASPECT_RATIO = 0x0000000140FBC2E8;
	constexpr uint64_t UI_ASPECT_RATIO = 0x000000014CC621D0;

	static int moduleEquip1 = 0;
	static int moduleEquip2 = 0;
	static int btnSeEquip = 0;
	static int skinEquip = 0;

	static int fpsLimitBak = 0;
	static bool showFps = false;
	static bool showUi2 = false;
	static bool showAbout = false;
	static bool debugUi = false;
	static bool lybdbg = false;
	static int firstTime = 8000;
	static int refreshshd = 0;
	static int sfxVolume = 100;
	static int bgmVolume = 100;
	static float uiTransparency = 0.8;
	static float sleep = 0;
	static float fpsDiff = 0;
	static bool morphologicalAA = 0;
	static bool morphologicalAA2 = 0;
	static bool temporalAA = 0;
	static bool temporalAA2 = 0;
	static bool copydepth = false;

	static bool pdframerate = false;

	static bool vsync = true;
	static bool lastvsync = false;

	static bool copymodules = false;

	static bool frameratemanager = false;
	static bool frameratemanagerdbg = false;
	static float framespeed = 1.0f;
	static float frametime = 60.0f;
	static int swapinterval = 1;
	static bool force_fpslimit_vsync = false;
	static bool frm_averaged_fps = false;

	const std::string RESOLUTION_CONFIG_FILE_NAME = "graphics.ini";

	static int allocshdpos = 0;
	static int* allocshdptr[9999];
	static GLenum allocshdenum[9999];
	static int allocshdlen[9999];

	static int allocatedshaders[9999];
	static int allocatedshadercount = 0;

	static int allocshdorigpos = 0;
	static int* allocshdorigptr[9999];

	static float res_scale[1000];
	static float originalResX = 2560;
	static float originalResY = 1440;

	static std::chrono::time_point mBeginFrame = system_clock::now();
	static std::chrono::time_point prevTimeInSeconds = time_point_cast<seconds>(mBeginFrame);

	static unsigned frameCountPerSecond = 0;

	static bool initialize = false;
	static int lastmodstate = 0;

	int last_pvid = -1;
	bool pvid_init = false;

	static bool toonShader = true;
	static bool toonShader2 = false;

	static bool forcetoonShader = false;
	static bool forcetoonShader2 = false;

	static bool forceDisableToonShaderOutline = false;

	static bool shaderafthookd = false;
	static int ReShadeState = 0;
	static bool copyfb = 0;
	static int copyfbid = 0;
	static bool scaling = false;

	PFNWGLSWAPINTERVALEXTPROC       wglSwapIntervalEXT = NULL;
	PFNWGLGETSWAPINTERVALEXTPROC    wglGetSwapIntervalEXT = NULL;

	//typedef PROC(*func_wglGetProcAddress_t) (LPCSTR lpszProc);
	//static func_wglGetProcAddress_t _wglGetProcAddress;
	//func_wglGetProcAddress_t	owglGetProcAddress;

	uint64_t hookTramp = NULL;

	typedef BOOL(__stdcall* twglSwapBuffers) (_In_ HDC hDc);

	twglSwapBuffers owglSwapBuffers;

	static std::ofstream outfile;
	static bool record = false;
	static int toread = 3528;
	static double wait = 0.02;
	static bool NoCardWorkaround = false;

	enum GameState : uint32_t
	{
		GS_STARTUP,
		GS_ADVERTISE,
		GS_GAME,
		GS_DATA_TEST,
		GS_TEST_MODE,
		GS_APP_ERROR,
		GS_MAX,
	};

	enum SubGameState : uint32_t
	{
		SUB_DATA_INITIALIZE,
		SUB_SYSTEM_STARTUP,
		SUB_SYSTEM_STARTUP_ERROR,
		SUB_WARNING,
		SUB_LOGO,
		SUB_RATING,
		SUB_DEMO,
		SUB_TITLE,
		SUB_RANKING,
		SUB_SCORE_RANKING,
		SUB_CM,
		SUB_PHOTO_MODE_DEMO,
		SUB_SELECTOR,
		SUB_GAME_MAIN,
		SUB_GAME_SEL,
		SUB_STAGE_RESULT,
		SUB_SCREEN_SHOT_SEL,
		SUB_SCREEN_SHOT_RESULT,
		SUB_GAME_OVER,
		SUB_DATA_TEST_MAIN,
		SUB_DATA_TEST_MISC,
		SUB_DATA_TEST_OBJ,
		SUB_DATA_TEST_STG,
		SUB_DATA_TEST_MOT,
		SUB_DATA_TEST_COLLISION,
		SUB_DATA_TEST_SPR,
		SUB_DATA_TEST_AET,
		SUB_DATA_TEST_AUTH_3D,
		SUB_DATA_TEST_CHR,
		SUB_DATA_TEST_ITEM,
		SUB_DATA_TEST_PERF,
		SUB_DATA_TEST_PVSCRIPT,
		SUB_DATA_TEST_PRINT,
		SUB_DATA_TEST_CARD,
		SUB_DATA_TEST_OPD,
		SUB_DATA_TEST_SLIDER,
		SUB_DATA_TEST_GLITTER,
		SUB_DATA_TEST_GRAPHICS,
		SUB_DATA_TEST_COLLECTION_CARD,
		SUB_TEST_MODE_MAIN,
		SUB_APP_ERROR,
		SUB_MAX,
	};

	static bool dxgi = false;
	static bool dxgidraw = false;

	static int frames = 10;

	typedef void ChangeGameState(GameState);
	ChangeGameState* changeBaseState = (ChangeGameState*)CHANGE_MODE_ADDRESS;

	typedef void ChangeLogGameState(GameState, SubGameState);
	ChangeLogGameState* changeSubState = (ChangeLogGameState*)CHANGE_SUB_MODE_ADDRESS;

	static float defaultAetFrameDuration;
	static bool dbgAutoFramerate = true;

	GLComponent::GLComponent()
	{

	}


	GLComponent::~GLComponent()
	{

	}
	static bool dbgframerateinitialized = false;
	static bool forcedbgframerateon = false;
	void InjectCode(void* address, const std::vector<uint8_t> data)
	{
		const size_t byteCount = data.size() * sizeof(uint8_t);

		DWORD oldProtect;
		VirtualProtect(address, byteCount, PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy(address, data.data(), byteCount);
		VirtualProtect(address, byteCount, oldProtect, nullptr);
	}

	int dirExists(const char* path)
	{
		struct stat info;

		if (stat(path, &info) != 0)
			return 0;
		else if (info.st_mode & S_IFDIR)
			return 1;
		else
			return 0;
	}

	void RefreshShaders()
	{
		printf("REFRESHSHADER!");
		fnDNRefreshShaders();

		glBindFramebufferEXT(GL_FRAMEBUFFER, 0);


		for (int i = 0; i < allocatedshadercount; i++)
		{
			if (allocatedshaders[i] != 1)
			{
				int newlen = fnDNProcessShader((int)allocshdorigptr[i], (int)allocshdptr[i], allocshdlen[i], 0, 0);
				if (newlen != -1) {
					printf("Deleto");
					//glDeleteShader(allocatedshaders[i]);
					printf("glBindProgramARB");
					//glGenProgramsARB(1, (GLuint*)allocatedshaders[i]);

					printf("%d\n", allocshdenum[i]);
					printf("%d\n", allocatedshaders[i]);
					glBindProgramARB(allocshdenum[i], allocatedshaders[i]);
					printf("fnDNProcessShader");

					printf("fnGLProgramStringARB");
					fnGLProgramStringARB(allocshdenum[i], GL_PROGRAM_FORMAT_ASCII_ARB, newlen, allocshdptr[i]);
					glGetError();
					glDisable(allocshdenum[i]);
				}
				//
				/*
				glLoadProgramNV(allocshdenum[i], allocatedshaders[i], newlen, (GLubyte*)allocshdptr[i]);
				*/
			}
		}
	}

	void setFramerateDbg()
	{
		if (!dbgframerateinitialized)
		{
			// This const variable is stored inside a data segment so we don't want to throw any access violations
			DWORD oldProtect;
			VirtualProtect((void*)AET_FRAME_DURATION_ADDRESS, sizeof(float), PAGE_EXECUTE_READWRITE, &oldProtect);

			// fix auto speed for high fps
			InjectCode((void*)0x140192d7b, { 0x90, 0x90, 0x90 });

			// fix frame speed slider initial value (should ignore effect of auto speed)
			InjectCode((void*)0x140338f2f, { 0xf3, 0x0f, 0x10, 0x0d, 0x61, 0x18, 0xba, 0x00 }); // MOVSS XMM1, dword ptr [0x140eda798] (raw framespeed)
			InjectCode((void*)0x140338f37, { 0x48, 0x8b, 0x8f, 0x80, 0x01, 0x00, 0x00 });       // MOV RCX, qword ptr [0x180 + RDI]

			InjectCode((void*)0x140338ebe, { 0xf3, 0x0f, 0x10, 0x0d, 0xd2, 0x18, 0xba, 0x00 }); // MOVSS XMM1, dword ptr [0x140eda798] (raw framespeed)
			InjectCode((void*)0x140338ec6, { 0x48, 0x8b, 0x05, 0xfb, 0xb1, 0xe5, 0x00 });       // MOV RAX, qword ptr [0x1411940c8]
			InjectCode((void*)0x140338ecd, { 0x48, 0x8b, 0x88, 0x80, 0x01, 0x00, 0x00 });       // MOV RCX, qword ptr [0x180 + RAX]

			// fix AETs
			InjectCode((void*)0x140170394, { 0xF3, 0x0F, 0x5E, 0x05, 0x34, 0xA3, 0xD6, 0x00 }); // DIVSS XMM0, dword ptr [0x140eda6d0] (framerate)

			// fix edit PV AETs
			InjectCode((void*)0x140192d30, { 0xF3, 0x0F, 0x10, 0x05, 0x5C, 0x02, 0x00, 0x00 }); // MOVSS XMM0, dword ptr [0x140192f94]

			// fix ageage hair effect
			InjectCode((void*)0x14054352f, { 0xE8, 0x1C, 0xF8, 0xC4, 0xFF }); // CALL 0x140192d50 (getFrameSpeed)
			InjectCode((void*)0x140543534, { 0xF3, 0x0F, 0x59, 0xD8 });       // MULSS XMM3, XMM0
			InjectCode((void*)0x140543538, { 0xEB, 0xB6 });                   // JMP 0x1405434f0

			// fix wind effect
			InjectCode((void*)0x14053ca71, { 0xEB, 0x3F });                                     // JMP 0x14053cab2
			InjectCode((void*)0x14053cab2, { 0xF3, 0x0F, 0x10, 0x05, 0xFA, 0x53, 0x46, 0x00 }); // MOVSS XMM0, dword ptr [0x1409a1eb4] (60.0f)
			InjectCode((void*)0x14053caba, { 0xE9, 0x42, 0xFE, 0xFF, 0xFF });                   // JMP 0x14053c901
			InjectCode((void*)0x14053c901, { 0xF3, 0x0F, 0x5E, 0x05, 0xC7, 0xDD, 0x99, 0x00 }); // DIVSS XMM0, dword ptr [0x140eda6d0] (framerate)
			InjectCode((void*)0x14053c909, { 0xE9, 0x68, 0x01, 0x00, 0x00 });                   // JMP 0x14053ca76
			dbgframerateinitialized = true;
		}
		float frameRate = frametime;
		float gameFrameRate = 1000.0f / ((float)(chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f));
		if (frm_averaged_fps) gameFrameRate = ImGui::GetIO().Framerate;
		//Fix Snow
		*(float*)0x140C9A4E0 = gameFrameRate / 60.0f;
		if (*(float*)0x140C9A4E0 >= 5000.0f)* (float*)0x140C9A4E0 = 5000.0f;
		//printf("%3.2f\n", );
		//*(float*)AET_FRAME_DURATION_ADDRESS = 1.0f / 60.0f;
		*(float*)PV_FRAME_RATE_ADDRESS = frameRate;

		bool inGame = *(GameState*)CURRENT_GAME_STATE_ADDRESS == GS_GAME;
		bool inPv = *(SubGameState*)(0x140EDA82C) == SUB_GAME_MAIN;
		bool inDemo = *(SubGameState*)(0x140EDA82C) == SUB_DEMO;
		if (inGame || inPv || inDemo || forcedbgframerateon)
		{
			// During the GAME state the frame rate will be handled by the PvFrameRate instead
			if (dbgAutoFramerate)
				framespeed = frametime / gameFrameRate;
			if (framespeed > 1000)
			{
				framespeed = 1000;
				printf("[DivaImGui] Warning over 1000!\n");
			}
			float defaultFrameRate = 60.0f;

			// This PV struct creates a copy of the PvFrameRate & PvFrameSpeed during the loading screen
			// so we'll make sure to keep updating it as well.
			// Each new motion also creates its own copy of these values but keeping track of the active motions is annoying
			// and they usually change multiple times per PV anyway so this should suffice for now
			float* pvStructPvFrameRate = (float*)(0x0000000140CDD978 + 0x2BF98);
			float* pvStructPvFrameSpeed = (float*)(0x0000000140CDD978 + 0x2BF9C);

			*pvStructPvFrameRate = *(float*)PV_FRAME_RATE_ADDRESS;
			*pvStructPvFrameSpeed = (defaultFrameRate / *(float*)PV_FRAME_RATE_ADDRESS);

			*(float*)FRAME_SPEED_ADDRESS = framespeed;
		}
		else
		{
			*(float*)FRAME_SPEED_ADDRESS = *(float*)AET_FRAME_DURATION_ADDRESS / defaultAetFrameDuration;
		}
	}

	void setFramerate()
	{
		float frameRate = MainModule::fpsLimitSet;

		if ((frameRate < 19) || vsync)
		{
			//frameRate = 60.0f;
			DEVMODE dm;
			dm.dmSize = sizeof(DEVMODE);

			EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm);
			frameRate = (float)dm.dmDisplayFrequency;
		}

		*(float*)AET_FRAME_DURATION_ADDRESS = 1.0f / frameRate;
		*(float*)PV_FRAME_RATE_ADDRESS = frameRate;
		//printf("%.2", *(float*)PV_FRAME_RATE_ADDRESS);
		bool inGame = *(GameState*)CURRENT_GAME_STATE_ADDRESS == GS_GAME;
		if (inGame)
		{
			// During the GAME state the frame rate will be handled by the PvFrameRate instead

			constexpr float defaultFrameSpeed = 1.0f;
			constexpr float defaultFrameRate = 60.0f;

			// This PV struct creates a copy of the PvFrameRate & PvFrameSpeed during the loading screen
			// so we'll make sure to keep updating it as well.
			// Each new motion also creates its own copy of these values but keeping track of the active motions is annoying
			// and they usually change multiple times per PV anyway so this should suffice for now
			float* pvStructPvFrameRate = (float*)(0x0000000140CDD978 + 0x2BF98);
			float* pvStructPvFrameSpeed = (float*)(0x0000000140CDD978 + 0x2BF9C);

			*pvStructPvFrameRate = *(float*)PV_FRAME_RATE_ADDRESS;
			*pvStructPvFrameSpeed = (defaultFrameRate / *(float*)PV_FRAME_RATE_ADDRESS);

			*(float*)FRAME_SPEED_ADDRESS = defaultFrameSpeed;
		}
		else
		{
			*(float*)FRAME_SPEED_ADDRESS = *(float*)AET_FRAME_DURATION_ADDRESS / defaultAetFrameDuration;
		}
	}

	static int hlastX = 0;
	static int hlastY = 0;

	void ScaleFb()
	{
		RECT hWindow;
		GetClientRect(DivaImGui::MainModule::DivaWindowHandle, &hWindow);

		int curX = (hWindow.right - hWindow.left);
		int curY = (hWindow.bottom - hWindow.top);

		//if ((hlastX != curX) || (hlastY != curY))
		{
			float* uiAspectRatio = (float*)UI_ASPECT_RATIO;
			float* uiWidth = (float*)UI_WIDTH_ADDRESS;
			float* uiHeight = (float*)UI_HEIGHT_ADDRESS;
			int* fb1Height = (int*)FB1_HEIGHT_ADDRESS;
			int* fb1Width = (int*)FB1_WIDTH_ADDRESS;
			double* fbAspectRatio = (double*)FB_ASPECT_RATIO;


			*uiAspectRatio = (float)(hWindow.right - hWindow.left) / (float)(hWindow.bottom - hWindow.top);
			*fbAspectRatio = (double)(hWindow.right - hWindow.left) / (double)(hWindow.bottom - hWindow.top);
			*uiWidth = hWindow.right - hWindow.left;
			*uiHeight = hWindow.bottom - hWindow.top;
			*fb1Width = hWindow.right - hWindow.left;
			*fb1Height = hWindow.bottom - hWindow.top;

			*((int*)0x00000001411AD608) = 0;

			*((int*)0x0000000140EDA8E4) = *(int*)0x0000000140EDA8BC;
			*((int*)0x0000000140EDA8E8) = *(int*)0x0000000140EDA8C0;

			*(float*)0x00000001411A1900 = 0;
			*(float*)0x00000001411A1904 = (float) * (int*)0x0000000140EDA8BC;
			*(float*)0x00000001411A1908 = (float) * (int*)0x0000000140EDA8C0;


			hlastX = curX;
			hlastY = curY;
		}
	}
	//SystemClass* System;

	/*
	void InitializeDX11Window()
	{
		bool result;

		// Create the system object.
		//System = new SystemClass;
		//if (!System)
		{
			printf("dx11: NG");
			return;
		}
		printf("dx11: Initializedd");
		// Initialize and run the system object.
		result = System->Initialize();
	}
	*/



	void InitializeImGui()
	{
		//if (WGLExtensionSupported("WGL_EXT_swap_control"))
		{
			// Extension is supported, init pointers.
			wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

			// this is another function from WGL_EXT_swap_control extension
			wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");


		}

		getFrameratePD = (PDGetFramerate)GetProcAddress(GetModuleHandle(L"Render.dva"), "getFramerateLimit");
		if (getFrameratePD != NULL)
		{
			setFrameratePD = (PDSetFramerate)GetProcAddress(GetModuleHandle(L"Render.dva"), "setFramerateLimit");
			//getFrameratePtrPD = (PDGetFrameratePtr)GetProcAddress(GetModuleHandle(L"Render.dva"), "getFrameratePtr");
			if (getFrameratePD() > 0)
			{
				usePDFrameLimit = true;
				//PDFrameLimit = getFrameratePtrPD();
				printf("[DivaImGui] Using render.dva FPS Limit.\n");
			}
		}
		else printf("[DivaImGui] Warning: Possibly old version of PD-Loader.\n");
		if (!usePDFrameLimit)
			printf("[DivaImGui] Using internal FPS Limit.\n");

		DWORD oldProtect;
		VirtualProtect((void*)AET_FRAME_DURATION_ADDRESS, sizeof(float), PAGE_EXECUTE_READWRITE, &oldProtect);

		defaultAetFrameDuration = *(float*)AET_FRAME_DURATION_ADDRESS;

		fprintf(stdout, "[DivaImGui] Using GLEW %s\n", glewGetString(GLEW_VERSION));

		DivaImGui::FileSystem::ConfigFile resolutionConfig(MainModule::GetModuleDirectory(), RESOLUTION_CONFIG_FILE_NAME.c_str());
		bool success = resolutionConfig.OpenRead();
		if (!success)
		{
			printf("[DivaImGui] Unable to parse %s\n", RESOLUTION_CONFIG_FILE_NAME.c_str());
		}

		if (success) {
			std::string trueString = "1";
			std::string* value;
			if (resolutionConfig.TryGetValue("fpsLimit", &value))
			{
				DivaImGui::MainModule::fpsLimitSet = std::stoi(*value);
			}
			if (resolutionConfig.TryGetValue("MLAA", &value))
			{
				if (*value == trueString)
					morphologicalAA = true;
			}
			if (resolutionConfig.TryGetValue("TAA", &value))
			{
				if (*value == trueString)
					temporalAA = true;
			}
			if (resolutionConfig.TryGetValue("frm", &value))
			{
				if (*value == trueString)
					frameratemanager = true;

				if (*value == "2")
					frameratemanagerdbg = true;
			}
			if (resolutionConfig.TryGetValue("motifrm", &value))
			{
				frametime = std::stoi(*value);
			}
			if (resolutionConfig.TryGetValue("toonShaderWorkaround", &value))
			{
				if (*value == trueString)
					toonShader = true;
			}
			if (resolutionConfig.TryGetValue("ReShadeState", &value))
			{
				ReShadeState = std::stoi(*value);
			}
			if (resolutionConfig.TryGetValue("NoCardWorkaround", &value))
			{
				if (*value == trueString)
					NoCardWorkaround = true;
			}
			if (resolutionConfig.TryGetValue("forceToonShader", &value))
			{
				if (*value == trueString)
					forcetoonShader = true;
			}
			if (resolutionConfig.TryGetValue("forceDisbleToonShaderOutline", &value))
			{
				if (*value == trueString)
					forceDisableToonShaderOutline = true;
			}
			if (resolutionConfig.TryGetValue("forcevsyncfpslimit", &value))
			{
				if (*value == trueString)
					force_fpslimit_vsync = true;
			}
			if (resolutionConfig.TryGetValue("fbWidth", &value))
			{
				*(int*)FB_RESOLUTION_WIDTH_ADDRESS = std::stoi(*value);
			}
			if (resolutionConfig.TryGetValue("fbHeight", &value))
			{
				*(int*)FB_RESOLUTION_HEIGHT_ADDRESS = std::stoi(*value);
			}
			if (resolutionConfig.TryGetValue("depthCopy", &value))
			{
				if (*value == trueString)
					copydepth = true;
			}
			if (resolutionConfig.TryGetValue("disableDof", &value))
			{
				if (*value == trueString)
					* (int*)0x00000001411AB650 = 1;
			}
			if (resolutionConfig.TryGetValue("showFps", &value))
			{
				if (*value == trueString)
					showFps = true;
			}
			if (resolutionConfig.TryGetValue("scaling", &value))
			{
				if (*value == trueString)
					scaling = true;
			}
			if (resolutionConfig.TryGetValue("dbg", &value))
			{
				if (*value == trueString)
					lybdbg = true;
			}
			if (resolutionConfig.TryGetValue("Vsync", &value))
			{
				if (*value == trueString)
					vsync = true;
				else
				{
					vsync = false;
					lastvsync = true;
					fpsLimitBak = DivaImGui::MainModule::fpsLimitSet;
				}
			}
			if (resolutionConfig.TryGetValue("glswapinterval", &value))
			{
				swapinterval = std::stoi(*value);
			}
			if (resolutionConfig.TryGetValue("customRes", &value))
			{
				if (*value == trueString)
				{
					int maxWidth = 2560;
					int maxHeight = 1440;
					printf("\n");
					if (resolutionConfig.TryGetValue("maxWidth", &value))
					{
						maxWidth = std::stoi(*value);
						printf(value->c_str());
					}
					printf("x");
					if (resolutionConfig.TryGetValue("maxHeight", &value))
					{
						maxHeight = std::stoi(*value);
						printf(value->c_str());
					}
					{
						DWORD oldProtect, bck;
						VirtualProtect((BYTE*)0x00000001409B8B68, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
						*((int*)0x00000001409B8B68) = maxWidth;
						VirtualProtect((BYTE*)0x00000001409B8B68, 6, oldProtect, &bck);
					}
					{
						DWORD oldProtect, bck;
						VirtualProtect((BYTE*)0x00000001409B8B6C, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
						*((int*)0x00000001409B8B6C) = maxHeight;
						VirtualProtect((BYTE*)0x00000001409B8B6C, 6, oldProtect, &bck);
					}
				}
			}
		}

		for (int i = 0; i < 1000; i++)
		{
			res_scale[i] = -1.0f;
		}

		originalResX = *(int*)FB_RESOLUTION_HEIGHT_ADDRESS;
		originalResY = *(int*)FB_RESOLUTION_WIDTH_ADDRESS;

		if (scaling)
		{
			{
				DWORD oldProtect, bck;
				VirtualProtect((BYTE*)0x00000001404ACD24, 7, PAGE_EXECUTE_READWRITE, &oldProtect);
				*((BYTE*)0x00000001404ACD24 + 0) = 0x44;
				*((BYTE*)0x00000001404ACD24 + 1) = 0x8B;
				*((BYTE*)0x00000001404ACD24 + 2) = 0x0D;
				*((BYTE*)0x00000001404ACD24 + 3) = 0xD1;
				*((BYTE*)0x00000001404ACD24 + 4) = 0x08;
				*((BYTE*)0x00000001404ACD24 + 5) = 0xD0;
				*((BYTE*)0x00000001404ACD24 + 6) = 0x00;
				VirtualProtect((BYTE*)0x00000001404ACD24, 7, oldProtect, &bck);
			}
			{
				DWORD oldProtect, bck;
				VirtualProtect((BYTE*)0x00000001404ACD2B, 7, PAGE_EXECUTE_READWRITE, &oldProtect);
				*((BYTE*)0x00000001404ACD2B + 0) = 0x44;
				*((BYTE*)0x00000001404ACD2B + 1) = 0x8B;
				*((BYTE*)0x00000001404ACD2B + 2) = 0x05;
				*((BYTE*)0x00000001404ACD2B + 3) = 0xC6;
				*((BYTE*)0x00000001404ACD2B + 4) = 0x08;
				*((BYTE*)0x00000001404ACD2B + 5) = 0xD0;
				*((BYTE*)0x00000001404ACD2B + 6) = 0x00;
				VirtualProtect((BYTE*)0x00000001404ACD2B, 7, oldProtect, &bck);
			}
			{
				DWORD oldProtect, bck;
				VirtualProtect((BYTE*)0x00000001405030A0, 6, PAGE_EXECUTE_READWRITE, &oldProtect);
				*((BYTE*)0x00000001405030A0 + 0) = 0x90;
				*((BYTE*)0x00000001405030A0 + 1) = 0x90;
				*((BYTE*)0x00000001405030A0 + 2) = 0x90;
				*((BYTE*)0x00000001405030A0 + 3) = 0x90;
				*((BYTE*)0x00000001405030A0 + 4) = 0x90;
				*((BYTE*)0x00000001405030A0 + 5) = 0x90;
				VirtualProtect((BYTE*)0x00000001405030A0, 6, oldProtect, &bck);
			}

		}

		std::ifstream f("res_scale.csv");
		if (f.good())
		{
			aria::csv::CsvParser parser(f);
			int rowNum = 0;
			int fieldNum = 0;
			int currentPvId = 0;
			for (auto& row : parser) {
				currentPvId = 999;
				for (auto& field : row) {
					if (fieldNum == 0)
						currentPvId = std::stoi(field);
					if (fieldNum == 1)
						res_scale[currentPvId] = std::stof(field);
					fieldNum++;
				}
				fieldNum = 0;
				rowNum++;
			}
		}

		//reput back original value
		{
			DWORD oldProtect, bck;
			VirtualProtect((BYTE*)0x000000014050214F, 2, PAGE_EXECUTE_READWRITE, &oldProtect);
			*((BYTE*)0x000000014050214F + 0) = 0x74;
			*((BYTE*)0x000000014050214F + 1) = 0x18;
			VirtualProtect((BYTE*)0x000000014050214F, 2, oldProtect, &bck);

		}

		{
			DWORD oldProtect, bck;
			VirtualProtect((BYTE*)0x0000000140641102, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
			*((BYTE*)0x0000000140641102 + 0) = 0x00;
			VirtualProtect((BYTE*)0x0000000140641102, 1, oldProtect, &bck);
		}

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.WantCaptureKeyboard == true;

		ImGui_ImplWin32_Init(MainModule::DivaWindowHandle);
		ImGui_ImplOpenGL2_Init();
		ImGui::StyleColorsDark();
		start = high_resolution_clock::now();
		initialize = true;
	}

	void Update()
	{
		int* pvid = (int*)0x00000001418054C4;

		if (NoCardWorkaround)
		{
			if (*(int*)0x00000001411A8850 != 1)
			{
				//copy to card
				*(int*)0x00000001411A8A28 = *(int*)0x00000001411A8A10;
				*(int*)(0x00000001411A8A28 + 4) = *(int*)(0x00000001411A8A28 + 4);
				*(int*)(0x00000001411A8A28 + 8) = *(int*)(0x00000001411A8A28 + 8);
				*(int*)(0x00000001411A8A28 + 12) = *(int*)(0x00000001411A8A28 + 12);
				*(int*)(0x00000001411A8A28 + 16) = *(int*)(0x00000001411A8A28 + 16);
				*(int*)(0x00000001411A8A28 + 18) = *(int*)(0x00000001411A8A28 + 18);

				//if (*(int*)0x00000001418034FC == 0)
				//	*(int*)0x00000001411A9790 = 0;
				//else *(int*)0x00000001411A9790 = 1;

				if (pvid_init == false)
				{
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x00000001405CBBA3, 8, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x00000001405CBBA3 + 0) = 0x90;
					*((BYTE*)0x00000001405CBBA3 + 1) = 0x90;
					*((BYTE*)0x00000001405CBBA3 + 2) = 0x90;
					*((BYTE*)0x00000001405CBBA3 + 3) = 0x90;
					*((BYTE*)0x00000001405CBBA3 + 4) = 0x90;
					*((BYTE*)0x00000001405CBBA3 + 5) = 0x90;
					*((BYTE*)0x00000001405CBBA3 + 6) = 0x90;
					*((BYTE*)0x00000001405CBBA3 + 7) = 0x90;
					VirtualProtect((BYTE*)0x00000001405CBBA3, 8, oldProtect, &bck);
					pvid_init = true;
				}

				if (*pvid != last_pvid)
				{
					if (res_scale[*pvid] != -1.0f) {
						if ((*pvid > 0) && (*pvid < 999))
						{
							*(int*)FB_RESOLUTION_HEIGHT_ADDRESS = originalResX * res_scale[*pvid];
							*(int*)FB_RESOLUTION_WIDTH_ADDRESS = originalResY * res_scale[*pvid];
						}
					}
					else {
						*(int*)FB_RESOLUTION_HEIGHT_ADDRESS = originalResX;
						*(int*)FB_RESOLUTION_WIDTH_ADDRESS = originalResY;
					}

					pvid_init = false;
					last_pvid = *pvid;
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x00000001405CBBA3, 8, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x00000001405CBBA3 + 0) = 0x42;
					*((BYTE*)0x00000001405CBBA3 + 1) = 0x89;
					*((BYTE*)0x00000001405CBBA3 + 2) = 0x84;
					*((BYTE*)0x00000001405CBBA3 + 3) = 0xb6;
					*((BYTE*)0x00000001405CBBA3 + 4) = 0xc0;
					*((BYTE*)0x00000001405CBBA3 + 5) = 0x01;
					*((BYTE*)0x00000001405CBBA3 + 6) = 0x00;
					*((BYTE*)0x00000001405CBBA3 + 7) = 0x00;
					VirtualProtect((BYTE*)0x00000001405CBBA3, 8, oldProtect, &bck);
				}
			}
			else {

				if (pvid_init == false)
				{
					if (copymodules) {
						*(int*)0x00000001411A8A28 = *(int*)0x00000001411A8A10;
						*(int*)0x00000001411A8A2C = *(int*)0x00000001411A8A14;
						*(int*)0x00000001411A8A30 = *(int*)0x00000001411A8A18;
						*(int*)0x00000001411A8A34 = *(int*)0x00000001411A8A1C;
						*(int*)0x00000001411A8A38 = *(int*)0x00000001411A8A20;
						*(int*)0x00000001411A8A3C = *(int*)0x00000001411A8A24;
					}
					pvid_init = true;
				}

				if ((lastmodstate == 1) && (*(int*)0x00000001411A9790 == 0) && (copymodules))
				{
					*(int*)0x00000001411A8A28 = *(int*)0x00000001411A8A10;
					*(int*)0x00000001411A8A2C = *(int*)0x00000001411A8A14;
					*(int*)0x00000001411A8A30 = *(int*)0x00000001411A8A18;
					*(int*)0x00000001411A8A34 = *(int*)0x00000001411A8A1C;
					*(int*)0x00000001411A8A38 = *(int*)0x00000001411A8A20;
					*(int*)0x00000001411A8A3C = *(int*)0x00000001411A8A24;
				}
				lastmodstate = *(int*)0x00000001411A9790;

				if (*pvid != last_pvid)
				{
					if (res_scale[*pvid] != -1.0f) {
						if ((*pvid > 0) && (*pvid < 999))
						{
							*(int*)FB_RESOLUTION_HEIGHT_ADDRESS = originalResX * res_scale[*pvid];
							*(int*)FB_RESOLUTION_WIDTH_ADDRESS = originalResY * res_scale[*pvid];
						}
					}
					else {
						*(int*)FB_RESOLUTION_HEIGHT_ADDRESS = originalResX;
						*(int*)FB_RESOLUTION_WIDTH_ADDRESS = originalResY;
					}

					pvid_init = false;
					last_pvid = *pvid;
				}

			}
		}
		else
		{

			if (*pvid != last_pvid)
			{
				if (res_scale[*pvid] != -1.0f) {
					if ((*pvid > 0) && (*pvid < 999))
					{
						*(int*)FB_RESOLUTION_HEIGHT_ADDRESS = originalResX * res_scale[*pvid];
						*(int*)FB_RESOLUTION_WIDTH_ADDRESS = originalResY * res_scale[*pvid];
					}
				}
				else {
					*(int*)FB_RESOLUTION_HEIGHT_ADDRESS = originalResX;
					*(int*)FB_RESOLUTION_WIDTH_ADDRESS = originalResY;
				}

				last_pvid = *pvid;
			}

		}


		if (temporalAA)
		{
			DWORD oldProtect, bck;
			VirtualProtect((BYTE*)0x00000001411AB67C, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
			*((BYTE*)0x00000001411AB67C + 0) = 0x01;
			VirtualProtect((BYTE*)0x00000001411AB67C, 1, oldProtect, &bck);
		}
		else {
			DWORD oldProtect, bck;
			VirtualProtect((BYTE*)0x00000001411AB67C, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
			*((BYTE*)0x00000001411AB67C + 0) = 0x00;
			VirtualProtect((BYTE*)0x00000001411AB67C, 1, oldProtect, &bck);
		}

		if (morphologicalAA)
		{
			DWORD oldProtect, bck;
			VirtualProtect((BYTE*)0x00000001411AB680, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
			*((BYTE*)0x00000001411AB680 + 0) = 0x01;
			VirtualProtect((BYTE*)0x00000001411AB680, 1, oldProtect, &bck);
		}
		else {
			DWORD oldProtect, bck;
			VirtualProtect((BYTE*)0x00000001411AB680, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
			*((BYTE*)0x00000001411AB680 + 0) = 0x00;
			VirtualProtect((BYTE*)0x00000001411AB680, 1, oldProtect, &bck);
		}

		if (toonShader)
		{
			if (forceDisableToonShaderOutline)
			{
				InjectCode((void*)0x14CC924E2, { 0x00 });
			}
			else {
				InjectCode((void*)0x14CC924E2, { 0x01 });
			}
		}
		else InjectCode((void*)0x14CC924E2, { 0x01 });

		if (toonShader)
		{
			if (!toonShader2)
			{
				InjectCode((void*)0x1406410f6, { 0x0F, 0x2F, 0xC0, 0x90, 0x90, 0x90, 0x90 });
				{
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x000000014050214F, 2, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x000000014050214F + 0) = 0x90;
					*((BYTE*)0x000000014050214F + 1) = 0x90;
					VirtualProtect((BYTE*)0x000000014050214F, 2, oldProtect, &bck);

				}
				/*
				{
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x0000000140641102, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x0000000140641102 + 0) = 0x01;
					VirtualProtect((BYTE*)0x0000000140641102, 1, oldProtect, &bck);

				}
				*/
				toonShader2 = true;
			}
		}
		else {
			if (toonShader2)
			{
				InjectCode((void*)0x1406410f6, { 0x0F, 0x2F, 0x05, 0xEB, 0x55, 0x36, 0x00 });
				{
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x000000014050214F, 2, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x000000014050214F + 0) = 0x74;
					*((BYTE*)0x000000014050214F + 1) = 0x18;
					VirtualProtect((BYTE*)0x000000014050214F, 2, oldProtect, &bck);

				}
				/*
				{
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x0000000140641102, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x0000000140641102 + 0) = 0x00;
					VirtualProtect((BYTE*)0x0000000140641102, 1, oldProtect, &bck);

				}
				*/
				toonShader2 = false;
			}
		}

		if (forcetoonShader)
		{
			if (!forcetoonShader2)
			{
				{
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x0000000140502FC0, 6, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x0000000140502FC0 + 0) = 0x90;
					*((BYTE*)0x0000000140502FC0 + 1) = 0x90;
					*((BYTE*)0x0000000140502FC0 + 2) = 0x90;
					*((BYTE*)0x0000000140502FC0 + 3) = 0x90;
					*((BYTE*)0x0000000140502FC0 + 4) = 0x90;
					*((BYTE*)0x0000000140502FC0 + 5) = 0x90;
					VirtualProtect((BYTE*)0x0000000140502FC0, 6, oldProtect, &bck);
				}

				{
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x00000001411AD638, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x00000001411AD638 + 0) = 0x01;
					VirtualProtect((BYTE*)0x00000001411AD638, 1, oldProtect, &bck);
				}

				forcetoonShader2 = true;
			}
		}
		else {
			if (forcetoonShader2)
			{
				{
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x0000000140502FC0, 6, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x0000000140502FC0 + 0) = 0x89;
					*((BYTE*)0x0000000140502FC0 + 1) = 0x0D;
					*((BYTE*)0x0000000140502FC0 + 2) = 0x72;
					*((BYTE*)0x0000000140502FC0 + 3) = 0xA6;
					*((BYTE*)0x0000000140502FC0 + 4) = 0xCA;
					*((BYTE*)0x0000000140502FC0 + 5) = 0x00;
					VirtualProtect((BYTE*)0x0000000140502FC0, 6, oldProtect, &bck);
				}

				forcetoonShader2 = false;
			}
		}
	}

	static bool enablesprites = true;

	void RenderGUI()
	{
		auto keyboard = DivaImGui::Input::Keyboard::GetInstance();
		keyboard->PollInput();
		if (!initialize)
		{
			MainModule::DivaWindowHandle = FindWindow(0, MainModule::DivaWindowName);
			if (MainModule::DivaWindowHandle == NULL)
				MainModule::DivaWindowHandle = FindWindow(0, MainModule::GlutDefaultName);

			if (MainModule::DivaWindowHandle == NULL)
				MainModule::DivaWindowHandle = FindWindow(0, MainModule::freeGlutDefaultName);

			if (MainModule::DivaWindowHandle == NULL)
				return;

			InitializeImGui();
		}

		Update();

		if (scaling)
		{
			ScaleFb();
		}

		int* fbWidth = (int*)FB_RESOLUTION_WIDTH_ADDRESS;
		int* fbHeight = (int*)FB_RESOLUTION_HEIGHT_ADDRESS;

		RECT hWindow;
		GetClientRect(DivaImGui::MainModule::DivaWindowHandle, &hWindow);
		long uiWidth = hWindow.right - hWindow.left;
		long uiHeight = hWindow.bottom - hWindow.top;

		if (copydepth)
		{
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER, 3);
			glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebufferEXT(0, 0, *fbWidth, *fbHeight, 0, 0, uiWidth, uiHeight,
				GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		}

		ImGui::SetNextWindowBgAlpha(uiTransparency);

		ImGui_ImplWin32_NewFrame();
		ImGui_ImplOpenGL2_NewFrame();
		ImGui::NewFrame();

		ImGuiIO& io = ImGui::GetIO();

		io.MouseDown[0] = keyboard->IsDown(VK_LBUTTON);
		io.MouseDown[1] = keyboard->IsDown(VK_RBUTTON);
		io.MouseDown[2] = keyboard->IsDown(VK_MBUTTON);

		if (((keyboard->IsDown(VK_CONTROL)) && (keyboard->IsDown(VK_LSHIFT)) && (keyboard->IsTapped(VK_BACK))))
		{
			if (MainModule::showUi) { MainModule::showUi = false; showUi2 = false; }
			else MainModule::showUi = true;
		}

		int i = 48;
		while (i != 58)
		{
			if (keyboard->IsTapped(i))
			{
				io.AddInputCharacter(i);
			}
			i++;
		}

		bool p_open = false;

		if (firstTime > 0)
		{
			ImGuiWindowFlags window_flags = 0;
			window_flags |= ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoResize;
			window_flags |= ImGuiWindowFlags_NoTitleBar;
			window_flags |= ImGuiWindowFlags_NoCollapse;
			window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
			ImGui::Begin("ELAC", &p_open, window_flags);
			ImGui::SetWindowPos(ImVec2(0, 0));
			ImGui::Text("Press Ctrl+LShift+Backspace to show/hide UI.");
			ImGui::End();
		}

		ImGuiWindowFlags window_flags = 0;
		window_flags |= ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoResize;
		window_flags |= ImGuiWindowFlags_NoTitleBar;
		window_flags |= ImGuiWindowFlags_NoCollapse;
		window_flags |= ImGuiWindowFlags_AlwaysAutoResize;

		if (MainModule::showUi) {
			if (ImGui::IsMouseHoveringWindow)
				MainModule::inputDisable = true;
			ImGui::SetNextWindowBgAlpha(uiTransparency);
			ImGuiWindowFlags window_flags = 0;
			window_flags |= ImGuiWindowFlags_NoResize;
			window_flags |= ImGuiWindowFlags_NoCollapse;
			window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			ImGui::Begin("DivaHook Config", &MainModule::showUi, window_flags);
			if (*(int*)0x00000001411A8850 == 1)
			{
				if (ImGui::CollapsingHeader("Modules and Custom Skins/Sounds (Card)"))
				{
					ImGui::Text("--- Changes only takes effect after entering a new stage. ---");
					ImGui::InputInt("Module 1 ID", (int*)0x00000001411A8A28);
					ImGui::InputInt("Module 2 ID", (int*)0x00000001411A8A2C);
					ImGui::InputInt("Module 3 ID", (int*)0x00000001411A8A30);
					ImGui::InputInt("Module 4 ID", (int*)0x00000001411A8A34);
					ImGui::InputInt("Module 5 ID", (int*)0x00000001411A8A38);
					ImGui::InputInt("Module 6 ID", (int*)0x00000001411A8A3C);
					ImGui::InputInt("HUD Skin ID", (int*)0x00000001411A8D98);
					ImGui::InputInt("Level Plate Skin ID", (int*)0x00000001411A8974);
					ImGui::InputInt("Level Plate Skin SFX", (int*)0x00000001411A8978);
					ImGui::Checkbox("Copy Default Modules to Card", &copymodules);
				}

				if (ImGui::CollapsingHeader("Modules (Read Only)"))
				{
					ImGui::Text("--- For viewing purposes. ---");
					ImGui::InputInt("Module 1 ID", (int*)0x00000001411A8A10);
					ImGui::InputInt("Module 2 ID", (int*)0x00000001411A8A14);
					ImGui::InputInt("Module 3 ID", (int*)0x00000001411A8A18);
					ImGui::InputInt("Module 4 ID", (int*)0x00000001411A8A1C);
					ImGui::InputInt("Module 5 ID", (int*)0x00000001411A8A20);
					ImGui::InputInt("Module 6 ID", (int*)0x00000001411A8A24);
				}

			}
			else {
				if (ImGui::CollapsingHeader("Modules and Custom Skins/Sounds"))
				{
					ImGui::Text("--- Changes only takes effect after entering a new stage. ---");
					ImGui::InputInt("Module 1 ID", (int*)0x00000001411A8A10);
					ImGui::InputInt("Module 2 ID", (int*)0x00000001411A8A14);
					ImGui::InputInt("Module 3 ID", (int*)0x00000001411A8A18);
					ImGui::InputInt("Module 4 ID", (int*)0x00000001411A8A1C);
					ImGui::InputInt("Module 5 ID", (int*)0x00000001411A8A20);
					ImGui::InputInt("Module 6 ID", (int*)0x00000001411A8A24);
					ImGui::InputInt("HUD Skin ID", (int*)0x00000001411A8D98);
					ImGui::InputInt("Level Plate Skin ID", (int*)0x00000001411A8974);
					ImGui::InputInt("Level Plate Skin SFX", (int*)0x00000001411A8978);
				}
			}

			if (ImGui::CollapsingHeader("Internal Resolution"))
			{
				ImGui::InputInt("Resolution Width", fbWidth);
				ImGui::InputInt("Resolution Height", fbHeight);
			}
			if (ImGui::CollapsingHeader("Framerate"))
			{
				if (wglGetSwapIntervalEXT() == 0 || force_fpslimit_vsync == true)
				{
					ImGui::Text("--- Set the FPS cap to 0 only if you have vsync. ---");
					ImGui::InputInt("Framerate Cap", &DivaImGui::MainModule::fpsLimitSet);
				}
				else {
					ImGui::Text("--- Disable vsync to use framelimiter ---");
				}
			}
			if (ImGui::CollapsingHeader("Graphics settings"))
			{
				ImGui::Text("--- Display ---");
				ImGui::Checkbox("Vsync", &vsync);
				ImGui::InputInt("Swap Interval", &swapinterval);
				ImGui::Text("--- Anti-Aliasing ---");
				ImGui::Checkbox("TAA (Temporal AA)", &temporalAA);
				ImGui::Checkbox("MLAA (Morphological AA)", &morphologicalAA);
				ImGui::Text("--- Bug Fixes ---");
				ImGui::Checkbox("Toon Shader (When Running with: -hdtv1080/-aa)", &toonShader);
				ImGui::Text("--- Misc ---");
				ImGui::Checkbox("Force Toon Shader", &forcetoonShader);
				if (toonShader)
					ImGui::Checkbox("Force Disable Toon Shader Outline", &forceDisableToonShaderOutline);
				if (*fnReshadeRender != nullptr)
				{
					ImGui::Text("--- ReShade ---");
					ImGui::SliderInt("ReShade Render Pass", &ReShadeState, 0 , 1);
				}
				ImGui::Checkbox("Sprites", &enablesprites);

				if (enablesprites)
				{
					*(int*)0x00000001411AD328 = 0x1010001;
				}
				else {
					*(int*)0x00000001411AD328 = 0x10001;
				}

				if (ImGui::CollapsingHeader("DOF settings"))
				{
					ImGui::RadioButton("Default", (int*)0x00000001411AB650, 0); ImGui::SameLine();
					ImGui::RadioButton("Force Off", (int*)0x00000001411AB650, 1); ImGui::SameLine();
					ImGui::RadioButton("Autofocus (1P)", (int*)0x00000001411AB650, 15);
					if (*(int*)0x00000001411AB650 == 15)
					{
						if (ImGui::CollapsingHeader("Autofocus Settings"))
						{
							//ImGui::SliderFloat("distance to Focus[m]", (float*)0x00000001411AB654, 0.01f, 30.0f);
							ImGui::SliderFloat("focal length", (float*)0x00000001411AB658, 0.0001f, 0.1f);
							ImGui::SliderFloat("F-number", (float*)0x00000001411AB65C, 0.1f, 40.0f);
						}
					}
					//ImGui::RadioButton("lyb's Autofocus", (int*)0x00000001411AB650, 2);
					//TODO: AutoFocus based on Camera FOV
				}
			}
			if (ImGui::CollapsingHeader("Sound Settings"))
			{
				ImGui::SliderInt("HP Volume", (int*)0x00000001411A8980, 0, 100);
				ImGui::SliderInt("ACT Volume", (int*)0x00000001411A8988, 0, 100);
				ImGui::SliderInt("SLIDER Volume", (int*)0x00000001411A898C, 0, 100);
			}
			if (ImGui::CollapsingHeader("UI Settings"))
			{
				ImGui::SliderFloat("UI Transparency", &uiTransparency, 0, 1.0);
				ImGui::Checkbox("Framerate Overlay", &showFps);
			}
			if (ImGui::Button("Close")) { MainModule::showUi = false; }; ImGui::SameLine();
			//if (ImGui::Button("Reset")) { resetGameUi = true; }; ImGui::SameLine();
			if (ImGui::Button("About")) { showAbout = true; } ImGui::SameLine();
			//if (ImGui::Button("Camera")) { cameraUi = true; } ImGui::SameLine();
			if (lybdbg)
				if (ImGui::Button("Debug")) { debugUi = true; } ImGui::SameLine();
			ImGui::End();
		}
		else {
			MainModule::inputDisable = false;
		}

		if (debugUi)
		{
			ImGui::SetNextWindowBgAlpha(uiTransparency);
			ImGuiWindowFlags window_flags = 0;
			window_flags |= ImGuiWindowFlags_NoResize;
			window_flags |= ImGuiWindowFlags_NoCollapse;
			window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			ImGui::Begin("Debug Ui", &debugUi, window_flags);
			if (ImGui::CollapsingHeader("SHADERS"))
			{
				ImGui::Checkbox("CopyFB", &copyfb);
				ImGui::InputInt("copyfbid", &copyfbid);

				if (copyfb)
				{
					glDisable(GL_SCISSOR_TEST);
					glDisable(GL_FRAMEBUFFER_SRGB);
					///glBindFramebufferEXT(GL_READ_FRAMEBUFFER, copyfbid);
					//glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
					//glReadBuffer(GL_BACK);
					//glDrawBuffer(GL_COLOR_ATTACHMENT0);
					//glBlitFramebufferEXT(0, 0, 1280, 720, 0, 0, 640, 360, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				}
				if (ImGui::Button("Refresh Shaders")) { refreshshd = 1; }
				ImGui::InputInt("Allocated Shader Count", &allocatedshadercount);

			}
			if (ImGui::CollapsingHeader("DATA TEST"))
			{
				if (ImGui::Button("GS_DATA_TEST")) {
					changeBaseState(GS_DATA_TEST);
				}
				if (ImGui::Button("SUB_DATA_TEST_OBJ")) {
					changeSubState(GS_DATA_TEST, SUB_DATA_TEST_ITEM);
				}; ImGui::SameLine();
				if (ImGui::Button("SUB_DATA_TEST_STG")) {
					changeSubState(GS_DATA_TEST, SUB_DATA_TEST_STG);
				}; ImGui::SameLine();
				if (ImGui::Button("SUB_DATA_TEST_MOT")) {
					changeSubState(GS_DATA_TEST, SUB_DATA_TEST_MOT);
				};

				if (ImGui::Button("SUB_DATA_TEST_AUTH_3D")) {
					changeSubState(GS_DATA_TEST, SUB_DATA_TEST_AUTH_3D);
				}; ImGui::SameLine();
				if (ImGui::Button("SUB_DATA_TEST_CHR")) {
					changeSubState(GS_DATA_TEST, SUB_DATA_TEST_CHR);
				}; ImGui::SameLine();
				if (ImGui::Button("SUB_DATA_TEST_ITEM")) {
					changeSubState(GS_DATA_TEST, SUB_DATA_TEST_ITEM);
				};
			}
			if (ImGui::CollapsingHeader("dbg"))
			{
				if (ImGui::CollapsingHeader("FramerateManager"))
				{
					ImGui::Checkbox("FramerateManager1", &frameratemanager);
					ImGui::Checkbox("FramerateManager2", &frameratemanagerdbg);
					if (frameratemanagerdbg)
					{
						ImGui::Checkbox("DbgAutoFramerate", &dbgAutoFramerate);
						ImGui::Checkbox("DbgForceFramerate", &forcedbgframerateon);
						ImGui::Checkbox("AveragedFramerate", &frm_averaged_fps);
						ImGui::InputFloat("Target", &frametime);
					}
					ImGui::InputFloat("AET_FRAME_DURATION", (float*)AET_FRAME_DURATION_ADDRESS);
					ImGui::InputFloat("PV_FRAME_RATE_ADDRESS", (float*)PV_FRAME_RATE_ADDRESS);
					ImGui::InputFloat("FRAME_SPEED_ADDRESS", (float*)FRAME_SPEED_ADDRESS);
					ImGui::SliderFloat("FRAME_SPEED_ADDRESS_", (float*)FRAME_SPEED_ADDRESS, 0, 1, "%.2f");
					ImGui::InputFloat("pvStructPvFrameRate", (float*)(0x0000000140CDD978 + 0x2BF98));
					ImGui::InputFloat("pvStructPvFrameSpeed", (float*)(0x0000000140CDD978 + 0x2BF9C));
				}
				/*
				if (ImGui::Button("Play")) {
					//BASS_ChannelPlay(stream, false);
				};

				if (ImGui::Button("Play Reset")) {
					//BASS_ChannelPlay(stream, true);
				};

				if (ImGui::Button("Pause")) {
					//BASS_ChannelPause(stream);
				};

				ImGui::Checkbox("record", &record);

				//ImGui::InputInt("bufferFrameCount", &bufferFrameCount);

				ImGui::InputInt("to read", &toread);
				*/
				/*
				if (dxgi)
				ImGui::Checkbox("dxgi", &dxgidraw);
				ImGui::InputInt("", (int*)0x000000014CD93788);
				if (ImGui::Button("sub_14058AA70")) {
				typedef void sub_(__int64);
				//sub_14058aa70* funci = (sub_14058aa70*)0x000000014058AA70;
				};

				if (ImGui::Button("sub_1404A9480")) {
					((__int64(__cdecl*)(__int64, char, signed int))0x00000001404A9480)(3, 0 , 1);
				};
				*/
			}
			ImGui::End();
		}

		if (showAbout)
		{
			ImGuiWindowFlags window_flags = 0;
			window_flags |= ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoResize;
			window_flags |= ImGuiWindowFlags_NoCollapse;
			window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			ImGui::Begin("About DivaImGui", &showAbout, window_flags);
			ImGui::Text("TLAC by:");
			ImGui::Text("samyuu");
			ImGui::Text("DIVAHook UI by:");
			ImGui::Text("lybxlpsv");
			ImGui::Text("DIVAHook UI Contributors:");
			ImGui::Text("BesuBaru");
			ImGui::Text("Uses third party libraries :");
			ImGui::Text("ImGui");
			ImGui::Text("Detours");
			ImGui::Text("MinHook (Tsuda Kageyu)");
			ImGui::Text("Hacker Disassembler Engine 32/64 C (Vyacheslav Patkov)");
			if (ImGui::Button("Close")) { showAbout = false; };
			ImGui::End();
		}

		if (showFps)
		{
			ImGui::SetNextWindowBgAlpha(uiTransparency);
			ImGuiWindowFlags window_flags = 0;
			window_flags |= ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoResize;
			window_flags |= ImGuiWindowFlags_NoTitleBar;
			window_flags |= ImGuiWindowFlags_NoCollapse;
			window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
			ImGui::Begin("FPS", &p_open, window_flags);
			ImGui::SetWindowPos(ImVec2(hWindow.right - 100, 0));
			ImGui::SetWindowSize(ImVec2(100, 70));
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			ImGui::Text("FT: %.2fms", 1000 / ImGui::GetIO().Framerate);
			ImGui::End();
		}

		if (firstTime > 0)
		{
			firstTime = firstTime - (int)(1000 / ImGui::GetIO().Framerate);
		}

		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

		if (frameratemanager)
			setFramerate();

		if (usePDFrameLimit)
		{
			if (DivaImGui::MainModule::fpsLimitSet != DivaImGui::MainModule::fpsLimit)
			{
				if (DivaImGui::MainModule::fpsLimitSet < 20)
					setFrameratePD(0);
				else
					setFrameratePD(DivaImGui::MainModule::fpsLimitSet);
				DivaImGui::MainModule::fpsLimit = DivaImGui::MainModule::fpsLimitSet;
			}
		}
		else {
			if (DivaImGui::MainModule::fpsLimitSet != DivaImGui::MainModule::fpsLimit)
			{
				mBeginFrame = system_clock::now();
				prevTimeInSeconds = time_point_cast<seconds>(mBeginFrame);
				frameCountPerSecond = 0;
				DivaImGui::MainModule::fpsLimit = DivaImGui::MainModule::fpsLimitSet;
			}

			auto invFpsLimit = round<system_clock::duration>(dsec{ 1. / DivaImGui::MainModule::fpsLimit });
			auto mEndFrame = mBeginFrame + invFpsLimit;

			auto timeInSeconds = time_point_cast<seconds>(system_clock::now());
			++frameCountPerSecond;
			if (timeInSeconds > prevTimeInSeconds)
			{
				frameCountPerSecond = 0;
				prevTimeInSeconds = timeInSeconds;
			}

			// This part keeps the frame rate.
			if (DivaImGui::MainModule::fpsLimit > 19)
				std::this_thread::sleep_until(mEndFrame);
			mBeginFrame = mEndFrame;
			mEndFrame = mBeginFrame + invFpsLimit;
		}

		end = high_resolution_clock::now();

		if (frameratemanagerdbg)
			setFramerateDbg();

		if (vsync)
		{
			if (!lastvsync) {
				wglSwapIntervalEXT(swapinterval);
				if (!force_fpslimit_vsync) {
					fpsLimitBak = DivaImGui::MainModule::fpsLimitSet;
					DivaImGui::MainModule::fpsLimitSet = 0;
				}
				lastvsync = vsync;
			}
		}
		else {
			if (lastvsync) {
				if (!force_fpslimit_vsync) {
					DivaImGui::MainModule::fpsLimitSet = fpsLimitBak;
				}
				wglSwapIntervalEXT(0);
				lastvsync = vsync;
			}
		}

		//if (dxgidraw)
		//System->Run();
		start = high_resolution_clock::now();

		if (refreshshd == 1)
		{
			/*
			glEnable(GL_VERTEX_PROGRAM_ARB);
			glGetError();
			printf("AM I OK?");
			glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 1);
			glGetError();
			printf("AM I OK?");
			glDisable(GL_VERTEX_PROGRAM_ARB);
			glGetError();
			*/
			RefreshShaders();
			printf("AM I OK?");
			refreshshd = 0;
		}
	}

	BOOL __stdcall hwglSwapBuffers(_In_ HDC hDc)
	{
		if (shaderafthookd)
			if (ReShadeState == 1)
				if (*fnReshadeRender != nullptr)
					fnReshadeRender();
				else {
					void* ptr = GetProcAddress(GetModuleHandle(L"DivaImGuiReShade.dva"), "ReshadeRender");
					if (ptr == nullptr) ptr = GetProcAddress(GetModuleHandle(L"opengl32.dll"), "ReshadeRender");
					//printf("[DivaImGui] ReshadeRender=%p\n", ptr);
					if (ptr != nullptr)
					{
						fnReshadeRender = (ReshadeRender)ptr;
						fnReshadeRender();
					}
				}
		RenderGUI();
		bool result = owglSwapBuffers(hDc);

		return result;
	}

	void __stdcall hwglShaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
	{
		//printf("[DivaImGui] Shader Catch!\n");
		return fnGLShaderSource(shader, count, string, length);
	}

	void __stdcall hwglShaderSourceARB(GLhandleARB shaderObj, GLsizei count, const GLcharARB** string, const GLint* length)
	{
		//printf("[DivaImGui] Shader CatchARB!\n");
		return fnGLShaderSourceARB(shaderObj, count, string, length);
	}

	static GLenum lasttarget = 0;
	static GLuint lastprogram = 0;
	static bool CatchAETRenderPass = false;

	void __stdcall hwglBindProgramARB(GLenum target, GLuint program)
	{
		//printf("[DivaImGui] Shader CatchARB!\n");
		lasttarget = target;
		lastprogram = program;
		if (target == GL_VERTEX_PROGRAM_ARB)
			if (program == 7683)
			{
				CatchAETRenderPass = true;
			}

		return fnGLBindProgramARB(target, program);
	}

	GLenum __stdcall hwglGetError()
	{
		if (CatchAETRenderPass)
		{
			CatchAETRenderPass = false;
			if (ReShadeState == 0)
				if (*fnReshadeRender != nullptr)
					fnReshadeRender();
				else {
					void* ptr = GetProcAddress(GetModuleHandle(L"DivaImGuiReShade.dva"), "ReshadeRender");
					if (ptr == nullptr) ptr = GetProcAddress(GetModuleHandle(L"opengl32.dll"), "ReshadeRender");
					//printf("[DivaImGui] ReshadeRender=%p\n", ptr);
					if (ptr != nullptr)
					{
						fnReshadeRender = (ReshadeRender)ptr;
						fnReshadeRender();
					}
				}
		}
		return FNGlGetError();
	}

	void __stdcall hwglProgramStringARB(GLenum target, GLenum format, GLsizei len, const void* pointer)
	{
		/*
		//printf("[DivaImGui] Shader Catch Program String!\n");
		int retVal = 0;
		//glGetIntegerv(GL_VERTEX_PROGRAM_BINDING_NV, &id);
		switch (target)
		{
			//Note: GL_VERTEX_PROGRAM_NV is the same enum
		case(GL_VERTEX_PROGRAM_ARB):
		case(GL_FRAGMENT_PROGRAM_ARB):
		{
			//Check for ARB support
			if (glGetProgramiv != NULL)
			{
				glGetProgramiv(target, GL_PROGRAM_BINDING_ARB, &retVal);
			}
			//If no ARB support, fall back to old NV way
			else if (target == GL_VERTEX_PROGRAM_NV)
			{
				glGetIntegerv(GL_VERTEX_PROGRAM_BINDING_NV, &retVal);
			}
		}
		break;

		case(GL_FRAGMENT_PROGRAM_NV):
			glGetIntegerv(GL_FRAGMENT_PROGRAM_BINDING_NV, &retVal);
			break;

		case(GL_VERTEX_STATE_PROGRAM_NV):
			//Not sure how to handle state programs
			break;
		}

		if (retVal == 0)
			return fnGLProgramStringARB(target, format, len, pointer);

		int glformat = format;
		char* ptr = (char*)pointer;
		if (format == GL_PROGRAM_FORMAT_ASCII_ARB)
		{
			allocshdptr[retVal] = (int*)malloc(len + 1000);
			allocshdorigptr[retVal] = (int*)pointer;
			allocshdenum[retVal] = target;
			allocshdlen[retVal] = len;
			allocatedshaders[allocatedshadercount] = retVal;
			allocatedshadercount++;

			int curpos = retVal;
			int newlen = fnDNProcessShader((int)pointer, (int)allocshdptr[retVal], len, retVal, glformat);
			if (newlen == -1)
				return fnGLProgramStringARB(target, format, len, pointer);
			return fnGLProgramStringARB(target, format, newlen, allocshdptr[retVal]);
		}
		return fnGLProgramStringARB(target, format, len, pointer);
		*/

		int retVal = 0;

		switch (target)
		{
			//Note: GL_VERTEX_PROGRAM_NV is the same enum
		case(GL_VERTEX_PROGRAM_ARB):
		case(GL_FRAGMENT_PROGRAM_ARB):
		{
			//Check for ARB support
			if (glGetProgramivARB != NULL)
			{
				glGetProgramivARB(target, GL_PROGRAM_BINDING_ARB, &retVal);
			}
			//If no ARB support, fall back to old NV way
			else if (target == GL_VERTEX_PROGRAM_NV)
			{
				glGetIntegerv(GL_VERTEX_PROGRAM_BINDING_NV, &retVal);
			}
		}
		break;

		case(GL_FRAGMENT_PROGRAM_NV):
			glGetIntegerv(GL_FRAGMENT_PROGRAM_BINDING_NV, &retVal);
			break;

		case(GL_VERTEX_STATE_PROGRAM_NV):
			//Not sure how to handle state programs
			break;

		}


		if (format == GL_PROGRAM_FORMAT_ASCII_ARB)
		{
			allocshdptr[allocatedshadercount] = (int*)malloc(len + 1000);
			allocshdorigptr[allocatedshadercount] = (int*)malloc(len + 1000);
			memcpy_s(allocshdorigptr[allocatedshadercount], len, pointer, len);
			allocshdenum[allocatedshadercount] = target;
			allocshdlen[allocatedshadercount] = len;
			allocatedshaders[allocatedshadercount] = lastprogram;
			int curpos = allocatedshadercount;
			allocatedshadercount++;

			int newlen = fnDNProcessShader((int)pointer, (int)allocshdptr[curpos], len, 0, 0);
			if (newlen == -1)
				return fnGLProgramStringARB(target, format, len, pointer);
			return fnGLProgramStringARB(target, format, newlen, allocshdptr[curpos]);
		}
		return fnGLProgramStringARB(target, format, len, pointer);
	}

	static bool wdetoursf = false;

	PROC __stdcall hWGlGetProcAddress(LPCSTR L)
	{
		//printf("[DivaImGui] ");
		//printf(L);
		//printf("\n");
		PROC leproc = wGlGetProcAddress(L);
		if (!wdetoursf)
		{
			wdetoursf = true;
			fnGLShaderSource = (GLShaderSource)wglGetProcAddress("glShaderSource");
			fnGLShaderSourceARB = (GLShaderSourceARB)wglGetProcAddress("glShaderSourceARB");
			fnGLProgramStringARB = (GLProgramStringARB)wglGetProcAddress("glProgramStringARB");
			fnGLBindProgramARB = (GLBindProgramARB)wglGetProcAddress("glBindProgramARB");
			fnDNInitialize();

			printf("[DivaImGui] Hooking glShaderSource=%p\n", fnGLShaderSource);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)fnGLShaderSource, (PVOID)hwglShaderSource);
			DetourTransactionCommit();

			printf("[DivaImGui] Hooking glShaderSourceARB=%p\n", fnGLShaderSourceARB);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)fnGLShaderSourceARB, (PVOID)hwglShaderSourceARB);
			DetourTransactionCommit();

			printf("[DivaImGui] Hooking glProgramStringARB=%p\n", fnGLProgramStringARB);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)fnGLProgramStringARB, (PVOID)hwglProgramStringARB);
			DetourTransactionCommit();

			printf("[DivaImGui] Hooking glBindProgramARB=%p\n", fnGLBindProgramARB);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)fnGLBindProgramARB, (PVOID)hwglBindProgramARB);
			DetourTransactionCommit();

			FNGlGetError = *glGetError;
			printf("[DivaImGui] Hooking glGetError=%p\n", FNGlGetError);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)FNGlGetError, (PVOID)hwglGetError);
			DetourTransactionCommit();
			shaderafthookd = true;
		}
		/*
		if (L == "glShaderSource")
			leproc = (PROC)hwglShaderSource;
		if (L == "glShaderSourceARB")
			leproc = (PROC)hwglShaderSourceARB;
		*/
		return leproc;
	}

	static HINSTANCE hGetProcIDDLL;

	void GLComponent::Initialize()
	{
		HMODULE hMod = GetModuleHandle(L"opengl32.dll");
		void* ptr = GetProcAddress(hMod, "wglSwapBuffers");
		MH_Initialize();
		MH_CreateHook(ptr, hwglSwapBuffers, reinterpret_cast<void**>(&owglSwapBuffers));
		MH_EnableHook(ptr);

		glewInit();

		hGetProcIDDLL = LoadLibrary(L"DivaImGuiDotNet.dll");

		fnDNInitialize = (DNInitialize)GetProcAddress(HMODULE(hGetProcIDDLL), "Initialize");
		fnDNProcessShader = (DNProcessShader)GetProcAddress(HMODULE(hGetProcIDDLL), "ProcessShader");
		fnDNRefreshShaders = (DNRefreshShaders)GetProcAddress(HMODULE(hGetProcIDDLL), "RefreshShaders");
		//fnDNInitialize();
		/*
		fnGLSwapBuffers = (GLSwapBuffers)GetProcAddress(GetModuleHandle(L"opengl32.dll"), "wglSwapBuffers");
		printf("[DivaImGui] glSwapBuffers=%p\n", fnGLSwapBuffers);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)fnGLSwapBuffers, (PVOID)hwglSwapBuffers);
		DetourTransactionCommit();
		*/
		const char* path = "../shadersaft/";
		if (dirExists(path) == 1)
		{
			wGlGetProcAddress = (WGlGetProcAddress)GetProcAddress(GetModuleHandle(L"opengl32.dll"), "wglGetProcAddress");
			printf("[DivaImGui] wGlGetProcAddress=%p\n", wGlGetProcAddress);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)wGlGetProcAddress, (PVOID)hWGlGetProcAddress);
			DetourTransactionCommit();
		}
	}
}