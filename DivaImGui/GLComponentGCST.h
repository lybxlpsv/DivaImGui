#pragma once
#include "GLHook/GLHook.h"

namespace DivaImGui::VGCST
{
	class GLComponentLight
	{
	public:
		GLComponentLight();
		~GLComponentLight();
		void Initialize();
		static GLHook::GLCtrl glctrl;
		static int gamever;
	};
}
