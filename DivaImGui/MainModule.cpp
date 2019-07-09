#include "MainModule.h"
#include <filesystem>
#include "GLComponent.h"
#include "GLComponent101.h"
#include "FileSystem/ConfigFile.h"

namespace DivaImGui
{
	typedef std::filesystem::path fspath;

	std::string *MainModule::moduleDirectory;

	const wchar_t* MainModule::DivaWindowName = L"Hatsune Miku Project DIVA Arcade Future Tone";
	const wchar_t* MainModule::GlutDefaultName = L"GLUT";
	const wchar_t* MainModule::freeGlutDefaultName = L"FREEGLUT";

	HWND MainModule::DivaWindowHandle;
	HMODULE MainModule::Module;

	int MainModule::fpsLimit = 0;
	int MainModule::fpsLimitSet = 0;
	bool MainModule::inputDisable = false;
	DivaImGui::GLComponent MainModule::glcomp;
	DivaImGui::V101::GLComponent101 MainModule::glcomp101;
	bool MainModule::showUi = false;

	void MainModule::initializeglcomp()
	{
		const std::string RESOLUTION_CONFIG_FILE_NAME = "graphics.ini";

		DivaImGui::FileSystem::ConfigFile resolutionConfig(MainModule::GetModuleDirectory(), RESOLUTION_CONFIG_FILE_NAME.c_str());
		bool success = resolutionConfig.OpenRead();
		if (!success)
		{
			printf("GLComponent: Unable to parse %s\n", RESOLUTION_CONFIG_FILE_NAME.c_str());
		}

		if (success) {
			std::string v101 = "1.01";

			std::string* value;
			if (resolutionConfig.TryGetValue("version", &value))
			{
				int version = std::stoi(*value);

				if (*value == v101)
				{
					printf("GLComponent: AFT v1.01\n");
					glcomp101.Initialize();
				}
				else {
					printf("GLComponent: AFT v7.10\n");
					glcomp.Initialize();
				}
			}
			else {
				printf("GLComponent: AFT v7.10\n");
				glcomp.Initialize();
			}
		}
	}

	std::string MainModule::GetModuleDirectory()
	{
		if (moduleDirectory == nullptr)
		{
			CHAR modulePathBuffer[MAX_PATH];
			GetModuleFileNameA(MainModule::Module, modulePathBuffer, MAX_PATH);

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