#pragma once
#include <Windows.h>
#include <string>


namespace DivaImGui::GLHook
{
	
	typedef int(__stdcall* ReshadeRender)();

	class GLCtrl
	{

	public:
		static bool Initialized;
		static int gamever;
		static int refreshshd;
		static void* fnuglswapbuffer;
		static void Update(HDC hdc);
		static int ReshadeState;
		static ReshadeRender fnReshadeRender;
	};
}
