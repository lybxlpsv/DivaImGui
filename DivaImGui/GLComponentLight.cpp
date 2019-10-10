#include "stdafx.h"
#include "GLComponentLight.h"

#include <stdio.h>

#include <chrono>
#include <thread>

#include "MainModule.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl2.h"
#include "imgui/imgui_impl_win32.h"
#include "GL/glew.h"
#include "GL/gl.h"
#include "GL/wglext.h"
#include "parser.hpp"
#include "FileSystem/ConfigFile.h"
#include "Keyboard/Keyboard.h"
#include "GLHook/GLHook.h"
#include "detours/detours.h"

namespace DivaImGui::VLight
{
	using namespace std::chrono;
	using dsec = duration<double>;

	typedef BOOL(__stdcall* GLSwapBuffers)(HDC);
	GLSwapBuffers fnGLSwapBuffers;

	static int fpsLimitBak = 0;
	static bool showFps = false;
	static bool showUi2 = false;
	static bool showAbout = false;
	static bool debugUi = false;
	static bool lybdbg = false;
	static int firstTime = 8000;

	static float uiTransparency = 0.8;
	static float sleep = 0;
	static float fpsDiff = 0;
	static bool copydepth = false;

	static bool vsync = true;
	static bool lastvsync = false;

	static bool copymodules = false;

	static bool frameratemanager = false;

	const std::string RESOLUTION_CONFIG_FILE_NAME = "graphics.ini";

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

	static bool scaling = false;
	
	GLHook::GLCtrl GLComponentLight::glctrl;
	int GLComponentLight::gamever = 0;

	PFNWGLSWAPINTERVALEXTPROC       wglSwapIntervalEXT = NULL;
	PFNWGLGETSWAPINTERVALEXTPROC    wglGetSwapIntervalEXT = NULL;

	typedef PROC(*func_wglGetProcAddress_t) (LPCSTR lpszProc);
	static func_wglGetProcAddress_t _wglGetProcAddress;
	func_wglGetProcAddress_t	owglGetProcAddress;

	uint64_t hookTramp = NULL;

	typedef BOOL(__stdcall* twglSwapBuffers) (_In_ HDC hDc);

	twglSwapBuffers owglSwapBuffers;

	typedef chrono::time_point<chrono::steady_clock> steady_clock;
	typedef chrono::high_resolution_clock high_resolution_clock;

	steady_clock start;
	steady_clock end;

	static bool dxgi = false;
	static bool dxgidraw = false;
	static bool lyb = false;
	static bool force_fpslimit_vsync;
	static float defaultAetFrameDuration;
	static int swapinterval;
	static bool guirender = true;

	GLComponentLight::GLComponentLight()
	{

	}


	GLComponentLight::~GLComponentLight()
	{

	}

	void InjectCode(void* address, const std::vector<uint8_t> data)
	{
		const size_t byteCount = data.size() * sizeof(uint8_t);

		DWORD oldProtect;
		VirtualProtect(address, byteCount, PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy(address, data.data(), byteCount);
		VirtualProtect(address, byteCount, oldProtect, nullptr);
	}

	void InitializeImGui()
	{
		//if (WGLExtensionSupported("WGL_EXT_swap_control"))
		{
			// Extension is supported, init pointers.
			wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

			// this is another function from WGL_EXT_swap_control extension
			wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
		}

		fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

		DivaImGui::FileSystem::ConfigFile resolutionConfig(MainModule::GetModuleDirectory(), RESOLUTION_CONFIG_FILE_NAME.c_str());
		bool success = resolutionConfig.OpenRead();
		if (!success)
		{
			printf("GLComponent: Unable to parse %s\n", RESOLUTION_CONFIG_FILE_NAME.c_str());
		}

		if (success) {
			std::string trueString = "1";
			std::string* value;
			if (resolutionConfig.TryGetValue("fpsLimit", &value))
			{
				DivaImGui::MainModule::fpsLimitSet = std::stoi(*value);
			}
			if (resolutionConfig.TryGetValue("shadernamed", &value))
			{
				if (*value == trueString)
					GLHook::GLCtrl::shaderaftmodified = true;
			}
			if (resolutionConfig.TryGetValue("forcevsyncfpslimit", &value))
			{
				if (*value == trueString)
					force_fpslimit_vsync = true;
			}
			if (resolutionConfig.TryGetValue("depthCopy", &value))
			{
				if (*value == trueString)
					copydepth = true;
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
			if (resolutionConfig.TryGetValue("disableGui", &value))
			{
				if (*value == "1")
					guirender = false;
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
		}

		for (int i = 0; i < 1000; i++)
		{
			res_scale[i] = -1.0f;
		}

		std::ifstream f("plugins\\res_scale.csv");
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

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.WantCaptureKeyboard == true;

		ImGui_ImplWin32_Init(MainModule::DivaWindowHandle);
		ImGui_ImplOpenGL2_Init();
		ImGui::StyleColorsDark();

		initialize = true;
	}

	void Update()
	{

	}

	static bool enablesprites = true;

	void RenderGui(HDC hDc)
	{

		auto keyboard = DivaImGui::Input::Keyboard::GetInstance();
		keyboard->PollInput();
		if (!initialize)
		{
			MainModule::DivaWindowHandle = FindWindow(0, MainModule::DivaWindowName);
			if (MainModule::DivaWindowHandle == NULL)
				MainModule::DivaWindowHandle = FindWindow(0, MainModule::GlutDefaultName);
			if (MainModule::DivaWindowHandle == NULL)
				MainModule::DivaWindowHandle = FindWindow(0, MainModule::ODivaWindowName);
			if (MainModule::DivaWindowHandle == NULL)
				return;

			InitializeImGui();
		}

		Update();

		RECT hWindow;
		GetClientRect(DivaImGui::MainModule::DivaWindowHandle, &hWindow);
		long uiWidth = hWindow.right - hWindow.left;
		long uiHeight = hWindow.bottom - hWindow.top;

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
			ImGui::Begin("DivaHook UI", &MainModule::showUi, window_flags);
			if (ImGui::CollapsingHeader("Graphics Settings"))
			{
				ImGui::Text("--- Display ---");
				ImGui::Checkbox("Vsync", &vsync);
				ImGui::InputInt("Swap Interval", &swapinterval);
				if (GLHook::GLCtrl::Enabled)
					if (ImGui::Button("Reload Shaders")) { GLHook::GLCtrl::refreshshd = 1; };
			}
			if (ImGui::CollapsingHeader("Framerate"))
			{
				if (wglGetSwapIntervalEXT() == 0)
				{
					ImGui::Text("--- Set the FPS cap to 0 only if you have vsync. ---");
					ImGui::InputInt("Framerate Cap", &DivaImGui::MainModule::fpsLimitSet);
				}
				else {
					ImGui::Text("--- Disable vsync to use framelimiter ---");
				}
			}
			if (ImGui::CollapsingHeader("UI Settings"))
			{
				ImGui::SliderFloat("UI Transparency", &uiTransparency, 0, 1.0);
				ImGui::Checkbox("Framerate Overlay", &showFps);
			}
			if (ImGui::Button("Close")) { MainModule::showUi = false; }; ImGui::SameLine();
			if (ImGui::Button("About")) { showAbout = true; } ImGui::SameLine();
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
			ImGui::Text("BesuBaru, RakiSaionji");
			ImGui::Text("Uses third party libraries :");
			ImGui::Text("ImGui");
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
	}

	BOOL __stdcall hwglSwapBuffers(_In_ HDC hDc)
	{
		GLHook::GLCtrl::Update(hDc);
		if (guirender)
			RenderGui(hDc);
		bool result = fnGLSwapBuffers(hDc);
		return result;
	}

	void GLComponentLight::Initialize()
	{
		glewInit();
		fnGLSwapBuffers = (GLSwapBuffers)GetProcAddress(GetModuleHandle(L"opengl32.dll"), "wglSwapBuffers");
		printf("[DivaImGui] glSwapBuffers=%p\n", fnGLSwapBuffers);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)fnGLSwapBuffers, (PVOID)hwglSwapBuffers);
		DetourTransactionCommit();
		GLHook::GLCtrl::fnuglswapbuffer = (void*)*fnGLSwapBuffers;
		GLHook::GLCtrl::Update(NULL);
	}
}