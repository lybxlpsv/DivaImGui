#pragma once
#include "GLHook/GLHook.h"

namespace DivaImGui::VLight
{
	class GLComponentLight
	{
	public:
		GLComponentLight();
		~GLComponentLight();
		void Initialize();
		static GLHook::GLCtrl glctrl;
	};
}
