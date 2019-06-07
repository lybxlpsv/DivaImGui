// DivaImGui.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "MainModule.h"


#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)

EXTERN_DLL_EXPORT int getVersion() {
	return 1;
}

EXTERN_DLL_EXPORT int getFrameRate() {
	return DivaImGui::MainModule::fpsLimitSet;
}

EXTERN_DLL_EXPORT void showUi() {
	DivaImGui::MainModule::showUi = true;
	return;
}

EXTERN_DLL_EXPORT void hideUi() {
	DivaImGui::MainModule::showUi = false;
	return;
}

EXTERN_DLL_EXPORT void toggleUi() {
	DivaImGui::MainModule::showUi = !DivaImGui::MainModule::showUi;
	return;
}
