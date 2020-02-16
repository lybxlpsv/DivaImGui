#pragma once
#include <Windows.h>
#include <string>
#include <filesystem>
#include "parser.hpp"
#include "Utilities/HookHelper.h"
#include "DebugHook701.h"

namespace DebugHooks
{
	//shd param
	HOOK(void*, sub_1405E52D0, 0x1405E52D0, unsigned int a1, __int64 a2, unsigned int a3)
	{
		printf("sub_1405E52D0 %d %d %d\n", a1, a2, a3);
		return originalsub_1405E52D0(a1, a2, a3);
	}

	HOOK(__int64*, sub_1405E5240, 0x1405E5240, unsigned int a1, __int64 a2, __int64 a3, unsigned int a4)
	{
		printf("sub_1405E5240 %d %d %d %d\n", a1, a2, a3, a4);
		return originalsub_1405E5240(a1, a2, a3, a4);
	}

	HOOK(signed __int64*, sub_1405E4B50, 0x1405E4B50, __int64 a1, __int64 a2)
	{
		//if (a1 == 1288011824)
			//if (a2 == 27)
		//if (_dbg_mmm_stuff1)
		{
			printf("sub_1405E4B50 %p\n", _ReturnAddress());
			printf("sub_1405E4B50 %d %d\n", a1, a2);
		}

		auto result = originalsub_1405E4B50(a1, a2);

		auto v2 = *(signed int*)(a1 + 12);

		auto v3 = 0;
		auto v6 = *(DWORD**)(a1 + 16);
		auto v4 = 0;
		__int64 v5 = 0;
		while (*v6 != (DWORD)a2)
		{
			++v5;
			++v4;
			v6 += 12;
			if (v5 >= v2)
				return 0;
		}
		auto v8 = (UINT64*)(*(UINT64*)(a1 + 16) + 48 * v4);
		if (!v8)
			return 0;
		auto v9 = *(signed int*)(a1 + 24);
		auto v10 = 1;
		auto v11 = 0;
		auto v12 = 1;
		if (v9 > 0)
		{
			auto v13 = (signed int*)v8[1];
			auto v14 = *(UINT64*)(a1 + 32) - (UINT64)v13;
			auto v15 = v8[2] - (UINT64)v13;
			do
			{
				auto v16 = *v13;
				a2 = *(unsigned int*)((char*)v13 + v15);
				auto v17 = *(DWORD*)0x14CC57AF0[(signed int*)((char*)v13 + v14)];
				++v13;
				auto v18 = v17;
				if (v17 > v16)
					v18 = v16;
				if (v17 > (signed int)a2)
					v17 = a2;
				v5 = (unsigned int)(v10 * v17);
				v11 += v12 * v18;
				v12 *= v16 + 1;
				v3 += v5;
				v10 *= (DWORD)a2 + 1;
				--v9;
			} while (v9);
		}
		auto v19 = *(DWORD*)(v8[4] + 4i64 * v11);
		auto v20 = *(DWORD*)(v8[5] + 4i64 * v3);

		//if (_dbg_mmm_stuff1)
		printf("v2=%d v6=%d v8[4]=%d v8[5]=%d v9=%d\n", v2, v6, v8[4], v8[5], v9);

		/*if (*(DWORD*)0x14CC587B0 != v19)
		{
			glBindProgramARB(34336i64, v19);
			*(DWORD*)0x14CC587B0 = v19;
		}
		if (*(DWORD*)0x14CC587B4 != v20)
		{
			glBindProgramARB(34820i64, v20);
			*(DWORD*)0x14CC587B0 = v20;
		}

		glEnable(GL_VERTEX_PROGRAM_ARB);
		glEnable(GL_FRAGMENT_PROGRAM_ARB);*/
		return 0;
	}

	HOOK(__int64*, sub_1404444B0, 0x1404444B0, __int64 a1, char a2)
	{
		{
			printf("sub_1404444B0 %p\n", _ReturnAddress());
			printf("sub_1404444B0 %d %d\n", a1, a2);
		}

		return originalsub_1404444B0(a1, a2);
	}

	HOOK(__int64*, sub_1404425B0, 0x1404425B0, __int64 a1)
	{
		{
			printf("sub_1404425B0 %p\n", _ReturnAddress());
			printf("sub_1404425B0 %d\n", a1);
		}

		return originalsub_1404425B0(a1);
	}

	HOOK(__int64*, sub_140437820, 0x140437820, __int64 a1, __int64 a2, void(__fastcall* a3)(__int64), unsigned int a4)
	{
		{
			printf("sub_140437820 %p\n", _ReturnAddress());
			printf("sub_140437820 %d %d %p %d\n", a1, a2, a3, a4);
		}
		return originalsub_140437820(a1, a2, a3, a4);
	}

}

