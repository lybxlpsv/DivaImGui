#pragma once
#include <windows.h>

namespace DivaImGui::Input
{
	const int KEYBOARD_KEYS = 0xFF;

	struct KeyboardState
	{
		BYTE KeyStates[KEYBOARD_KEYS];
	
		bool IsDown(BYTE keycode);
	};
}

