#include <Windows.h>
#include <Shlwapi.h>
#include <comdef.h>
#include <wrl.h>
#include <stdint.h>
#include <cstdio>

#include <thread>
#include <chrono>

using namespace std::literals;

namespace
{
	namespace MS
	{
		inline void ThrowLastError()
		{
			auto error = GetLastError();
			throw HRESULT_FROM_WIN32(error);
		}

		template <typename T>
		inline void ThrowIfZero(T value)
		{
			if (value == 0)
			{
				ThrowLastError();
			}
		}
	}
}

namespace
{
	namespace App
	{
		enum class ExceptionType : uint32_t
		{
			CouldNotLoadLibrary
		};

		const wchar_t* ExceptionToString(ExceptionType code)
		{
			static const wchar_t* table[] =
			{
				L"Could not load injection library"
			};

			auto index = static_cast<uint32_t>(code);

			return table[index];
		}
	}
}

namespace
{
	template <size_t Size>
	void RemoveFileName(wchar_t(&buffer)[Size])
	{
		PathRemoveFileSpecW(buffer);
		wcscat_s(buffer, L"\\");
	}

	struct ProcessWriter
	{
		ProcessWriter
		(
			HANDLE process,
			void* startaddress
		) :
			Process(process),
			Address(static_cast<uint8_t*>(startaddress))
		{

		}

		auto PushMemory(void* address, size_t size)
		{
			SIZE_T written;

			MS::ThrowIfZero
			(
				WriteProcessMemory
				(
					Process,
					Address,
					address,
					size,
					&written
				)
			);

			auto retaddr = Address;

			Address += written;

			return retaddr;
		}

		HANDLE Process;
		uint8_t* Address;
	};

	PROCESS_INFORMATION StartProcess
	(
		const wchar_t* path
	)
	{
		/*
			Always make the process' working directory
			the same as its path.
		*/
		wchar_t rundir[1024];
		wcscpy_s(rundir, path);

		RemoveFileName(rundir);

		wchar_t args[1024];
		wcscpy_s(args, path);

		STARTUPINFOW startinfo = {};
		startinfo.cb = sizeof(startinfo);

		PROCESS_INFORMATION procinfo;

		MS::ThrowIfZero
		(
			CreateProcessW
			(
				path,
				args,
				nullptr,
				nullptr,
				false,

				/*
					This really should be created with CREATE_SUSPENDED
					but Hammer gives an error message in that case, don't know
					where it comes from.
				*/
				DETACHED_PROCESS,
				nullptr,
				rundir,
				&startinfo,
				&procinfo
			)
		);

		return procinfo;
	}

	struct VirtualMemory
	{
		VirtualMemory
		(
			HANDLE process,
			size_t size,
			DWORD flags = MEM_COMMIT | MEM_RESERVE,
			DWORD protect = PAGE_READWRITE
		) :
			Process(process)
		{
			Address = VirtualAllocEx
			(
				process,
				nullptr,
				size,
				flags,
				protect
			);

			if (!Address)
			{
				MS::ThrowLastError();
			}
		}

		~VirtualMemory()
		{
			VirtualFreeEx
			(
				Process,
				Address,
				0,
				MEM_RELEASE
			);
		}
			
		void* Address = nullptr;
		HANDLE Process;
	};

	using ScopedHandle = Microsoft::WRL::Wrappers::HandleT
	<
		Microsoft::WRL::Wrappers::HandleTraits::HANDLENullTraits
	>;

	void Inject(HANDLE process)
	{
		wchar_t dllname[] = L"HammerPatch.dll";

		VirtualMemory memory(process, sizeof(dllname));
		ProcessWriter writer(process, memory.Address);
		
		auto stringaddr = writer.PushMemory(dllname, sizeof(dllname));

		ScopedHandle loaderthread
		(
			CreateRemoteThread
			(
				process,
				nullptr,
				0,
				(LPTHREAD_START_ROUTINE)LoadLibraryW,
				stringaddr,
				0,
				nullptr
			)
		);

		if (!loaderthread.IsValid())
		{
			MS::ThrowLastError();
		}

		auto waitres = WaitForSingleObject
		(
			loaderthread.Get(),
			INFINITE
		);

		if (waitres != WAIT_OBJECT_0)
		{
			MS::ThrowLastError();
		}

		DWORD exitcode;

		MS::ThrowIfZero
		(
			GetExitCodeThread
			(
				loaderthread.Get(),
				&exitcode
			)
		);

		if (exitcode == 0)
		{
			throw App::ExceptionType::CouldNotLoadLibrary;
		}
	}
}

void wmain(int argc, wchar_t* argv[])
{
	wchar_t hammerexe[1024];
	GetModuleFileNameW(nullptr, hammerexe, sizeof(hammerexe));
	RemoveFileName(hammerexe);
	wcscat_s(hammerexe, L"hammer.exe");

	try
	{
		auto info = StartProcess(hammerexe);

		ScopedHandle process(info.hProcess);
		ScopedHandle thread(info.hThread);

		/*
			Zzz. Wait for Hammer to load up I guess.
			Why doesn't suspended start work again?
		*/
		std::this_thread::sleep_for(3s);

		{
			DWORD exitcode;

			MS::ThrowIfZero
			(
				GetExitCodeThread
				(
					thread.Get(),
					&exitcode
				)
			);

			/*
				Hammer closed too early.
			*/
			if (exitcode != STILL_ACTIVE)
			{
				return;
			}
		}

		Inject(info.hProcess);
	}

	catch (HRESULT code)
	{
		_com_error error(code);
		auto message = error.ErrorMessage();

		wprintf_s(L"Windows error: %s\n", message);
		wprintf_s(L"Press Enter to exit\n");
		
		std::getchar();

		return;
	}

	catch (App::ExceptionType code)
	{
		auto message = App::ExceptionToString(code);

		wprintf_s(L"Application error: %s\n", message);
		wprintf_s(L"Press Enter to exit\n");

		std::getchar();

		return;
	}
}
