#pragma once
#include <Windows.h>
#include <string>
#include "GLComponent.h"
#include "GLComponent101.h"
#include "GLComponentLight.h"
#include "GLComponentDummy.h"
#include "GLComponentGCST.h"

namespace DivaImGui
{
	class MainModule
	{
	private:
		static std::string *moduleDirectory;

	public:
		static const wchar_t* DivaWindowName;
		static const wchar_t* ODivaWindowName;
		static const wchar_t* GlutDefaultName;
		static const wchar_t* freeGlutDefaultName;

		static HWND DivaWindowHandle;
		static HMODULE Module;

		static int fpsLimit;
		static int fpsLimitSet;
		static bool inputDisable;

		static DivaImGui::GLComponent glcomp;
		static DivaImGui::V101::GLComponent101 glcomp101;
		static DivaImGui::VLight::GLComponentLight glcomplight;
		static DivaImGui::VGCST::GLComponentLight glcompgcst;
		static DivaImGui::Vdummy::GLComponentDummy glcompdummy;

		static bool showUi;
		static std::string GetModuleDirectory();
		static RECT GetWindowBounds();

		static void initializeglcomp();
	};
}
