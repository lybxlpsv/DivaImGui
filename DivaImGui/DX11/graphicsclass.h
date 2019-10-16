////////////////////////////////////////////////////////////////////////////////
// Filename: graphicsclass.h
////////////////////////////////////////////////////////////////////////////////
#ifndef _GRAPHICSCLASS_H_
#define _GRAPHICSCLASS_H_


///////////////////////
// MY CLASS INCLUDES //
///////////////////////
#include "d3dclass.h"
#include <gl/glew.h>
#include <gl/gl.h>

/////////////
// GLOBALS //
/////////////
const float SCREEN_DEPTH = 1000.0f;
const float SCREEN_NEAR = 0.1f;


////////////////////////////////////////////////////////////////////////////////
// Class name: GraphicsClass
////////////////////////////////////////////////////////////////////////////////
class GraphicsClass
{
public:
	GraphicsClass();
	GraphicsClass(const GraphicsClass&);
	~GraphicsClass();

	bool Initialize(int, int, HWND);
	void Shutdown();
	bool Frame();

	static bool FULL_SCREEN;
	static bool VSYNC_ENABLED;
	static int SWAPCHAIN_FORMAT;
	static int DISPLAY_FORMAT;
	static HDC currentHdc;
	static bool reinit;
private:
	bool Render();

private:
	D3DClass* m_D3D;
};

#endif