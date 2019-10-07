#pragma once
#include <Windows.h>
#include <string>
#include <regex>
#include "GL/glew.h"
#include "GL/gl.h"
#include "GL/wglext.h"

namespace DivaImGui::GLHook
{
	struct ShaderPatchInfo {
		std::regex fileRegex;
		std::regex dataRegex;
		std::string dataReplace;
		std::vector<std::string> archs;
		std::string cfg;
	};

	struct shaderNames {
		GLuint gluint;
		GLenum glenum;
		std::string filename;
		std::string file;
	};

	class GLCtrl
	{

	public:


		static bool Initialized;
		static int gamever;
		static int refreshshd;
		static bool shaderaftmodified;
		static void* fnuglswapbuffer;
		static void Update(HDC hdc);
	};
}
