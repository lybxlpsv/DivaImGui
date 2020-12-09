#include "stdafx.h"
#include "GLComponentGCST.h"

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
#include "MinHook.h"
#include <intrin.h>.
#include "detours/detours.h"
#include "Utilities/HookHelper.h"
#include <timeapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <dwmapi.h>

#pragma intrinsic(_ReturnAddress)

namespace DivaImGui::VGCST
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

	static bool vsync = true;
	static bool lastvsync = false;

	const std::string GRAPHICS_CONFIG_FILE = "graphics.ini";

	static std::chrono::time_point mBeginFrame = system_clock::now();
	static std::chrono::time_point prevTimeInSeconds = time_point_cast<seconds>(mBeginFrame);
	static unsigned frameCountPerSecond = 0;

	static bool initialize = false;

	GLHook::GLCtrl GLComponentLight::glctrl;
	int GLComponentLight::gamever = 0;

	PFNWGLSWAPINTERVALEXTPROC       wglSwapIntervalEXT = NULL;
	PFNWGLGETSWAPINTERVALEXTPROC    wglGetSwapIntervalEXT = NULL;

	typedef PROC(*func_wglGetProcAddress_t) (LPCSTR lpszProc);
	static func_wglGetProcAddress_t _wglGetProcAddress;
	func_wglGetProcAddress_t	owglGetProcAddress;

	typedef BOOL(__stdcall* twglSwapBuffers) (_In_ HDC hDc);
	twglSwapBuffers owglSwapBuffers;

	typedef chrono::time_point<chrono::steady_clock> steady_clock;
	typedef chrono::high_resolution_clock high_resolution_clock;

	steady_clock start;
	steady_clock end;

	static bool dxgi = false;
	static bool dxgidraw = false;
	static bool force_fpslimit_vsync;
	static int swapinterval;
	static bool guirender = true;

	static int gcfpsaddress = 0x46EF24;

	GLComponentLight::GLComponentLight()
	{

	}


	GLComponentLight::~GLComponentLight()
	{

	}

	HOOK(HRESULT, oDwmFlush, DwmFlush)
	{
		if (vsync)
			return originaloDwmFlush();
		else return 0;
	}

	void InitializeImGui()
	{
		uintptr_t imageBase = (uintptr_t)GetModuleHandleA(0);
		DWORD oldProtect, bck;
		VirtualProtect((BYTE*)imageBase + gcfpsaddress, 4, PAGE_EXECUTE_READWRITE, &oldProtect);

		INSTALL_HOOK(oDwmFlush);
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
		wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
		
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
	
	HGLRC thisContext;
	bool contextCreated = false;

	GLvoid initContext() {
		RECT hWindow;
		GetClientRect(DivaImGui::MainModule::DivaWindowHandle, &hWindow);
		long uiWidth = hWindow.right - hWindow.left;
		long uiHeight = hWindow.bottom - hWindow.top;

		contextCreated = true;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, uiWidth, uiHeight, 0.0, 1.0, -1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glClearColor(0, 0, 0, 1.0);
	}

	void RenderGui(HDC hDc)
	{
		auto keyboard = DivaImGui::Input::Keyboard::GetInstance();
		keyboard->PollInput();
		
		HGLRC oldContext = wglGetCurrentContext();

		if (!contextCreated) {
			thisContext = wglCreateContext(hDc);
			wglMakeCurrent(hDc, thisContext);
			initContext();
		}
		else {
			wglMakeCurrent(hDc, thisContext);
		}

		if (!initialize)
		{
			MainModule::DivaWindowHandle = FindWindow(0, MainModule::DivaWindowName);
			if (MainModule::DivaWindowHandle == NULL)
				MainModule::DivaWindowHandle = FindWindow(0, MainModule::GlutDefaultName);
			if (MainModule::DivaWindowHandle == NULL)
				MainModule::DivaWindowHandle = FindWindow(0, MainModule::ODivaWindowName);
			if (MainModule::DivaWindowHandle == NULL)
				MainModule::DivaWindowHandle = FindWindow(0, L"GROOVECOASTER");
			if (MainModule::DivaWindowHandle == NULL)
				return;
			printf("[DivaImGui] Window Found!\n");

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
			}
			if (ImGui::CollapsingHeader("Framerate"))
			{
				{
					ImGui::Text("--- Set the FPS cap ---");
					ImGui::InputInt("Framerate Cap", &DivaImGui::MainModule::fpsLimitSet);
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
			ImGui::Text("FT: %.2fms", 1000 / (ImGui::GetIO().Framerate));
			ImGui::End();
		}

		if (firstTime > 0)
		{
			firstTime = firstTime - (int)(1000 / ImGui::GetIO().Framerate);
		}

		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		
		uintptr_t imageBase = (uintptr_t)GetModuleHandleA(0);

		{
			if (DivaImGui::MainModule::fpsLimitSet < 19)
				*(int*)(imageBase + gcfpsaddress) = 300;
			else *(int*)(imageBase + gcfpsaddress) = DivaImGui::MainModule::fpsLimitSet;
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
		mBeginFrame = mEndFrame;
		mEndFrame = mBeginFrame + invFpsLimit;

		wglMakeCurrent(hDc, oldContext);

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
		if (guirender)
			RenderGui(hDc);
		bool result = owglSwapBuffers(hDc);
		return result;
	}

	void GLComponentLight::Initialize()
	{
		DivaImGui::FileSystem::ConfigFile graphicsConfig(MainModule::GetModuleDirectory(), GRAPHICS_CONFIG_FILE.c_str());
		bool success = graphicsConfig.OpenRead();
		if (!success)
		{
			printf("GLComponent: Unable to parse %s\n", GRAPHICS_CONFIG_FILE.c_str());
		}

		if (success) {
			std::string trueString = "1";
			std::string* value;
			if (graphicsConfig.TryGetValue("fpsLimit", &value))
			{
				DivaImGui::MainModule::fpsLimitSet = std::stoi(*value);

			}
			if (graphicsConfig.TryGetValue("forcevsyncfpslimit", &value))
			{
				if (*value == trueString)
					force_fpslimit_vsync = true;
			}
			if (graphicsConfig.TryGetValue("showFps", &value))
			{
				if (*value == trueString)
					showFps = true;
			}
			if (graphicsConfig.TryGetValue("gc", &value))
			{
				gcfpsaddress = std::stoi(*value);
			}
			if (graphicsConfig.TryGetValue("dbg", &value))
			{
				if (*value == trueString)
				{
					lybdbg = true;
				}
			}
			if (graphicsConfig.TryGetValue("disableGui", &value))
			{
				if (*value == "1")
					guirender = false;
			}
			if (graphicsConfig.TryGetValue("Vsync", &value))
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
			if (graphicsConfig.TryGetValue("glswapinterval", &value))
			{
				swapinterval = std::stoi(*value);
			}
		}

		fnGLSwapBuffers = (GLSwapBuffers)GetProcAddress(GetModuleHandle(L"opengl32.dll"), "wglSwapBuffers");
		printf("[DivaImGui] glSwapBuffers=%p\n", fnGLSwapBuffers);
		printf("[DivaImGui] Using MinHook\n");
		HMODULE hMod = GetModuleHandle(L"opengl32.dll");
		void* ptr = GetProcAddress(hMod, "wglSwapBuffers");
		MH_Initialize();
		MH_CreateHook(ptr, hwglSwapBuffers, reinterpret_cast<void**>(&owglSwapBuffers));
		MH_EnableHook(ptr);
	}
}