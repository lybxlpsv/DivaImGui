
#include "GLHook.h"
#include <filesystem>
#include "GL/glew.h"
#include "GL/gl.h"
#include "detours/detours.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl2.h"
#include "imgui/imgui_impl_win32.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <stdint.h>
#include <fstream>

namespace DivaImGui::GLHook
{
	typedef void(__stdcall* GLShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
	typedef void(__stdcall* GLShaderSourceARB)(GLhandleARB, GLsizei, const GLcharARB**, const GLint*);
	typedef void(__stdcall* GLProgramStringARB)(GLenum, GLenum, GLsizei, const void*);
	typedef void(__stdcall* GLBindProgramARB)(GLenum, GLuint);
	typedef void(__stdcall* GLBindTexture)(GLenum, GLuint);
	typedef GLenum(__stdcall* GLGetError)();
	typedef PROC(__stdcall* WGlGetProcAddress)(LPCSTR);
	typedef bool(__stdcall* WGLDXUnlockObjectsNV)(HANDLE, GLint, HANDLE*);
	typedef bool(__stdcall* WGLDXlockObjectsNV)(HANDLE, GLint, HANDLE*);
	typedef BOOL(__stdcall* GLSwapBuffers)(HDC);
	GLSwapBuffers fnGLSwapBuffers;

	GLShaderSource fnGLShaderSource;
	GLShaderSourceARB fnGLShaderSourceARB;
	GLProgramStringARB fnGLProgramStringARB;
	GLBindProgramARB fnGLBindProgramARB;

	WGlGetProcAddress wGlGetProcAddress;
	GLBindTexture fnGLBindTexture;
	GLGetError FNGlGetError;
	ReshadeRender GLCtrl::fnReshadeRender;
	WGLDXUnlockObjectsNV fnWGLDXUnlockObjectsNV;
	WGLDXlockObjectsNV fnWGLDXlockObjectsNV;

	bool GLCtrl::Initialized = false;
	int GLCtrl::gamever = 0;
	int GLCtrl::refreshshd = 0;
	int GLCtrl::ReshadeState = -1;
	void* GLCtrl::fnuglswapbuffer;

	static HINSTANCE hGetProcIDDLL;

	static bool wdetoursf = false;
	bool GLCtrl::shaderaftmodified = false;

	std::vector<ShaderPatchInfo> patchesVec;
	std::vector<shaderNames> shaderNamesVec;
	typedef std::pair<std::string, std::string> strpair;
	std::map<std::string, strpair> configMap;

	inline int dirExists(const char* path)
	{
		struct stat info;

		if (stat(path, &info) != 0)
			return 0;
		else if (info.st_mode & S_IFDIR)
			return 1;
		else
			return 0;
	}

	std::vector<std::string> SplitString(const std::string& str, const std::string& delim)
	{
		std::vector<std::string> tokens;
		size_t prev = 0, pos = 0;
		do
		{
			pos = str.find(delim, prev);
			if (pos == std::string::npos)
				pos = str.length();

			std::string token = str.substr(prev, pos - prev);

			if (!token.empty())
				tokens.push_back(token);

			prev = pos + delim.length();
		} while (pos < str.length() && prev < str.length());

		return tokens;
	}

	std::string TrimString(const std::string& str, const std::string& whitespace)
	{
		const size_t strBegin = str.find_first_not_of(whitespace);

		if (strBegin == std::string::npos)
			return "";

		const size_t strEnd = str.find_last_not_of(whitespace);
		const size_t strRange = strEnd - strBegin + 1;

		return str.substr(strBegin, strRange);
	}

	std::string StringReplace(const std::string& str, const std::string& srch, const std::string& repl)
	{
		size_t pos = 0;
		std::string outstr = str;

		while (true)
		{
			pos = outstr.find(srch, pos);
			if (pos == std::string::npos)
				break;

			outstr.replace(pos, srch.length(), repl);
		}

		return outstr;
	}

	void LoadConfig()
	{
		std::ifstream fileStream("plugins\\ShaderPatch.ini");

		if (!fileStream.is_open())
		{
			return;
		}

		std::string line;
		std::string section = "patches";
		std::string lastComment;
		bool isInComment = false;

		// check for BOM
		std::getline(fileStream, line);
		if (line.size() >= 3 && line.rfind("\xEF\xBB\xBF", 0) == 0)
			fileStream.seekg(3);
		else
			fileStream.seekg(0);

		while (std::getline(fileStream, line))
		{
			// detect comments first to make comment exit logic easier
			int commentStartPos;
			if ((commentStartPos = 1, line.size() >= 1 && line[0] == '#') || (commentStartPos = 2, line.size() >= 2 && line.rfind("//", 0) == 0))
			{
				line.erase(0, commentStartPos);
				line = TrimString(line, " \t");
				if (isInComment)
				{
					lastComment += "\n" + line;
				}
				else
				{
					lastComment = line;
					isInComment = true;
				}
				continue;
			}

			isInComment = false;

			if (line.size() <= 0) // skip empty lines
				continue;

			if (line[0] == '[') // section name
			{
				size_t endIdx = line.find(']');
				section = line.substr(1, endIdx - 1);
				std::transform(section.begin(), section.end(), section.begin(), ::tolower);
				continue;
			}

			std::vector<std::string> equalSplit = SplitString(line, "=");
			if (equalSplit.size() < 2) continue;

			if (section == "patches")
			{
				ShaderPatchInfo patch = ShaderPatchInfo();

				patch.fileRegex = equalSplit[0];

				std::vector<std::string> rules = SplitString(equalSplit[1], "||");
				if (rules.size() < 1) continue; // probably will never trigger but whatever

				for (std::string& rule : rules)
				{
					if (rule.size() >= 5 && rule.rfind("arch:", 0) == 0)
					{
						rule.erase(0, 5);
						patch.archs = SplitString(rule, ",");
					}
					else if (rule.size() >= 4 && rule.rfind("cfg:", 0) == 0)
					{
						rule.erase(0, 4);
						// force cfg key to lower because ini shouldn't be case sensitive
						std::transform(rule.begin(), rule.end(), rule.begin(), ::tolower);
						patch.cfg = rule;
					}
					else if (rule.size() >= 5 && rule.rfind("from:", 0) == 0)
					{
						rule.erase(0, 5);
						patch.dataRegex = rule;
					}
					else if (rule.size() >= 3 && rule.rfind("to:", 0) == 0)
					{
						rule.erase(0, 3);
						patch.dataReplace = rule;
					}
				}

				patchesVec.push_back(patch);
			}
			else if (section == "config")
			{
				// force cfg key to lower because ini shouldn't be case sensitive
				std::transform(equalSplit[0].begin(), equalSplit[0].end(), equalSplit[0].begin(), ::tolower);
				equalSplit[1] = TrimString(equalSplit[1], " \t");

				configMap.insert(std::pair<std::string, strpair>(equalSplit[0], strpair(equalSplit[1], lastComment)));
			}
		}

		fileStream.close();
	}


	void __stdcall hwglShaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
	{
		//printf("[DivaImGui] Shader Catch!\n");
		return fnGLShaderSource(shader, count, string, length);
	}

	void __stdcall hwglShaderSourceARB(GLhandleARB shaderObj, GLsizei count, const GLcharARB** string, const GLint* length)
	{
		//printf("[DivaImGui] Shader CatchARB!\n");
		return fnGLShaderSourceARB(shaderObj, count, string, length);
	}

	static GLenum lasttarget = 0;
	static GLuint lastprogram = 0;
	static bool CatchAETRenderPass = false;

	static int allocshdpos = 0;
	static int* allocshdptr[9999];
	static GLenum allocshdenum[9999];
	static int allocshdlen[9999];

	static int allocatedshaders[9999];
	static int allocatedshadercount = 0;

	static int allocshdorigpos = 0;
	static int* allocshdorigptr[9999];

	void __stdcall hwglBindProgramARB(GLenum target, GLuint program)
	{
		//printf("[DivaImGui] Shader CatchARB!%d\n", program);
		lasttarget = target;
		lastprogram = program;
		if (target == GL_VERTEX_PROGRAM_ARB)
			if (program == 7683)
			{
				CatchAETRenderPass = true;
			}

		return fnGLBindProgramARB(target, program);
	}

	GLenum __stdcall hwglGetError()
	{
		if (CatchAETRenderPass)
		{
			//printf("CatchAETRenderPass %p\n", _ReturnAddress());
			CatchAETRenderPass = false;
			if (GLCtrl::ReshadeState == 0)
				if (*GLCtrl::fnReshadeRender != nullptr)
					GLCtrl::fnReshadeRender();
				else {
					void* ptr = GetProcAddress(GetModuleHandle(L"DivaImGuiReShade.dva"), "ReshadeRender");
					if (ptr == nullptr) ptr = GetProcAddress(GetModuleHandle(L"DivaImGuiReShade.asi"), "ReshadeRender");
					if (ptr == nullptr) ptr = GetProcAddress(GetModuleHandle(L"opengl32.dll"), "ReshadeRender");

					if (ptr != nullptr)
					{
						GLCtrl::fnReshadeRender = (ReshadeRender)ptr;
						GLCtrl::fnReshadeRender();
					}
				}
		}
		return FNGlGetError();
	}



	int ProcessShader(const void* source, int* dest, int len, std::string fileName)
	{
		std::string modifiedStr = "";
		if (fileName == "NULL_shd")
		{
			modifiedStr = std::string((char*)source, len);
			auto splitted = SplitString(modifiedStr, "\n");
			fileName = StringReplace(splitted.at(1), "#", "");
			//printf("[DivaImGui] Patching %s\n", fileName.c_str());
			modifiedStr = std::string((char*)source, 0);
		}
		//printf("[DivaImGui] Patching %s\n", fileName.c_str());
		for (ShaderPatchInfo& patch : patchesVec)
		{
			bool archMatches = false;
			{
				archMatches = true;
			}

			bool cfgMatches = false;
			if (patch.cfg.length() == 0 || // patch has no config setting
				(configMap.find(patch.cfg) != configMap.end() && configMap[patch.cfg].first != "0")) // patch has a toggle and is not set to 0
			{
				cfgMatches = true;
			}

			if (archMatches && cfgMatches && std::regex_match(fileName.c_str(), patch.fileRegex))
			{
				if (modifiedStr.length() == 0)
					modifiedStr = std::string((char*)source, len);

				modifiedStr = std::regex_replace(modifiedStr, patch.dataRegex, patch.dataReplace);

				if (patch.cfg.length() > 0) // patch has a config setting
				{
					int valNum = 0;
					std::string valKey;
					while (valNum++, valKey = patch.cfg + "_val" + std::to_string(valNum),
						configMap.find(valKey) != configMap.end()) // loop until there's no more config values set
					{
						modifiedStr = StringReplace(modifiedStr, "<val" + std::to_string(valNum) + ">", configMap[valKey].first);
					}
				}
			}
		}
		//printf("[DivaImGui] source %s.*s\n", len, source);
		if (modifiedStr.length() > 0)
		{
			strcpy_s((char*)dest, len + 1000, modifiedStr.c_str());
			printf("[DivaImGui] Patched %s\n", fileName.c_str());
			return modifiedStr.length();
		}
		else {
			return len;
		}
	}

	int fnDNProcessShader(const void* source, int* dest, int len, GLenum glformat, GLuint glid)
	{
		//printf("Processing! %d\n", glid);
		memcpy(dest, source, len);

		if (GLCtrl::shaderaftmodified)
		{
			return ProcessShader(source, dest, len, "NULL_shd");
		}
		else for (auto& shadernames : shaderNamesVec)
		{
			if (shadernames.glenum == glformat)
				if (shadernames.gluint == glid)
				{
					return ProcessShader(source, dest, len, shadernames.filename);
				}
		}
		return len;
	}

	void fnDNRefreshShaders()
	{
		printf("[DivaImGui] Loading Configs...\n");
		shaderNamesVec.clear();

		std::ifstream fileStream("plugins\\shadernames.txt");
		if (!fileStream.good())
		{
			if (!GLCtrl::shaderaftmodified)
				printf("[DivaImGui] shadernames.txt missing!\n");
			patchesVec.clear();
			configMap.clear();
			LoadConfig();
			int patches = patchesVec.size();
			printf("[DivaImGui] Configs Loaded! Shd=0 Patch=%d\n", patches);
			return;
		}
		std::string line;

		while (std::getline(fileStream, line))
		{
			auto shadername = shaderNames();
			auto splitted = SplitString(line, ",");
			int count = 0;
			for (auto& str : splitted)
			{
				if (count == 0)
					shadername.gluint = stoi(str);
				if (count == 1)
					shadername.glenum = stoi(str);
				if (count == 2)
					shadername.filename = str;
				count++;

				if (count == 3)
				{
					shaderNamesVec.push_back(shadername);
				}
			}
		}

		patchesVec.clear();
		configMap.clear();
		LoadConfig();
		int shaders = shaderNamesVec.size();
		int patches = patchesVec.size();
		printf("[DivaImGui] Configs Loaded! Shd=%d Patch=%d\n", shaders, patches);
	}

	static bool shdInitialized = false;
	void __stdcall hwglProgramStringARB(GLenum target, GLenum format, GLsizei len, const void* pointer)
	{
		int retVal = 0;

		switch (target)
		{
			//Note: GL_VERTEX_PROGRAM_NV is the same enum
		case(GL_VERTEX_PROGRAM_ARB):
		case(GL_FRAGMENT_PROGRAM_ARB):
		{
			//Check for ARB support
			if (glGetProgramivARB != NULL)
			{
				glGetProgramivARB(target, GL_PROGRAM_BINDING_ARB, &retVal);
			}
			//If no ARB support, fall back to old NV way
			else if (target == GL_VERTEX_PROGRAM_NV)
			{
				glGetIntegerv(GL_VERTEX_PROGRAM_BINDING_NV, &retVal);
			}
		}
		break;

		case(GL_FRAGMENT_PROGRAM_NV):
			glGetIntegerv(GL_FRAGMENT_PROGRAM_BINDING_NV, &retVal);
			break;

		case(GL_VERTEX_STATE_PROGRAM_NV):
			//Not sure how to handle state programs
			break;

		}


		if (format == GL_PROGRAM_FORMAT_ASCII_ARB)
		{
			allocshdptr[allocatedshadercount] = (int*)malloc(len + 1000);
			allocshdorigptr[allocatedshadercount] = (int*)malloc(len + 1000);
			memcpy_s(allocshdorigptr[allocatedshadercount], len, pointer, len);
			allocshdenum[allocatedshadercount] = target;
			allocshdlen[allocatedshadercount] = len;
			allocatedshaders[allocatedshadercount] = lastprogram;
			int curpos = allocatedshadercount;
			allocatedshadercount++;

			int newlen = fnDNProcessShader(pointer, allocshdptr[curpos], len, target, lastprogram);
			if (newlen == -1)
				return fnGLProgramStringARB(target, format, len, pointer);
			return fnGLProgramStringARB(target, format, newlen, allocshdptr[curpos]);
		}
		return fnGLProgramStringARB(target, format, len, pointer);
	}

	bool catchtexid = false;

	PROC __stdcall hWGlGetProcAddress(LPCSTR L)
	{
		PROC leproc = wGlGetProcAddress(L);

		if (!wdetoursf)
		{
			wdetoursf = true;

			const char* path = "shadersaft/";

			fnGLShaderSource = (GLShaderSource)wglGetProcAddress("glShaderSource");
			fnGLShaderSourceARB = (GLShaderSourceARB)wglGetProcAddress("glShaderSourceARB");
			fnGLProgramStringARB = (GLProgramStringARB)wglGetProcAddress("glProgramStringARB");

			//if ((dirExists(path) == 1) && (*fnDNInitialize != nullptr)) {
			{

				printf("[DivaImGui] Hooking glShaderSource=%p\n", fnGLShaderSource);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)fnGLShaderSource, (PVOID)hwglShaderSource);
				DetourTransactionCommit();

				printf("[DivaImGui] Hooking glShaderSourceARB=%p\n", fnGLShaderSourceARB);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)fnGLShaderSourceARB, (PVOID)hwglShaderSourceARB);
				DetourTransactionCommit();

				printf("[DivaImGui] Hooking glProgramStringARB=%p\n", fnGLProgramStringARB);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)fnGLProgramStringARB, (PVOID)hwglProgramStringARB);
				DetourTransactionCommit();
			}

			fnGLBindProgramARB = (GLBindProgramARB)wglGetProcAddress("glBindProgramARB");
			printf("[DivaImGui] Hooking glBindProgramARB=%p\n", fnGLBindProgramARB);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)fnGLBindProgramARB, (PVOID)hwglBindProgramARB);
			DetourTransactionCommit();

			FNGlGetError = *glGetError;
			printf("[DivaImGui] Hooking glGetError=%p\n", FNGlGetError);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)FNGlGetError, (PVOID)hwglGetError);
			DetourTransactionCommit();
			fnDNRefreshShaders();
			glewInit();
		}
		return leproc;
	}

	void RefreshShaders(HDC hdc = NULL)
	{
		fnDNRefreshShaders();

		glBindFramebufferEXT(GL_FRAMEBUFFER, 0);

		for (int i = 0; i < allocatedshadercount; i++)
		{
			if (allocatedshaders[i] != 1)
			{
				int newlen = -1;
				newlen = fnDNProcessShader((const void*)allocshdorigptr[i], allocshdptr[i], allocshdlen[i], allocshdenum[i], allocatedshaders[i]);
				if (newlen != -1) {
					//printf("Deleto");
					//glDeleteShader(allocatedshaders[i]);
					//printf("glBindProgramARB");
					//glGenProgramsARB(1, (GLuint*)allocatedshaders[i]);

					//printf("%d\n", allocshdenum[i]);
					//printf("%d\n", allocatedshaders[i]);
					glBindProgramARB(allocshdenum[i], allocatedshaders[i]);
					//printf("fnDNProcessShader");

					//printf("fnGLProgramStringARB");
					fnGLProgramStringARB(allocshdenum[i], GL_PROGRAM_FORMAT_ASCII_ARB, newlen, allocshdptr[i]);
					glGetError();
					glDisable(allocshdenum[i]);
				}

				if (hdc != NULL)
				{
					DisableProcessWindowsGhosting();
					glClear(GL_COLOR_BUFFER_BIT);
					ImGui::SetNextWindowBgAlpha(1.0f);

					ImGui_ImplWin32_NewFrame();
					ImGui_ImplOpenGL2_NewFrame();
					ImGui::NewFrame();

					{
						ImGuiWindowFlags window_flags = 0;
						bool p_open = true;
						window_flags |= ImGuiWindowFlags_NoMove;
						window_flags |= ImGuiWindowFlags_NoResize;
						window_flags |= ImGuiWindowFlags_NoTitleBar;
						window_flags |= ImGuiWindowFlags_NoCollapse;
						window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
						ImGui::Begin("ELAC", &p_open, window_flags);
						ImGui::SetWindowPos(ImVec2(0, 0));
						ImGui::Text("Compiling Shader! id=%d", allocatedshaders[i]);
						ImGui::End();
					}

					ImGui::Render();
					ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
					fnGLSwapBuffers(hdc);
				}
			}
		}
	}
	static bool init2 = false;
	void GLCtrl::Update(HDC hdc)
	{
		fnGLSwapBuffers = (GLSwapBuffers)GLCtrl::fnuglswapbuffer;
		if (!GLCtrl::Initialized)
		{
			LoadConfig();

			{
				wGlGetProcAddress = (WGlGetProcAddress)GetProcAddress(GetModuleHandle(L"opengl32.dll"), "wglGetProcAddress");
				printf("[DivaImGui] wGlGetProcAddress=%p\n", wGlGetProcAddress);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)wGlGetProcAddress, (PVOID)hWGlGetProcAddress);
				DetourTransactionCommit();
			}
			
			GLCtrl::Initialized = true;
		}

		if (refreshshd == 1)
		{
			RefreshShaders(NULL);
			refreshshd = 0;
		}
	}
}