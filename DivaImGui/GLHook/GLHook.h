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

	struct SimpleReplace {
		std::string find;
		std::string replace;
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
		static bool Enabled;
		static bool Initialized;
		static int gamever;
		static int refreshshd;
		static bool shaderaftmodified;
		static bool isAmd;
		static bool isIntel;
		static bool disableGpuDetect;
		static bool disableSprShader;
		static bool debug;
		static void* fnuglswapbuffer;
		static void Update(HDC hdc);
	};
}
