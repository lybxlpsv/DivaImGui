#include "KeyboardState.h"

namespace DivaImGui::Input
{
	bool KeyboardState::IsDown(BYTE keycode)
	{
		return KeyStates[keycode];
	}
}
