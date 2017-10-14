#include "PrecompiledHeader.hpp"
#include "Application.hpp"
#include <cctype>

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

			bool IsValid() const
			{
				return StdOutHandle != INVALID_HANDLE_VALUE;
			}

			void Write(const char* message)
			{
				if (!IsValid())
				{
					return;
				}

				WriteConsoleA(StdOutHandle, message, strlen(message), nullptr, nullptr);
			}
		} Console;
	};

	Application MainApplication;

	namespace Memory
	{
		/*
			Not accessing the STL iterators in debug mode makes this run >10x faster, less sitting around waiting for nothing.
		*/
		inline bool DataCompare(const uint8_t* data, const HAP::BytePattern::Entry* pattern, size_t patternlength)
		{
			int index = 0;

			for (size_t i = 0; i < patternlength; i++)
			{
				auto byte = *pattern;

				if (!byte.Unknown && *data != byte.Value)
				{
					return false;
				}
				
				++data;
				++pattern;
				++index;
			}

			return index == patternlength;
		}

		void* FindPattern(void* start, size_t searchlength, const HAP::BytePattern& pattern)
		{
			auto patternstart = pattern.Bytes.data();
			auto length = pattern.Bytes.size();
			
			for (size_t i = 0; i <= searchlength - length; ++i)
			{
				auto addr = (const uint8_t*)(start) + i;
				
				if (DataCompare(addr, patternstart, length))
				{
					return (void*)(addr);
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
		SetConsoleTitleA("HammerPatch Console");
		ShowWindow(GetConsoleWindow(), SW_SHOWMINNOACTIVE);
	}
}

void HAP::CreateModules()
{
	auto res = MH_Initialize();

	if (res != MH_OK)
	{
		MessageWarning("HAP: Failed to initialize hooks\n");
		throw res;
	}

	MessageNormal("HAP: Creating %d modules\n", MainApplication.Modules.size());

	for (auto module : MainApplication.Modules)
	{
		auto res = module->Create();
		auto name = module->DisplayName;

		if (res != MH_OK)
		{
			MessageWarning("HAP: Could not enable module \"%s\": \"%s\"\n", name, MH_StatusToString(res));
			throw res;
		}

		auto library = module->Module;
		auto function = module->TargetFunction;

		MH_EnableHook(function);

		MessageNormal("HAP: Enabled module \"%s\" -> %s @ 0x%p\n", name, library, function);
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
	MainApplication.Console.Write(message);
}

void HAP::MessageWarning(const char* message)
{
	MainApplication.Console.Write(message);
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

	MessageNormal("HAP: Calling %d startup procedures\n", funcs.size());

	for (const auto& entry : funcs)
	{
		auto res = entry.Function();

		if (!res)
		{
			throw entry.Name;
		}

		MessageNormal("HAP: Startup procedure \"%s\" passed\n", entry.Name);
	}
}

HAP::BytePattern HAP::GetPatternFromString(const char* input)
{
	BytePattern ret;

	while (*input)
	{
		if (std::isspace(*input))
		{
			++input;
		}

		BytePattern::Entry entry;

		if (std::isxdigit(*input))
		{
			entry.Unknown = false;
			entry.Value = std::strtol(input, nullptr, 16);

			input += 2;
		}

		else
		{
			entry.Unknown = true;
			input += 2;
		}

		ret.Bytes.emplace_back(entry);
	}

	return ret;
}

void* HAP::GetAddressFromPattern(const ModuleInformation& library, const BytePattern& pattern)
{
	return Memory::FindPattern(library.MemoryBase, library.MemorySize, pattern);
}

void HAP::AddModule(HookModuleBase* module)
{
	MainApplication.Modules.emplace_back(module);
}

bool HAP::IsGame(const wchar_t* test)
{
	/*
		Takes the current directory, goes up a step, gets the directory name and compares
		against parameter "test".
	*/

	wchar_t directory[4096];
	GetCurrentDirectoryW(sizeof(directory), directory);
	PathRemoveFileSpecW(directory);
	
	auto name = PathFindFileNameW(directory);

	return _wcsicmp(name, test) == 0;
}

bool HAP::IsCSGO()
{
	return IsGame(L"Counter-Strike Global Offensive");
}
