#include "MainModule.h"
#include <filesystem>
#include "GLComponent.h"
#include "GLComponent101.h"
#include "GLComponentLight.h"
#include "GLComponentDummy.h"
#include "FileSystem/ConfigFile.h"
#include "GLHook/GLHook.h"

namespace DivaImGui
{
	typedef std::filesystem::path fspath;

	std::string* MainModule::moduleDirectory;

	const wchar_t* MainModule::DivaWindowName = L"Hatsune Miku Project DIVA Arcade Future Tone";
	const wchar_t* MainModule::ODivaWindowName = L"Project DIVA Arcade";
	const wchar_t* MainModule::GlutDefaultName = L"GLUT";
	const wchar_t* MainModule::freeGlutDefaultName = L"FREEGLUT";

	HWND MainModule::DivaWindowHandle;
	HMODULE MainModule::Module;

	int MainModule::fpsLimit = 0;
	int MainModule::fpsLimitSet = 0;
	bool MainModule::inputDisable = false;
	DivaImGui::GLComponent MainModule::glcomp;
	DivaImGui::V101::GLComponent101 MainModule::glcomp101;
	DivaImGui::VLight::GLComponentLight MainModule::glcomplight;
	DivaImGui::VGCST::GLComponentLight MainModule::glcompgcst;
	DivaImGui::Vdummy::GLComponentDummy MainModule::glcompdummy;
	bool MainModule::showUi = false;

	void MainModule::initializeglcomp()
	{
		const std::string RESOLUTION_CONFIG_FILE_NAME = "graphics.ini";

		DivaImGui::FileSystem::ConfigFile resolutionConfig(MainModule::GetModuleDirectory(), RESOLUTION_CONFIG_FILE_NAME.c_str());
		bool success = resolutionConfig.OpenRead();
		if (!success)
		{
			printf("[DivaImGui] Unable to parse %s\n", RESOLUTION_CONFIG_FILE_NAME.c_str());
		}

		if (success) {
			std::string aftv101 = "1.01";
			std::string aftv710 = "7.10";
			std::string* value;
			bool forceLightweight = false;

			if (resolutionConfig.TryGetValue("shaderpatch", &value))
			{
				if (*value == "1")
					GLHook::GLCtrl::Enabled = true;
			}

			if (resolutionConfig.TryGetValue("amd", &value))
			{
				if (*value == "1")
					GLHook::GLCtrl::isAmd = true;
			}

			if (resolutionConfig.TryGetValue("intel", &value))
			{
				if (*value == "1")
					GLHook::GLCtrl::isIntel = true;
			}

			if (resolutionConfig.TryGetValue("disableGpuDetect", &value))
			{
				if (*value == "1")
					GLHook::GLCtrl::disableGpuDetect = true;
			}

			if (resolutionConfig.TryGetValue("patchAsGameLoads", &value))
			{
				if (*value == "1")
					GLHook::GLCtrl::patchAsGameLoads = true;
			}

			if (resolutionConfig.TryGetValue("disableSprShaders", &value))
			{
				if (*value == "1")
					GLHook::GLCtrl::disableSprShader = true;
			}

			if (resolutionConfig.TryGetValue("forceLightweight", &value))
			{
				if (*value == "1")
					forceLightweight = true;
			}

			if (resolutionConfig.TryGetValue("lybdbg", &value))
			{
				if (*value == "1")
					GLHook::GLCtrl::debug = true;
			}

			if (resolutionConfig.TryGetValue("shadernamed", &value))
			{
				if (*value == "1")
					GLHook::GLCtrl::shaderaftmodified = true;
			}

			if (resolutionConfig.TryGetValue("gc", &value))
			{
				glcompgcst.Initialize();
				return;
			}

			if (forceLightweight)
			{
				if (resolutionConfig.TryGetValue("version", &value))
				{
					double version = std::stod(*value);
					int iv = (version * 100);
					GLHook::GLCtrl::gamever = iv;
				}
				printf("[DivaImGui] Using Universal Mode!\n");
				glcomplight.Initialize();
			}
			else if (resolutionConfig.TryGetValue("version", &value))
			{
				double version = std::stod(*value);
				int iv = (version * 100);
				GLHook::GLCtrl::gamever = iv;

				switch (iv) {
#if _WIN64
				case 101:
					printf("[DivaImGui] AFT v1.01\n");
					glcomp101.Initialize();
					break;

				case 710:
					printf("[DivaImGui] AFT v7.10\n");
					DWORD oldProtect, bck;
					VirtualProtect((BYTE*)0x0000000140626C29, 2, PAGE_EXECUTE_READWRITE, &oldProtect);
					*((BYTE*)0x0000000140626C29 + 0) = 0x48;
					*((BYTE*)0x0000000140626C29 + 1) = 0xE9;
					VirtualProtect((BYTE*)0x0000000140626C29, 2, oldProtect, &bck);
					glcomp.Initialize();
					break;

				case 999:
					glcomp.Initialize();
					break;
#endif
#if _WIN32

#endif
				default:
					printf("[DivaImGui] Unknown Game Version! %d\n", iv);
					printf("[DivaImGui] Using Universal Mode!\n");
					glcomplight.Initialize();
				}
			}
		}
	}

	std::string MainModule::GetModuleDirectory()
	{
		if (moduleDirectory == nullptr)
		{
			WCHAR modulePathBuffer[MAX_PATH];
			GetModuleFileNameW(MainModule::Module, modulePathBuffer, MAX_PATH);

			fspath configPath = fspath(modulePathBuffer).parent_path();
			moduleDirectory = new std::string(configPath.u8string());
		}

		return *moduleDirectory;
	}

	RECT MainModule::GetWindowBounds()
	{
		RECT windowRect;
		GetWindowRect(DivaWindowHandle, &windowRect);

		return windowRect;
	}
}