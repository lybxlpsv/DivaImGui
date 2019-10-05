
#include "GLHook.h"
#include <filesystem>
#include "GL/glew.h"
#include "GL/gl.h"
#include "detours/detours.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl2.h"
#include "imgui/imgui_impl_win32.h"

namespace DivaImGui::GLHook
{
	typedef void(__stdcall* DNInitialize)(int);
	typedef int(__stdcall* DNRefreshShaders)(void);
	typedef int(__stdcall* DNProcessShader)(int, int, int, int, int);
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
	DNInitialize fnDNInitialize;
	DNProcessShader fnDNProcessShader;
	DNRefreshShaders fnDNRefreshShaders;
	WGlGetProcAddress wGlGetProcAddress;
	GLBindTexture fnGLBindTexture;
	GLGetError FNGlGetError;
	ReshadeRender GLCtrl::fnReshadeRender;
	WGLDXUnlockObjectsNV fnWGLDXUnlockObjectsNV;
	WGLDXlockObjectsNV fnWGLDXlockObjectsNV;

	bool GLCtrl::Initialized = false;
	int GLCtrl::gamever = 1701;
	int GLCtrl::refreshshd = 0;
	int GLCtrl::ReshadeState = -1;
	void* GLCtrl::fnuglswapbuffer;

	static HINSTANCE hGetProcIDDLL;

	static bool wdetoursf = false;
	static bool shaderafthookd = false;

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
					if (ptr == nullptr) ptr = GetProcAddress(GetModuleHandle(L"opengl32.dll"), "ReshadeRender");
					//printf("[DivaImGui] ReshadeRender=%p\n", ptr);
					if (ptr != nullptr)
					{
						GLCtrl::fnReshadeRender = (ReshadeRender)ptr;
						GLCtrl::fnReshadeRender();
					}
				}
		}
		return FNGlGetError();
	}
	static bool shdInitialized = false;
	void __stdcall hwglProgramStringARB(GLenum target, GLenum format, GLsizei len, const void* pointer)
	{
		printf("[DivaImGui] hwglProgramStringARB %d\n", lastprogram);
		if (!shdInitialized)
		{
			if (*fnDNInitialize != nullptr)
				fnDNInitialize(9);
			shdInitialized = true;
		}
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

			if (!shaderafthookd)
			{
				return fnGLProgramStringARB(target, format, len, pointer);
			}
			int newlen = fnDNProcessShader((int)pointer, (int)allocshdptr[curpos], len, target, lastprogram);
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

			fnDNInitialize = (DNInitialize)GetProcAddress(GetModuleHandle(L"DivaImGuiDotNet.asi"), "SetPDVer");
			printf("[DivaImGui] fnDNInitialize=%p\n", fnDNInitialize);
			fnDNProcessShader = (DNProcessShader)GetProcAddress(GetModuleHandle(L"DivaImGuiDotNet.asi"), "ProcessShader");
			printf("[DivaImGui] fnDNProcessShader=%p\n", fnDNProcessShader);
			fnDNRefreshShaders = (DNRefreshShaders)GetProcAddress(GetModuleHandle(L"DivaImGuiDotNet.asi"), "RefreshShaders");
			printf("[DivaImGui] fnDNRefreshShaders=%p\n", fnDNRefreshShaders);

			//if ((dirExists(path) == 1) && (*fnDNInitialize != nullptr)) {
			if ((dirExists(path) == 1) && (*fnDNInitialize != nullptr)) {
				
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
				shaderafthookd = true;
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

			glewInit();

			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourDetach(&(PVOID&)wGlGetProcAddress, (PVOID)hWGlGetProcAddress);
			DetourTransactionCommit();
		}
		return leproc;
	}

	void RefreshShaders(HDC hdc = NULL)
	{
		if (shaderafthookd)
			fnDNRefreshShaders();

		glBindFramebufferEXT(GL_FRAMEBUFFER, 0);

		for (int i = 0; i < allocatedshadercount; i++)
		{
			if (allocatedshaders[i] != 1)
			{
				int newlen = -1;
				if (shaderafthookd)
					newlen = fnDNProcessShader((int)allocshdorigptr[i], (int)allocshdptr[i], allocshdlen[i], allocshdenum[i], allocatedshaders[i]);
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
			{
				//hGetProcIDDLL = LoadLibrary(L"DivaImGuiDotNet.dll");

				
			}

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
				//fnDNInitialize(9);
				//fnDNRefreshShaders();
				init2 = true;
			}
		}

		if (refreshshd == 1)
		{
			RefreshShaders(hdc);
			refreshshd = 0;
		}
		//printf("%d\n", allocatedshadercount);
	}
}