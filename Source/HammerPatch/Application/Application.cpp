#include "PrecompiledHeader.hpp"
#include "Application.hpp"

namespace
{
	struct Application
	{
		std::vector<HAP::HookModuleBase*> Modules;
		std::vector<HAP::ShutdownFuncType> OnCloseFunctions;
		std::vector<HAP::StartupFuncData> StartupFunctions;

		struct
		{
			HANDLE StdOutHandle = INVALID_HANDLE_VALUE;
			WORD DefaultAttributes;
		} Console;
	};

	Application MainApplication;

	/*
		From Yalter's SPT
	*/
	namespace Memory
	{
		inline bool DataCompare
		(
			const uint8_t* data,
			const uint8_t* pattern,
			const char* mask
		)
		{
			for (; *mask != 0; ++data, ++pattern, ++mask)
			{
				if (*mask == 'x' && *data != *pattern)
				{
					return false;
				}
			}

			return (*mask == 0);
		}

		void* FindPattern
		(
			const void* start,
			size_t length,
			const uint8_t* pattern,
			const char* mask
		)
		{
			auto masklength = strlen(mask);
			
			for (size_t i = 0; i <= length - masklength; ++i)
			{
				auto addr = reinterpret_cast<const uint8_t*>(start) + i;
				
				if (DataCompare(addr, pattern, mask))
				{
					return const_cast<void*>
					(
						reinterpret_cast<const void*>(addr)
					);
				}
			}

			return nullptr;
		}
	}
}

void HAP::Setup()
{
	{
		AllocConsole();

		auto& console = MainApplication.Console;

		console.StdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		if (console.StdOutHandle != INVALID_HANDLE_VALUE)
		{
			SetConsoleTitleA("HammerPatch Console");
			ShowWindow(GetConsoleWindow(), SW_MINIMIZE);
		}
	}

	auto res = MH_Initialize();

	if (res != MH_OK)
	{
		LogMessage("HAP: Failed to initialize hooks\n");
		throw res;
	}

	LogMessage
	(
		"HAP: Creating %d modules\n",
		MainApplication.Modules.size()
	);

	for (auto module : MainApplication.Modules)
	{
		auto res = module->Create();
		auto name = module->DisplayName;

		if (res != MH_OK)
		{
			LogMessage
			(
				"HAP: Could not enable module \"%s\": \"%s\"\n",
				name,
				MH_StatusToString(res)
			);

			throw res;
		}

		auto library = module->Module;
		auto function = module->TargetFunction;

		MH_EnableHook(function);

		LogMessage
		(
			"HAP: Enabled module \"%s\" -> %s @ 0x%p\n",
			name,
			library,
			function
		);
	}
}

void HAP::Close()
{
	for (auto&& func : MainApplication.OnCloseFunctions)
	{
		func();
	}

	FreeConsole();

	MH_Uninitialize();
}

void HAP::LogMessageText(const char* message)
{
	auto& console = MainApplication.Console;

	if (console.StdOutHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	WriteConsoleA
	(
		console.StdOutHandle,
		message,
		strlen(message),
		nullptr,
		nullptr
	);
}

void HAP::AddShutdownFunction(ShutdownFuncType function)
{
	MainApplication.OnCloseFunctions.emplace_back(function);
}

void HAP::AddStartupFunction(const StartupFuncData& data)
{
	MainApplication.StartupFunctions.emplace_back(data);
}

void HAP::CallStartupFunctions()
{
	LogMessage
	(
		"HAP: Calling %d startup procedures\n",
		MainApplication.StartupFunctions.size()
	);

	for (const auto& entry : MainApplication.StartupFunctions)
	{
		auto res = entry.Function();

		if (!res)
		{
			throw entry.Name;
		}

		LogMessage("HAP: Startup procedure \"%s\" passed\n", entry.Name);
	}
}

void HAP::AddModule(HookModuleBase* module)
{
	MainApplication.Modules.emplace_back(module);
}

void* HAP::GetAddressFromPattern
(
	const ModuleInformation& library,
	const uint8_t* pattern,
	const char* mask
)
{
	return Memory::FindPattern
	(
		library.MemoryBase,
		library.MemorySize,
		pattern,
		mask
	);
}
