// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "windows.h"
#include "MainModule.h"
#include "GLComponent.h"



void AttachImGui()
{
	printf("[DivaImGui] Initializing hooks...\n");
	DivaImGui::MainModule::initializeglcomp();
	printf("[DivaImGui] Hooks initialized\n");
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		

		DivaImGui::MainModule::Module = hModule;
		AttachImGui();
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

