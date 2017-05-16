#include "PrecompiledHeader.hpp"
#include "Application.hpp"

namespace
{
	struct Application
	{
		std::vector<HAP::HookModuleBase*> Modules;
		std::vector<HAP::ShutdownFuncType> CloseFunctions;
		std::vector<HAP::StartupFuncData> StartupFunctions;

		struct
		{
			HANDLE StdOutHandle = INVALID_HANDLE_VALUE;
			WORD DefaultAttributes;

			bool IsValid() const
			{
				return StdOutHandle != INVALID_HANDLE_VALUE;
			}

			void Write
			(
				const char* message,
				WORD attributes = 0
			)
			{
				if (!IsValid())
				{
					return;
				}

				SetConsoleTextAttribute
				(
					StdOutHandle,
					attributes
				);

				WriteConsoleA
				(
					StdOutHandle,
					message,
					strlen(message),
					nullptr,
					nullptr
				);

				SetConsoleTextAttribute
				(
					StdOutHandle,
					DefaultAttributes
				);
			}
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

void HAP::CreateConsole()
{
	AllocConsole();

	auto& console = MainApplication.Console;

	console.StdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

	if (console.IsValid())
	{
		CONSOLE_SCREEN_BUFFER_INFO coninfo;

		GetConsoleScreenBufferInfo
		(
			console.StdOutHandle,
			&coninfo
		);

		console.DefaultAttributes = coninfo.wAttributes;

		SetConsoleTitleA("HammerPatch Console");

		ShowWindow
		(
			GetConsoleWindow(),
			SW_SHOWMINNOACTIVE
		);
	}
}

void HAP::CreateModules()
{
	auto res = MH_Initialize();

	if (res != MH_OK)
	{
		MessageError("HAP: Failed to initialize hooks\n");
		throw res;
	}

	MessageNormal
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
			MessageError
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

		MessageNormal
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
	for (auto&& func : MainApplication.CloseFunctions)
	{
		func();
	}

	FreeConsole();

	MH_Uninitialize();
}

void HAP::MessageNormal(const char* message)
{
	MainApplication.Console.Write
	(
		message
	);
}

void HAP::MessageWarning(const char* message)
{
	MainApplication.Console.Write
	(
		message,
		FOREGROUND_RED | FOREGROUND_GREEN
	);
}

void HAP::MessageError(const char* message)
{
	MainApplication.Console.Write
	(
		message,
		FOREGROUND_RED
	);

	ShowWindow
	(
		GetConsoleWindow(),
		SW_SHOW
	);
}

void HAP::AddShutdownFunction(ShutdownFuncType function)
{
	MainApplication.CloseFunctions.emplace_back(function);
}

void HAP::AddStartupFunction(const StartupFuncData& data)
{
	MainApplication.StartupFunctions.emplace_back(data);
}

void HAP::CallStartupFunctions()
{
	const auto& funcs = MainApplication.StartupFunctions;

	if (funcs.empty())
	{
		return;
	}

	MessageNormal
	(
		"HAP: Calling %d startup procedures\n",
		funcs.size()
	);

	for (const auto& entry : funcs)
	{
		auto res = entry.Function();

		if (!res)
		{
			throw entry.Name;
		}

		MessageNormal
		(
			"HAP: Startup procedure \"%s\" passed\n",
			entry.Name
		);
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
