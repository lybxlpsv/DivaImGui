#include "GLHook.h"
#include "GLHookConstants.h"
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
#include <snappy/snappy.h>

#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

namespace DivaImGui::GLHook
{
	typedef void(__stdcall* GLShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
	typedef void(__stdcall* GLShaderSourceARB)(GLhandleARB, GLsizei, const GLcharARB**, const GLint*);
	typedef void(__stdcall* GLProgramStringARB)(GLenum, GLenum, GLsizei, const void*);
	typedef void(__stdcall* GLBindProgramARB)(GLenum, GLuint);
	typedef PROC(__stdcall* WGlGetProcAddress)(LPCSTR);
	typedef void(__stdcall* GLBindTexture)(GLenum, GLuint);
	typedef BOOL(__stdcall* GLSwapBuffers)(HDC);
	typedef BOOL(__stdcall* GLEnable)(GLenum);
	typedef void(__stdcall* GLDrawRangeElements)(GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid*);
	GLSwapBuffers fnGLSwapBuffers;
	PFNGLBINDFRAMEBUFFEREXTPROC fnGLBindFramebufferEXT;
	GLShaderSource fnGLShaderSource;
	GLShaderSourceARB fnGLShaderSourceARB;
	GLProgramStringARB fnGLProgramStringARB;
	GLBindProgramARB fnGLBindProgramARB;
	GLDrawRangeElements fnGLDrawRangeElements;
	GLBindTexture fnGLBindTexture;
	WGlGetProcAddress wGlGetProcAddress;
	GLEnable fnGLEnable;
	PFNGLBINDBUFFERPROC fnGLBindBuffer;
	PFNGLBUFFERDATAPROC fnGLBufferData;

	bool GLCtrl::Initialized = false;
	int GLCtrl::gamever = 0;
	int GLCtrl::refreshshd = 0;
	void* GLCtrl::fnuglswapbuffer;
	bool GLCtrl::Enabled = false;
	bool GLCtrl::isAmd = false;
	bool GLCtrl::isIntel = false;
	bool GLCtrl::debug = false;
	bool GLCtrl::disableSprShader = false;
	static HINSTANCE hGetProcIDDLL;
	static bool gpuchecked = false;

	static bool wdetoursf = false;
	bool GLCtrl::shaderaftmodified = false;

	static int PatchedCounter = 0;

	std::vector<SimpleReplace> amdPatchesVec;
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

	inline bool FindString(const std::string& line, const std::string& search)
	{
		std::size_t found = line.find(search);
		if (found != std::string::npos)
			return true;
		else return false;
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

	std::string JoinString(std::vector<std::string> arr, std::string delimiter)
	{
		if (arr.empty()) return "";

		std::string str;
		for (auto i : arr)
			str += i + delimiter;
		str = str.substr(0, str.size() - delimiter.size());
		return str;
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
						rule = StringReplace(rule, "\\n", "\n");
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

		return fnGLBindProgramARB(target, program);
	}

	void __stdcall hwglEnable(GLenum cap)
	{
		fnGLEnable(cap);
#if !_WIN64

		if (GLCtrl::gamever == 201)
		{
			{
				if (lastprogram > 3269)
				{
					glDisable(GL_VERTEX_PROGRAM_ARB);
					glDisable(GL_FRAGMENT_PROGRAM_ARB);
				}
			}
		}

		if (GLCtrl::gamever == 300)
		{
			{
				if (lastprogram > 3761)
				{
					glDisable(GL_VERTEX_PROGRAM_ARB);
					glDisable(GL_FRAGMENT_PROGRAM_ARB);
				}
			}
		}

		if (GLCtrl::gamever == 600)
		{
			{
				if (lastprogram > 4179)
				{
					glDisable(GL_VERTEX_PROGRAM_ARB);
					glDisable(GL_FRAGMENT_PROGRAM_ARB);
				}
			}
		}
#endif

#if _WIN64
		if (GLCtrl::gamever == 101)
		{
			{
				if (lastprogram > 7791)
				{
					//printf("disabling");
					glDisable(GL_VERTEX_PROGRAM_ARB);
					glDisable(GL_FRAGMENT_PROGRAM_ARB);
				}
			}
		}

		if ((GLCtrl::gamever == 710) || (GLCtrl::gamever == 600))
		{
			{
				if (lastprogram > 8573)
				{
					//printf("disabling");
					glDisable(GL_VERTEX_PROGRAM_ARB);
					glDisable(GL_FRAGMENT_PROGRAM_ARB);
				}
			}
		}
#endif
		return;
	}

	int ProcessShader(const void* source, int* dest, int len, std::string fileName, GLenum glformat, GLuint glid)
	{
		std::string modifiedStr = "";
		if (fileName == "NULL_shd")
		{
			modifiedStr = std::string((char*)source, len);
			auto splitted = SplitString(modifiedStr, "\n");
			fileName = StringReplace(splitted.at(1), "#", "");
			//printf("[DivaImGui] Patching %s\n", fileName.c_str());
			modifiedStr = std::string((char*)source, 0);
			std::ofstream out("dump.txt", std::ios::app);
			out << glid << "," << glformat << "," << fileName << "\n";
			out.close();
		}
		//printf("[DivaImGui] Patching %s\n", fileName.c_str());

		if ((GLCtrl::isAmd) || (GLCtrl::isIntel))
		{
			amdPatchesVec.clear();

			modifiedStr = std::string((char*)source, len);

			auto splitted = SplitString(modifiedStr, "\n");
			std::vector<std::string> newsplitted;

			for (std::string& line : splitted)
			{
				std::size_t found = line.find("ATTRIB");
				if (found != std::string::npos)
				{
					SimpleReplace patch = SimpleReplace();
					std::string newstr = line;
					newstr = StringReplace(newstr, "ATTRIB", "");
					newstr = StringReplace(newstr, ";", "");
					newstr = StringReplace(newstr, " = ", "=");
					newstr = StringReplace(newstr, " ", "");
					auto nsplit = SplitString(newstr, "=");
					patch.find = nsplit[0];
					patch.replace = nsplit.at(1);
					amdPatchesVec.push_back(patch);
				}
				else {
					found = line.find("OUTPUT");
					if (found != std::string::npos)
					{
						bool ispatch = true;
						found = line.find("SHORT");
						if (found != std::string::npos)
						{
							ispatch = false;
						}
						found = line.find("LONG");
						if (found != std::string::npos)
						{
							ispatch = false;
						}
						if (GLCtrl::isIntel)
							ispatch = true;
						if (ispatch)
						{
							SimpleReplace patch = SimpleReplace();
							std::string newstr = line;
							newstr = StringReplace(newstr, "OUTPUT", "");
							newstr = StringReplace(newstr, "SHORT ", "");
							newstr = StringReplace(newstr, "LONG ", "");
							newstr = StringReplace(newstr, ";", "");
							newstr = StringReplace(newstr, " = ", "=");
							newstr = StringReplace(newstr, " ", "");
							auto nsplit = SplitString(newstr, "=");
							patch.find = nsplit.at(0);
							patch.replace = nsplit.at(1);
							amdPatchesVec.push_back(patch);
						}
						if (GLCtrl::isIntel)
						{
							//line = StringReplace(line, "SHORT OUTPUT", "##SHORT OUTPUT");
							//line = StringReplace(line, "LONG OUTPUT", "##LONG OUTPUT");
						}
					}
					else {
						if ((GLCtrl::isIntel) || (GLCtrl::gamever == 710))
						{
							found = line.find("PARAM");
							if (found != std::string::npos)
							{
								found = line.find(".color");
								if (found != std::string::npos)
								{
									SimpleReplace patch = SimpleReplace();
									std::string newstr = line;
									newstr = StringReplace(newstr, "PARAM", "");
									newstr = StringReplace(newstr, ";", "");
									newstr = StringReplace(newstr, " = ", "=");
									newstr = StringReplace(newstr, " ", "");
									auto nsplit = SplitString(newstr, "=");
									patch.find = nsplit[0];
									patch.replace = nsplit.at(1);
									amdPatchesVec.push_back(patch);
									line = StringReplace(line, "PARAM", "##PARAM");
								}
							}

						}

						for (SimpleReplace& patch : amdPatchesVec)
						{
							line = StringReplace(line, patch.find, patch.replace);
						}
						//line = StringReplace(line, "POW", "##POW");
					}
				}
			}
			modifiedStr = JoinString(splitted, "\n");
		}

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
				if (GLCtrl::Enabled == true)
					cfgMatches = true;
			}

			if (patch.cfg == "amd")
				if (GLCtrl::isAmd)
					cfgMatches = true;

			if (patch.cfg == "intel")
				if (GLCtrl::isIntel)
					cfgMatches = true;

			if (archMatches && cfgMatches && std::regex_match(fileName.c_str(), patch.fileRegex))
			{
				if (modifiedStr.length() == 0)
				{
					modifiedStr = std::string((char*)source, len);
				}

				modifiedStr = std::regex_replace(modifiedStr, patch.dataRegex, patch.dataReplace);
				modifiedStr = StringReplace(modifiedStr, "<fname>", fileName);

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

		if (modifiedStr.length() > 0)
		{
			//printf("%s", modifiedStr.c_str());
			strcpy_s((char*)dest, len + 1000, modifiedStr.c_str());
			//printf("[DivaImGui] Patched %s\n", fileName.c_str());
			PatchedCounter++;
			return modifiedStr.length();
		}
		else {
			return len;
		}
	}

	int fnDNProcessShader(const void* source, int* dest, int len, GLenum glformat, GLuint glid)
	{
		memcpy(dest, source, len);

		if (GLCtrl::shaderaftmodified)
		{
			return ProcessShader(source, dest, len, "NULL_shd", glformat, glid);
		}
		else for (auto& shadernames : shaderNamesVec)
		{
			if (shadernames.glenum == glformat)
				if (shadernames.gluint == glid)
				{
					return ProcessShader(source, dest, len, shadernames.filename, glformat, glid);
				}
		}
		return len;
	}

	inline void loadShaderNameFromMemory(std::string outputstr)
	{
		auto per_line = SplitString(outputstr, "\r\n");
		for (auto& str : per_line)
		{
			auto shadername = shaderNames();
			auto splitted = SplitString(str, ",");
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
	}

	void fnDNRefreshShaders()
	{
		printf("[DivaImGui] Loading Configs...\n");
		shaderNamesVec.clear();

		std::ifstream fileStream("plugins\\shadernames.txt");
		bool shaderLoaded = false;

		if (!fileStream.good())
		{
#if _WIN64
			if (GLCtrl::gamever == 710)
			{
				void* output = malloc(aft701_len);
				auto outputstr = std::string((char*)output, sizeof(aft701_len));
				shaderLoaded = snappy::Uncompress((char*)aft701, sizeof(aft701), &outputstr);
				loadShaderNameFromMemory(outputstr);
				free(output);
			}

			if (GLCtrl::gamever == 600)
			{
				void* output = malloc(aft600_len);
				auto outputstr = std::string((char*)output, sizeof(aft600_len));
				shaderLoaded = snappy::Uncompress((char*)aft600, sizeof(aft600), &outputstr);
				loadShaderNameFromMemory(outputstr);
				free(output);
			}

			if (GLCtrl::gamever == 101)
			{
				void* output = malloc(aft101_len);
				auto outputstr = std::string((char*)output, sizeof(aft101_len));
				shaderLoaded = snappy::Uncompress((char*)aft101, sizeof(aft101), &outputstr);
				loadShaderNameFromMemory(outputstr);
				free(output);
			}
#endif

#if !_WIN64

			if (GLCtrl::gamever == 101)
			{
				void* output = malloc(pda101_len);
				auto outputstr = std::string((char*)output, sizeof(pda101_len));
				shaderLoaded = snappy::Uncompress((char*)pda101, sizeof(pda101), &outputstr);
				loadShaderNameFromMemory(outputstr);
				free(output);
			}

			if (GLCtrl::gamever == 200)
			{
				void* output = malloc(pda200_len);
				auto outputstr = std::string((char*)output, sizeof(pda200_len));
				shaderLoaded = snappy::Uncompress((char*)pda200, sizeof(pda200), &outputstr);
				loadShaderNameFromMemory(outputstr);
				free(output);
			}

			if (GLCtrl::gamever == 301)
			{
				void* output = malloc(pda301_len);
				auto outputstr = std::string((char*)output, sizeof(pda301_len));
				shaderLoaded = snappy::Uncompress((char*)pda301, sizeof(pda301), &outputstr);
				loadShaderNameFromMemory(outputstr);
				free(output);
			}

			if (GLCtrl::gamever == 600)
			{
				void* output = malloc(pda600_len);
				auto outputstr = std::string((char*)output, sizeof(pda600_len));
				shaderLoaded = snappy::Uncompress((char*)pda600, sizeof(pda600), &outputstr);
				loadShaderNameFromMemory(outputstr);
				free(output);
			}
#endif
			if ((!GLCtrl::shaderaftmodified) && !shaderLoaded)
				printf("[DivaImGui] shadernames.txt missing!\n");

			patchesVec.clear();
			configMap.clear();
			LoadConfig();
			int shaders = shaderNamesVec.size();
			int patches = patchesVec.size();
			printf("[DivaImGui] Configs Loaded! Shd=%d Patch=%d\n", shaders, patches);
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

	void InitGpu()
	{
		gpuchecked = true;
		auto gvendor = glGetString(GL_VENDOR);
		std::string vendor = std::string((char*)gvendor);
		std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);
		if (FindString(vendor, "nvidia"))
		{
			GLCtrl::isAmd = false;
			GLCtrl::isIntel = false;
			printf("[DivaImGui] NVIDIA detected\n");
		}
		else if (FindString(vendor, "amd") || FindString(vendor, "ati"))
		{
			GLCtrl::isAmd = true;
			GLCtrl::isIntel = false;
			printf("[DivaImGui] AMD detected, applying fixes...\n");
		}
		else if (FindString(vendor, "intel") || FindString(vendor, "mesa"))
		{
			GLCtrl::isAmd = true;
			GLCtrl::isIntel = true;
			printf("[DivaImGui] Mesa/Intel detected, applying fixes...\n");
		}
		else {
			printf("[DivaImGui] Unable to indentify gpu : %s\n", vendor.c_str());
		}
	}

	void __stdcall hwglBindBuffer(GLenum target, GLuint buffer)
	{
		if (GLCtrl::isAmd || GLCtrl::isIntel)
		{
			if ((target == GL_VERTEX_PROGRAM_PARAMETER_BUFFER_NV) || (target == GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_NV))
			{
				return;
			}
			else return fnGLBindBuffer(target, buffer);
		}
	}

	void __stdcall hwglBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
	{
		if (GLCtrl::isAmd || GLCtrl::isIntel)
		{
			if ((target == GL_VERTEX_PROGRAM_PARAMETER_BUFFER_NV) || (target == GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_NV))
			{
				return;
			}
			else return fnGLBufferData(target, size, data, usage);
		}
	}

	void __stdcall hwglBindFramebufferEXT(GLenum target, GLuint framebuffer)
	{
		if (framebuffer == 6)
			framebuffer = 0;
		fnGLBindFramebufferEXT(target, framebuffer);
		return;
	}

	void __stdcall hwglBindTexture(GLenum format, GLuint id)
	{
		if (GLCtrl::isAmd || GLCtrl::isIntel)
		{
#if _WIN32
			if (GLCtrl::gamever == 101)
			{
				if (id == 29)
					if (lastprogram == 2045)
						return fnGLBindTexture(format, 0);

				if (id == 30)
					if (lastprogram == 2045)
						return fnGLBindTexture(format, 0);

				if (id == 31)
					if (lastprogram == 2045)
						return fnGLBindTexture(format, 0);

				if (id == 32)
					if (lastprogram == 2045)
						return fnGLBindTexture(format, 0);

				if (id == 33)
					if (lastprogram == 2045)
						return fnGLBindTexture(format, 0);

				if (id == 34)
					if (lastprogram == 2045)
						return fnGLBindTexture(format, 0);
			}
#endif
		}
		return fnGLBindTexture(format, id);
	}

	void __stdcall hwGLDrawRangeElements(GLenum mode,
		GLuint start,
		GLuint end,
		GLsizei count,
		GLenum type,
		const GLvoid* indices)
	{
		if (indices == nullptr)
		{
			//printf("[DivaImGui] %p is responsible for nulltpr!\n", _ReturnAddress());
			fnGLDrawRangeElements(mode, start, end, count, type, indices);
			//glDrawElements(mode, count, type, indices);
			return;
		}
		else {
			//glDrawElements(mode, count, type, indices);
			fnGLDrawRangeElements(mode, start, end, count, type, indices);
			return;
		}
	}

	bool catchtexid = false;

	int __stdcall StubbedFunc(int a = 0, int i = 0, int u = 0, int e = 0, int o = 0)
	{
		return 1;
	}

	PROC __stdcall hWGlGetProcAddress(LPCSTR name)
	{
		if (!gpuchecked)
		{
			InitGpu();
		}

		PROC leproc = wGlGetProcAddress(name);
		if (leproc == nullptr)
		{
			/*
			std::string strname = std::string(name);
			printf("[DivaImGui] %s is nullptr\n", strname.c_str());
			std::size_t found = strname.find("EXT");
			if (found != std::string::npos)
			{
				strname = StringReplace(strname, "EXT", "");
				PROC leproc = wGlGetProcAddress(strname.c_str());
				printf("[DivaImGui] Attempting %s to %s\n", name, strname.c_str());
			}

			if (found != std::string::npos)
			{
				strname = StringReplace(strname, "NV", "");
				PROC leproc = wGlGetProcAddress(strname.c_str());
				printf("[DivaImGui] Attempting %s to %s\n", name, strname.c_str());
			}*/
			if (leproc == nullptr)
				leproc = (PROC)*StubbedFunc;
			else
			{
				//printf("[DivaImGui] Redirect %s to %s\n", name, strname.c_str());
			}
		}
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

			if (GLCtrl::disableSprShader)
			{
				fnGLEnable = (GLEnable)*glEnable;
				printf("[DivaImGui] Hooking glEnable=%p\n", fnGLEnable);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)fnGLEnable, (PVOID)hwglEnable);
				DetourTransactionCommit();
			}

			glewInit();

			if (GLCtrl::isIntel || GLCtrl::isAmd)
			{
				fnGLBindBuffer = *glBindBuffer;
				printf("[DivaImGui] Hooking glBindBuffer=%p\n", fnGLBindBuffer);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)fnGLBindBuffer, (PVOID)hwglBindBuffer);
				DetourTransactionCommit();

				fnGLBufferData = *glBufferData;
				printf("[DivaImGui] Hooking glBufferData=%p\n", fnGLBufferData);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)fnGLBufferData, (PVOID)hwglBufferData);
				DetourTransactionCommit();
			}

			if (GLCtrl::debug)
			{
				fnGLDrawRangeElements = (GLDrawRangeElements)wglGetProcAddress("glDrawRangeElements");
				printf("[DivaImGui] Hooking glDrawRangeElements=%p\n", fnGLDrawRangeElements);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)fnGLDrawRangeElements, (PVOID)hwGLDrawRangeElements);
				DetourTransactionCommit();

				fnGLBindTexture = *glBindTexture;
				printf("[DivaImGui] fnGLBindTexture=%p\n", glBindTexture);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)fnGLBindTexture, (PVOID)hwglBindTexture);
				DetourTransactionCommit();


				fnGLBindFramebufferEXT = *glBindFramebufferEXT;
				printf("[DivaImGui] fnGLBindFramebufferEXT=%p\n", fnGLBindFramebufferEXT);
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)fnGLBindFramebufferEXT, (PVOID)hwglBindFramebufferEXT);
				DetourTransactionCommit();
			}
			else glewInit();

			fnDNRefreshShaders();

		}
		return leproc;
	}

	void RefreshShaders(HDC hdc = NULL)
	{
		PatchedCounter = 0;
		fnDNRefreshShaders();

		glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
		int counter = 0;
		for (int i = 0; i < allocatedshadercount; i++)
		{
			if (allocatedshaders[i] != 1)
			{
				int newlen = -1;
				newlen = fnDNProcessShader((const void*)allocshdorigptr[i], allocshdptr[i], allocshdlen[i], allocshdenum[i], allocatedshaders[i]);
				if (newlen != -1) {
					glBindProgramARB(allocshdenum[i], allocatedshaders[i]);
					fnGLProgramStringARB(allocshdenum[i], GL_PROGRAM_FORMAT_ASCII_ARB, newlen, allocshdptr[i]);
					glGetError();
					glDisable(allocshdenum[i]);
				}
				counter++;
				if (counter == 251)
					counter = 0;
				if ((hdc != NULL) && (counter == 250))
				{
					DisableProcessWindowsGhosting();
					//glClear(GL_COLOR_BUFFER_BIT);
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
		printf("[DivaImGui] Patched %d Shaders\n", PatchedCounter);
	}
	static bool init2 = false;
	void GLCtrl::Update(HDC hdc)
	{
		if (!GLCtrl::isAmd && !GLCtrl::isIntel)
			if (GLCtrl::Enabled == false)
				return;

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
		else {
			if (!init2)
			{
				printf("[DivaImGui] Patched %d Shaders\n", PatchedCounter);
				init2 = true;
			}
		}

		if (refreshshd == 1)
		{
			RefreshShaders(hdc);
			refreshshd = 0;
		}
	}
	}