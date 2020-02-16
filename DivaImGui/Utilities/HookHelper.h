#define PRINT(value, ...) printf(value, __VA_ARGS__);

#define PROC_ADDRESS(libraryName, procName) \
	GetProcAddress(LoadLibrary(TEXT(libraryName)), procName)

#define SAFE_RELEASE(object) if (object) { object->Release(); object = NULL; }

#define VTABLE_HOOK(returnType, className, functionName, ...) \
	typedef returnType functionName(className* This, __VA_ARGS__); \
	functionName* original##functionName; \
	returnType implOf##functionName(className* This, __VA_ARGS__)

#define INSTALL_VTABLE_HOOK(object, functionName, functionIndex) \
	{ \
		void** addr = &(*(void***)object)[functionIndex]; \
		if (*addr != implOf##functionName) \
		{ \
			PRINT("[DivaImGui] Installing %s hook...\n", #functionName); \
			original##functionName = (functionName*)*addr; \
			DWORD oldProtect; \
			VirtualProtect(addr, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect); \
			*addr = implOf##functionName; \
			VirtualProtect(addr, sizeof(void*), oldProtect, NULL); \
		} \
	}

#define HOOK(returnType, function, location, ...) \
    typedef returnType function(__VA_ARGS__); \
    function* original##function = (function*)location; \
    returnType implOf##function(__VA_ARGS__)

#define INSTALL_HOOK(functionName) \
	{ \
		PRINT("[DivaImGui] Installing %s hook...\n", #functionName); \
		DetourTransactionBegin(); \
		DetourUpdateThread(GetCurrentThread()); \
		DetourAttach((void**)&original##functionName, implOf##functionName); \
		DetourTransactionCommit(); \
	}

#define ASSERT(function) \
	if (FAILED(result = (function))) \
	{ \
		PRINT("[DivaImGui] %s failed (error code: 0x%08x)\n", #function, (unsigned int)result); \
		goto end; \
	} /*\
	else \
	{ \
		PRINT("[DivaMovie] %s succeeded\n", #function); \
	}*/