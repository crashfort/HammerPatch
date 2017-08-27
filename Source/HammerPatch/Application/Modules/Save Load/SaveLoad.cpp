#include "PrecompiledHeader.hpp"
#include "Application\Application.hpp"

namespace
{
	struct Vector3
	{
		float X;
		float Y;
		float Z;
	};

	struct PlaneWinding
	{
		int Numpoints;
		Vector3* Points;
	};
}

namespace
{
	struct ScopedFile
	{
		ScopedFile(const char* path, const char* mode)
		{
			Assign(path, mode);
		}

		~ScopedFile()
		{
			Close();
		}

		void Close()
		{
			if (Handle)
			{
				fclose(Handle);
				Handle = nullptr;
			}
		}

		auto Get() const
		{
			return Handle;
		}

		explicit operator bool() const
		{
			return Get() != nullptr;
		}

		bool Assign(const char* path, const char* mode)
		{
			Close();

			Handle = fopen(path, mode);
			return Handle != nullptr;
		}

		template <typename... Types>
		bool WriteSimple(Types&&... args)
		{
			size_t adder[] =
			{
				[&]()
				{
					return fwrite(&args, sizeof(args), 1, Get());
				}()...
			};

			for (auto value : adder)
			{
				if (value == 0)
				{
					return false;
				}
			}

			return true;
		}

		size_t WriteRegion(void* start, size_t size, int count = 1)
		{
			return fwrite(start, size, count, Get());
		}

		template <typename... Args>
		int WriteText(const char* format, Args&&... args)
		{
			return fprintf_s(Get(), format, std::forward<Args>(args)...);
		}

		template <typename... Types>
		bool ReadSimple(Types&... args)
		{
			size_t adder[] =
			{
				[&]()
				{
					return fread_s(&args, sizeof(args), sizeof(args), 1, Get());
				}()...
			};

			for (auto value : adder)
			{
				if (value == 0)
				{
					return false;
				}
			}

			return true;
		}

		template <typename T>
		size_t ReadRegion(std::vector<T>& vec, int count = 1)
		{
			return fread_s(&vec[0], vec.size() * sizeof(T), sizeof(T), count, Get());
		}

		void SeekAbsolute(size_t pos)
		{
			fseek(Get(), pos, SEEK_SET);
		}

		FILE* Handle = nullptr;
	};

	struct MapFace
	{
		static Vector3* GetPointsPtr(void* thisptr)
		{
			/*
				Static structure offset May 8 2017
			*/
			HAP::StructureWalker walker(thisptr);
			auto ret = *(Vector3**)walker.Advance(340);

			return ret;
		}

		static int GetPointCount(void* thisptr)
		{
			/*
				Static structure offset May 8 2017
			*/
			HAP::StructureWalker walker(thisptr);
			auto ret = *(int*)walker.Advance(344);

			return ret;
		}

		static int GetFaceID(void* thisptr)
		{
			/*
				Static structure offset May 8 2017
			*/
			HAP::StructureWalker walker(thisptr);
			auto ret = *(int*)walker.Advance(412);
			
			return ret;
		}

		int ParentSolidID;
		int ID;

		std::vector<Vector3> Points;
	};

	struct MapSolid
	{
		static int GetID(void* thisptr)
		{
			/*
				Static structure offset May 8 2017
			*/
			HAP::StructureWalker walker(thisptr);
			auto ret = *(int*)walker.Advance(164);

			return ret;
		}

		static int GetFaceCount(void* thisptr)
		{
			/*
				Static structure offset May 8 2017
			*/
			HAP::StructureWalker walker(thisptr);
			auto ret = *(short*)walker.Advance(556);

			return ret;
		}

		int ID;
		std::vector<MapFace> Faces;
	};

	struct VertexSharedData
	{
		struct
		{
			/*
				This must always be the first 4 bytes.
				Anything after can vary per version.
			*/
			int32_t FileVersion;

			int NumberOfSolids;
		} FileHeader;

		char VertexFileName[1024];

		ScopedFile* VertFilePtr;

		bool IsLoading = false;
		bool IsSaving = false;
	} SharedData;

	struct VertexSaveData
	{
		enum
		{
			Version = 1
		};

		ScopedFile* TextFilePtr;
	} SaveData;

	struct VertexLoadData
	{
		void LoadVertexFile(ScopedFile* fileptr)
		{
			Solids.clear();

			int32_t fileversion;
			fileptr->ReadSimple(fileversion);

			HAP::MessageNormal
			(
				"HAP: Master version: %d\n"
				"HAP: Map version: %d\n",
				VertexSaveData::Version,
				fileversion
			);

			fileptr->SeekAbsolute(0);
			fileptr->ReadSimple(SharedData.FileHeader);

			Solids.resize(SharedData.FileHeader.NumberOfSolids);

			for (auto& cursolid : Solids)
			{
				int facecount;

				SharedData.VertFilePtr->ReadSimple
				(
					cursolid.ID,
					facecount
				);

				cursolid.Faces.resize(facecount);

				for (auto& curface : cursolid.Faces)
				{
					int pointscount;

					SharedData.VertFilePtr->ReadSimple
					(
						curface.ID,
						pointscount
					);

					curface.Points.resize(pointscount);

					SharedData.VertFilePtr->ReadRegion
					(
						curface.Points,
						pointscount
					);

					curface.ParentSolidID = cursolid.ID;
				}
			}
		}

		MapFace* FindFaceByID(int id)
		{
			for (auto& solid : LoadData.Solids)
			{
				for (auto& face : solid.Faces)
				{
					if (face.ID == id)
					{
						return &face;
					}
				}
			}

			return nullptr;
		}

		std::vector<MapSolid> Solids;
	} LoadData;
}

namespace
{
	namespace Module_MapDocLoad
	{
		#pragma region Init

		/*
			0x10097AC0 static Hammer IDA address May 8 2017
		*/
		auto Pattern = HAP::MemoryPattern
		(
			"\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x85\xC0\x53"
		);

		auto Mask =
		(
			"xxxxxx????xx????xxxx????xx????x????xxx"
		);

		bool __fastcall Override(void* thisptr, void* edx, const char* filename, bool unk);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook{"hammer_dll.dll", "MapDocLoad", Override, Pattern, Mask};

		#pragma endregion

		bool __fastcall Override(void* thisptr, void* edx, const char* filename, bool unk)
		{
			SharedData.IsLoading = true;

			strcpy_s(SharedData.VertexFileName, filename);
			PathRenameExtensionA(SharedData.VertexFileName, ".hpverts");

			ScopedFile file(SharedData.VertexFileName, "rb");
			SharedData.VertFilePtr = &file;

			if (!file)
			{
				SharedData.VertFilePtr = nullptr;

				HAP::MessageError("HAP: Could not open vertex file\n");
			}

			else
			{
				LoadData.LoadVertexFile(SharedData.VertFilePtr);
			}

			auto ret = ThisHook.GetOriginal()(thisptr, edx, filename, unk);

			if (SharedData.VertFilePtr)
			{
				char actualname[1024];
				strcpy_s(actualname, filename);

				PathStripPathA(actualname);
				PathRemoveExtensionA(actualname);

				HAP::MessageNormal("HAP: Loaded map \"%s\"\n", actualname);

				/*
					This memory is not used anymore
				*/
				LoadData.Solids.clear();
			}

			SharedData.VertFilePtr = nullptr;
			SharedData.IsLoading = false;

			return ret;
		}
	}

	namespace Module_MapDocSave
	{
		#pragma region Init

		/*
			0x100A36E0 static Hammer IDA address May 8 2017
		*/
		auto Pattern = HAP::MemoryPattern
		(
			"\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x81\xEC\x00\x00\x00\x00\x53\x56\x57\x8B\xF9\x8D\x8D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xC7\x45\x00\x00\x00\x00\x00\x8D\x8D\x00\x00\x00\x00\x8B\x5D\x08\x6A\x01\x53\xE8\x00\x00\x00\x00\x8B\xCF\x8B\xF0\xE8\x00\x00\x00\x00\x68\x00\x00\x00\x00\xE8\x00\x00\x00\x00"
		);

		auto Mask =
		(
			"xxxxxx????xx????xxxx????xx????xxxxxxx????x????xx"
			"?????xx????xxxxxxx????xxxxx????x????x????"
		);

		bool __fastcall Override(void* thisptr, void* edx, const char* filename, int saveflags);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook{"hammer_dll.dll", "MapDocSave", Override, Pattern, Mask};

		#pragma endregion

		bool __fastcall Override(void* thisptr, void* edx, const char* filename, int saveflags)
		{
			SharedData.IsSaving = true;

			strcpy_s(SharedData.VertexFileName, filename);
			PathRenameExtensionA(SharedData.VertexFileName, ".hpverts");

			ScopedFile vertfile(SharedData.VertexFileName, "wb");
			SharedData.VertFilePtr = &vertfile;

			if (!vertfile)
			{
				SharedData.VertFilePtr = nullptr;

				HAP::MessageError("HAP: Could not create vertex file\n");
			}

			char textfilename[1024];
			strcpy_s(textfilename, filename);
			PathRenameExtensionA(textfilename, ".hpvertstext");

			ScopedFile textfile(textfilename, "wb");
			SaveData.TextFilePtr = &textfile;

			if (!textfile)
			{
				SaveData.TextFilePtr = nullptr;

				HAP::MessageError("HAP: Could not create vertex text file\n");
			}

			if (vertfile)
			{
				/*
					Reset the header fields, it all gets overwritten later.
				*/
				SharedData.FileHeader = {};
				SharedData.FileHeader.FileVersion = VertexSaveData::Version;

				vertfile.WriteSimple(SharedData.FileHeader);
			}

			auto ret = ThisHook.GetOriginal()(thisptr, edx, filename, saveflags);

			if (vertfile)
			{
				vertfile.SeekAbsolute(0);
				vertfile.WriteSimple(SharedData.FileHeader);
			}

			SaveData.TextFilePtr = nullptr;
			SharedData.VertFilePtr = nullptr;
			SharedData.IsSaving = false;

			return ret;
		}
	}

	namespace Module_MapSolidSave
	{
		#pragma region Init

		/*
			0x10149C90 static Hammer IDA address May 8 2017
		*/
		auto Pattern = HAP::MemoryPattern
		(
			"\x55\x8B\xEC\x83\xEC\x08\x57\x8B\xF9\x8B\x4D\x0C\x57\x89\x7D\xFC\xE8\x00\x00\x00\x00\x84\xC0"
		);

		auto Mask =
		(
			"xxxxxxxxxxxxxxxxx????xx"
		);

		int __fastcall Override(void* thisptr, void* edx, void* file, void* saveinfo);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook{"hammer_dll.dll", "MapSolidSave", Override, Pattern, Mask};

		#pragma endregion

		int __fastcall Override(void* thisptr, void* edx, void* file, void* saveinfo)
		{
			if (SharedData.VertFilePtr)
			{
				SharedData.FileHeader.NumberOfSolids++;

				auto id = MapSolid::GetID(thisptr);
				auto facecount = MapSolid::GetFaceCount(thisptr);

				SharedData.VertFilePtr->WriteSimple(id, facecount);

				if (SaveData.TextFilePtr)
				{
					SaveData.TextFilePtr->WriteText("solid id: %d\n", id);
				}
			}

			auto ret = ThisHook.GetOriginal()(thisptr, edx, file, saveinfo);
			return ret;
		}
	}

	namespace Module_MapFaceCreateFaceFromWinding
	{
		#pragma region Init

		/*
			0x101302A0 static Hammer IDA address May 9 2017
		*/
		auto Pattern = HAP::MemoryPattern
		(
			"\x55\x8B\xEC\x51\x53\x56\x57\x8B\xF1\x6A\x00\x89\x75\xFC\xE8\x00\x00\x00\x00\x8B\x7D\x08\x83\xC4\x04\x8B\xCE\xFF\x37\xE8\x00\x00\x00\x00\x33\xDB"
		);

		auto Mask =
		(
			"xxxxxxxxxxxxxxx????xxxxxxxxxxx????xx"
		);

		void __fastcall Override(void* thisptr, void* edx, PlaneWinding* winding, int flags);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook{"hammer_dll.dll", "MapFaceCreateFaceFromWinding", Override, Pattern, Mask};

		#pragma endregion

		void __fastcall Override(void* thisptr, void* edx, PlaneWinding* winding, int flags)
		{
			if (SharedData.IsLoading)
			{
				auto id = MapFace::GetFaceID(thisptr);

				auto sourceface = LoadData.FindFaceByID(id);

				if (sourceface)
				{
					for (size_t i = 0; i < winding->Numpoints; i++)
					{
						winding->Points[i] = sourceface->Points[i];
					}
				}

				else
				{
					HAP::MessageError("HAP: No saved face with id %d\n", id);
				}

				flags = 0;
			}

			ThisHook.GetOriginal()(thisptr, edx, winding, flags);
		}
	}

	namespace Module_MapFaceSave
	{
		#pragma region Init

		/*
			0x10135C30 static Hammer IDA address May 7 2017
		*/
		auto Pattern = HAP::MemoryPattern
		(
			"\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\x56\x57\x8B\xF1\xE8\x00\x00\x00\x00\x6A\x00\x8B\xCE\xE8\x00\x00\x00\x00"
		);

		auto Mask =
		(
			"xxxxx????xxxxx????xxxxx????"
		);

		int __fastcall Override(void* thisptr, void* edx, void* file, void* saveinfo);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook{"hammer_dll.dll", "MapFaceSave", Override, Pattern, Mask};

		#pragma endregion

		int __fastcall Override(void* thisptr, void* edx, void* file, void* saveinfo)
		{
			if (SharedData.VertFilePtr)
			{				
				auto pointsaddr = MapFace::GetPointsPtr(thisptr);
				auto pointscount = MapFace::GetPointCount(thisptr);
				auto faceid = MapFace::GetFaceID(thisptr);

				SharedData.VertFilePtr->WriteSimple(faceid, pointscount);

				SharedData.VertFilePtr->WriteRegion(pointsaddr, sizeof(Vector3), pointscount);

				if (SaveData.TextFilePtr)
				{
					SaveData.TextFilePtr->WriteText("\tface id: %d\n", faceid);

					for (size_t i = 0; i < pointscount; i++)
					{
						auto vec = pointsaddr[i];
						SaveData.TextFilePtr->WriteText("\t\t[%g %g %g]\n", vec.X, vec.Y, vec.Z);
					}
				}
			}

			auto ret = ThisHook.GetOriginal()(thisptr, edx, file, saveinfo);
			return ret;
		}
	}
}
