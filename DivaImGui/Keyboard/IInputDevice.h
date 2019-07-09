#pragma once

namespace DivaImGui::Input
{
	class IInputDevice
	{
	public:
		virtual bool PollInput() = 0;
	};
}
