#pragma once

namespace HAP
{
	void CreateConsole();
	void CreateModules();
	void Close();

	using MessageFuncType = void(*)(const char*);

	template <typename... Args>
	void Message(MessageFuncType function, const char* format, Args&&... args)
	{
		if (sizeof...(args) == 0)
		{
			function(format);
			return;
		}

		char buf[1024];
		sprintf_s(buf, format, std::forward<Args>(args)...);

		function(buf);
	}

	void MessageNormal(const char* message);

	template <typename... Args>
	void MessageNormal(const char* format, Args&&... args)
	{
		Message(MessageNormal, format, std::forward<Args>(args)...);
	}

	void MessageWarning(const char* message);

	template <typename... Args>
	void MessageWarning(const char* format, Args&&... args)
	{
		Message(MessageWarning, format, std::forward<Args>(args)...);
	}

	using ShutdownFuncType = void(*)();
	void AddShutdownFunction(ShutdownFuncType function);

	struct ShutdownFunctionAdder
	{
		ShutdownFunctionAdder(ShutdownFuncType function)
		{
			AddShutdownFunction(function);
		}
	};

	struct StartupFuncData
	{
		using FuncType = bool(*)();
		
		const char* Name;
		FuncType Function;
	};

	void AddStartupFunction(const StartupFuncData& data);

	struct StartupFunctionAdder
	{
		StartupFunctionAdder(const char* name, StartupFuncData::FuncType function)
		{
			StartupFuncData data;
			data.Name = name;
			data.Function = function;

			AddStartupFunction(data);
		}
	};

	void CallStartupFunctions();

	struct ModuleInformation
	{
		ModuleInformation(const char* name) : Name(name)
		{
			MODULEINFO info;			
			K32GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(name), &info, sizeof(info));

			MemoryBase = info.lpBaseOfDll;
			MemorySize = info.SizeOfImage;
		}

		const char* Name;

		void* MemoryBase;
		size_t MemorySize;
	};

	struct BytePattern
	{
		struct Entry
		{
			uint8_t Value;
			bool Unknown;
		};

		std::vector<Entry> Bytes;
	};

	BytePattern GetPatternFromString(const char* input);
	void* GetAddressFromPattern(const ModuleInformation& library, const BytePattern& pattern);

	struct HookModuleBase
	{
		HookModuleBase(const char* module, const char* name, void* newfunc) :
			DisplayName(name),
			Module(module),
			NewFunction(newfunc)
		{

		}

		const char* DisplayName;
		const char* Module;

		void* TargetFunction;
		void* NewFunction;
		void* OriginalFunction;

		virtual MH_STATUS Create() = 0;
	};

	void AddModule(HookModuleBase* module);

	template <typename FuncSignature>
	class HookModuleMask final : public HookModuleBase
	{
	public:
		HookModuleMask(const char* module, const char* name, FuncSignature newfunction, const char* pattern) :
			HookModuleBase(module, name, newfunction),
			Pattern(GetPatternFromString(pattern))
		{
			AddModule(this);
		}

		inline auto GetOriginal() const
		{
			return static_cast<FuncSignature>(OriginalFunction);
		}

		virtual MH_STATUS Create() override
		{
			ModuleInformation info(Module);
			TargetFunction = GetAddressFromPattern(info, Pattern);

			auto res = MH_CreateHookEx(TargetFunction, NewFunction, &OriginalFunction);
			return res;
		}

	private:
		BytePattern Pattern;
	};

	struct StructureWalker
	{
		StructureWalker(void* address) :
			Address(static_cast<uint8_t*>(address)),
			Start(Address)
		{

		}

		template <typename Modifier = uint8_t>
		uint8_t* Advance(int offset)
		{
			Address += offset * sizeof(Modifier);
			return Address;
		}

		template <typename Modifier = uint8_t>
		uint8_t* AdvanceAbsolute(int offset)
		{
			Reset();
			Address += offset * sizeof(Modifier);
			return Address;
		}

		void Reset()
		{
			Address = Start;
		}

		uint8_t* Address;
		uint8_t* Start;
	};

	bool IsGame(const wchar_t* test);
	bool IsCSGO();
}
