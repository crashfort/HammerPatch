#include "PrecompiledHeader.hpp"
#include <process.h>

#include "Application\Application.hpp"

namespace
{
	enum
	{
		ApplicationVersion = 1
	};
}

namespace
{
	unsigned int __stdcall MainThread(void* args)
	{
		HAP::CreateConsole();

		HAP::MessageNormal
		(
			"HAP: Current version: %d\n",
			ApplicationVersion
		);

		try
		{
			HAP::CreateModules();
		}

		catch (MH_STATUS status)
		{
			return 1;
		}

		try
		{
			HAP::CallStartupFunctions();
		}

		catch (const char* name)
		{
			HAP::MessageError
			(
				"HAP: Startup procedure \"%s\" failed\n",
				name
			);

			return 1;
		}

		HAP::MessageNormal
		(
			"HAP: HammerPatch loaded\n"
		);

		return 1;
	}
}

BOOL APIENTRY DllMain
(
	HMODULE module,
	DWORD reason,
	LPVOID
)
{
	switch (reason)
	{
		case DLL_PROCESS_ATTACH:
		{
			_beginthreadex
			(
				nullptr,
				0,
				MainThread,
				nullptr,
				0,
				nullptr
			);

			break;
		}

		case DLL_PROCESS_DETACH:
		{
			HAP::Close();
			break;
		}
	}

	return 1;
}
