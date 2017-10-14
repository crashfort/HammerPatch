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
			int offset = 0;

			if (HAP::IsCSGO())
			{
				/*
					Static CSGO structure offset August 27 2017
				*/
				offset = 564;
			}

			else
			{
				/*
					Static 2013 structure offset May 8 2017
				*/
				offset = 556;
			}

			HAP::StructureWalker walker(thisptr);
			auto ret = *(short*)walker.Advance(offset);

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

			HAP::MessageNormal("HAP: Master version: %d\n" "HAP: Map version: %d\n", VertexSaveData::Version, fileversion);

			fileptr->SeekAbsolute(0);
			fileptr->ReadSimple(SharedData.FileHeader);

			Solids.resize(SharedData.FileHeader.NumberOfSolids);

			for (auto& cursolid : Solids)
			{
				int facecount;

				SharedData.VertFilePtr->ReadSimple(cursolid.ID, facecount);
				cursolid.Faces.resize(facecount);

				for (auto& curface : cursolid.Faces)
				{
					int pointscount;

					SharedData.VertFilePtr->ReadSimple(curface.ID, pointscount);
					curface.Points.resize(pointscount);

					SharedData.VertFilePtr->ReadRegion(curface.Points, pointscount);

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
		/*
			0x10097AC0 static 2013 Hammer IDA address May 8 2017
			0x100A0B90 static CSGO Hammer IDA address August 27 2017
		*/
		auto Pattern2013 = "55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 ?? ?? ?? ?? 50 64 89 25 ?? ?? ?? ?? 81 EC ?? ?? ?? ?? A1 ?? ?? ?? ?? 85 C0 53 56 57 0F 94 C3 8B F9 40 88 5D F2 A3 ?? ?? ?? ?? 83 BF ?? ?? ?? ?? ?? 75 34 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 04 89 45 EC C7 45 ?? ?? ?? ?? ?? 85 C0 74 0A 57 8B C8 E8 ?? ?? ?? ?? EB 02";
		auto PatternCSGO = "55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 ?? ?? ?? ?? 50 64 89 25 ?? ?? ?? ?? 81 EC ?? ?? ?? ?? A1 ?? ?? ?? ?? 85 C0 53 8B D9 0F 94 C1 40 88 4D F2 56 A3 ?? ?? ?? ?? 83 BB ?? ?? ?? ?? ?? 75 37 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 04 89 45 E8 C7 45 ?? ?? ?? ?? ?? 85 C0 74 0A 53 8B C8 E8 ?? ?? ?? ?? EB 02";

		bool __fastcall Override(void* thisptr, void* edx, const char* filename, bool unk);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook("hammer_dll.dll", "MapDocLoad", Override, []()
		{
			if (HAP::IsCSGO())
			{
				return PatternCSGO;
			}

			return Pattern2013;
		}());

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
		/*
			0x100A36E0 static 2013 Hammer IDA address May 8 2017
			0x100B3770 static CSGO Hammer IDA address August 27 2017
		*/
		auto Pattern2013 = "55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 ?? ?? ?? ?? 50 64 89 25 ?? ?? ?? ?? 81 EC ?? ?? ?? ?? 53 56 57 8B F9 8D 8D ?? ?? ?? ?? E8 ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? 8D 8D ?? ?? ?? ?? 8B 5D 08 6A 01 53 E8 ?? ?? ?? ?? 8B CF 8B F0 E8 ?? ?? ?? ?? 68 ?? ?? ?? ??";
		auto PatternCSGO = "55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 ?? ?? ?? ?? 50 64 89 25 ?? ?? ?? ?? 81 EC ?? ?? ?? ?? 53 56 8B D9 8D 8D ?? ?? ?? ?? 57 89 5D E4 E8 ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? 8B 75 08 68 ?? ?? ?? ?? 56 E8 ?? ?? ?? ?? 83 C4 08 89 85 ?? ?? ?? ?? 85 C0 75 05 8D 78 02 EB 0C C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 33 FF 8B CB E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ??";

		bool __fastcall Override(void* thisptr, void* edx, const char* filename, int saveflags);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook("hammer_dll.dll", "MapDocSave", Override, []()
		{
			if (HAP::IsCSGO())
			{
				return PatternCSGO;
			}

			return Pattern2013;
		}());

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
		/*
			0x10149C90 static 2013 Hammer IDA address May 8 2017
			0x1017DC20 static CSGO Hammer IDA address August 27 2017
		*/
		auto Pattern2013 = "55 8B EC 83 EC 08 57 8B F9 8B 4D 0C 57 89 7D FC E8 ?? ?? ?? ?? 84 C0 75 09 33 C0 5F 8B E5 5D C2 08 00 80 BF ?? ?? ?? ?? ?? 53 8B 5D 08 75 14 68 ?? ?? ?? ?? 8B CB E8 ?? ?? ?? ?? 85 C0 0F 85 ?? ?? ?? ??";
		auto PatternCSGO = "55 8B EC 83 EC 08 8B C1 8B 4D 0C 89 45 FC 80 39 00 74 11 F6 80 ?? ?? ?? ?? ?? 75 08 33 C0 8B E5 5D C2 08 00 F6 80 ?? ?? ?? ?? ?? 53 8B 5D 08 56 57 75 16 68 ?? ?? ?? ?? 8B CB E8 ?? ?? ?? ?? 8B F0 85 F6 0F 85 ?? ?? ?? ??";

		int __fastcall Override(void* thisptr, void* edx, void* file, void* saveinfo);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook("hammer_dll.dll", "MapSolidSave", Override, []()
		{
			if (HAP::IsCSGO())
			{
				return PatternCSGO;
			}

			return Pattern2013;
		}());

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
		/*
			0x101302A0 static 2013 Hammer IDA address May 9 2017
			0x10162D70 static CSGO Hammer IDA address August 27 2017
		*/
		auto Pattern2013 = "55 8B EC 51 53 56 57 8B F1 6A 00 89 75 FC E8 ?? ?? ?? ?? 8B 7D 08 83 C4 04 8B CE FF 37 E8 ?? ?? ?? ?? 33 DB 39 9E ?? ?? ?? ?? 7E 41 33 D2 8B FF 8B 47 04 43 8B 8E ?? ?? ?? ?? D9 04 02 D9 1C 0A 8B 47 04 8B 8E ?? ?? ?? ?? D9 44 10 04 D9 5C 11 04 8B 47 04 8B 8E ?? ?? ?? ?? D9 44 10 08 D9 5C 11 08 83 C2 0C 3B 9E ?? ?? ?? ?? 7C C3";
		auto PatternCSGO = "55 8B EC 51 53 56 57 8B F9 89 7D FC FF 15 ?? ?? ?? ?? 8B 75 08 8B CF FF 05 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? FF 36 E8 ?? ?? ?? ?? 33 DB 39 9F ?? ?? ?? ?? 7E 4B 33 D2 66 66 0F 1F 84 00 ?? ?? ?? ?? 8B 46 04 8D 52 0C 8B 8F ?? ?? ?? ?? 43 8B 44 02 F4 89 44 0A F4 8B 46 04 8B 8F ?? ?? ?? ?? 8B 44 02 F8 89 44 0A F8 8B 46 04 8B 8F ?? ?? ?? ?? 8B 44 02 FC 89 44 0A FC 3B 9F ?? ?? ?? ?? 7C C1";

		void __fastcall Override(void* thisptr, void* edx, PlaneWinding* winding, int flags);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook("hammer_dll.dll", "MapFaceCreateFaceFromWinding", Override, []()
		{
			if (HAP::IsCSGO())
			{
				return PatternCSGO;
			}

			return Pattern2013;
		}());

		void __fastcall Override(void* thisptr, void* edx, PlaneWinding* winding, int flags)
		{
			if (SharedData.IsLoading)
			{
				auto id = MapFace::GetFaceID(thisptr);

				auto sourceface = LoadData.FindFaceByID(id);

				if (sourceface)
				{
					auto size = sourceface->Points.size() * sizeof(Vector3);
					std::memcpy(winding->Points, sourceface->Points.data(), size);
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
		/*
			0x10135C30 static 2013 Hammer IDA address May 7 2017
			0x10166350 static CSGO Hammer IDA address August 27 2017
		*/
		auto Pattern2013 = "55 8B EC 81 EC ?? ?? ?? ?? 56 57 8B F1 E8 ?? ?? ?? ?? 6A 00 8B CE E8 ?? ?? ?? ?? 85 C0 75 07 8B CE E8 ?? ?? ?? ?? 8B 7D 08 8B CF 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 0F 85 ?? ?? ?? ?? FF B6 ?? ?? ?? ?? 8B CF 68 ?? ?? ?? ??";
		auto PatternCSGO = "55 8B EC 83 E4 C0 81 EC ?? ?? ?? ?? 56 57 8B F1 E8 ?? ?? ?? ?? 6A 00 8B CE E8 ?? ?? ?? ?? 85 C0 75 07 8B CE E8 ?? ?? ?? ?? 8B 7D 08 8B CF 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 0F 85 ?? ?? ?? ?? FF B6 ?? ?? ?? ?? 8D 44 24 3C 68 ?? ?? ?? ??";

		int __fastcall Override(void* thisptr, void* edx, void* file, void* saveinfo);

		using ThisFunction = decltype(Override)*;

		HAP::HookModuleMask<ThisFunction> ThisHook("hammer_dll.dll", "MapFaceSave", Override, []()
		{
			if (HAP::IsCSGO())
			{
				return PatternCSGO;
			}

			return Pattern2013;
		}());

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
