
#include "Precomp.h"
#include "Utils/File.h"
#include "Utils/UTF16.h"
#include "Utils/StrTools.h"
#ifdef WIN32
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#else
#include <libgen.h>
#include <fnmatch.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>
#include <unistd.h>
#endif
#include "Utils/Exception.h"
#include <cstring>
#include <sstream>
#include <cstdio>

#ifdef WIN32

class FileImpl : public File
{
public:
	FileImpl(HANDLE handle) : handle(handle)
	{
	}

	~FileImpl()
	{
		CloseHandle(handle);
	}

	void close() override
	{
		if (handle == INVALID_HANDLE_VALUE)
			return;
		BOOL ok = CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
		if (!ok)
			Exception::Throw("CloseHandle failed");
	}

	void write(const void *data, size_t size) override
	{
		size_t pos = 0;
		while (pos < size)
		{
			size_t writesize = std::min(size, (size_t)0xffffffff);
			BOOL result = WriteFile(handle, (const uint8_t*)data + pos, (DWORD)writesize, nullptr, nullptr);
			if (result == FALSE)
				Exception::Throw("WriteFile failed");
			pos += writesize;
		}
	}

	void read(void *data, size_t size) override
	{
		size_t pos = 0;
		while (pos < size)
		{
			size_t readsize = std::min(size, (size_t)0xffffffff);
			DWORD bytesRead = 0;
			BOOL result = ReadFile(handle, (uint8_t*)data + pos, (DWORD)readsize, &bytesRead, nullptr);
			if (result == FALSE || bytesRead != readsize)
				Exception::Throw("ReadFile failed");
			pos += readsize;
		}
	}

	int64_t size() override
	{
		LARGE_INTEGER fileSize;
		BOOL result = GetFileSizeEx(handle, &fileSize);
		if (result == FALSE)
			Exception::Throw("GetFileSizeEx failed");
		return fileSize.QuadPart;
	}

	void seek(int64_t offset, SeekPoint origin) override
	{
		LARGE_INTEGER off, newoff;
		off.QuadPart = offset;
		DWORD moveMethod = FILE_BEGIN;
		if (origin == SeekPoint::current) moveMethod = FILE_CURRENT;
		else if (origin == SeekPoint::end) moveMethod = FILE_END;
		BOOL result = SetFilePointerEx(handle, off, &newoff, moveMethod);
		if (result == FALSE)
			Exception::Throw("SetFilePointerEx failed");
	}

	uint64_t tell() override
	{
		LARGE_INTEGER offset, delta;
		delta.QuadPart = 0;
		BOOL result = SetFilePointerEx(handle, delta, &offset, FILE_CURRENT);
		if (result == FALSE)
			Exception::Throw("SetFilePointerEx failed");
		return offset.QuadPart;
	}

	HANDLE handle = INVALID_HANDLE_VALUE;
};

std::shared_ptr<File> File::create_always(const std::string &filename)
{
	HANDLE handle = CreateFile(to_utf16(filename).c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle == INVALID_HANDLE_VALUE)
		Exception::Throw("Could not create " + filename);

	return std::make_shared<FileImpl>(handle);
}

std::shared_ptr<File> File::open_existing(const std::string &filename)
{
	HANDLE handle = CreateFile(to_utf16(filename).c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle == INVALID_HANDLE_VALUE)
		Exception::Throw("Could not open " + filename);

	return std::make_shared<FileImpl>(handle);
}

bool File::try_delete(const std::string& filename)
{
	return DeleteFile(to_utf16(filename).c_str()) == TRUE;
}

void File::delete_always(const std::string& filename)
{
	if (!try_delete(filename))
		throw std::runtime_error("Could not delete " + filename);
}

void File::rename(const std::string& sourceName, const std::string& destinationName)
{
	std::filesystem::rename(sourceName, destinationName);
}

#else

class FileImpl : public File
{
public:
	FileImpl(FILE* handle) : handle(handle)
	{
	}

	~FileImpl()
	{
		if (handle)
			fclose(handle);
	}

	// The last buffered flush, checked: fclose's failure (ENOSPC, EIO) was
	// passed over in the destructor, an ini or a save left short.
	void close() override
	{
		if (!handle)
			return;
		int result = fclose(handle);
		handle = nullptr;
		if (result != 0)
			Exception::Throw("fclose failed");
	}

	int64_t size() override
	{
		auto pos = ftell(handle);
		fseek(handle, 0, SEEK_END);
		auto s = ftell(handle);
		fseek(handle, pos, SEEK_SET);
		return s;
	}

	void read(void *data, size_t size) override
	{
		if (size == 0)
			return;

		size_t result = fread(data, size, 1, handle);
		if (result != 1)
			Exception::Throw("fread failed");
	}

	void write(const void *data, size_t size) override
	{
		if (size == 0)
			return;

		size_t result = fwrite(data, size, 1, handle);
		if (result != 1)
			Exception::Throw("fwrite failed");
	}

	void seek(int64_t offset, SeekPoint origin = SeekPoint::begin) override
	{
		int moveMethod = SEEK_SET;
		if (origin == SeekPoint::current) moveMethod = SEEK_CUR;
		else if (origin == SeekPoint::end) moveMethod = SEEK_END;
		int result = fseek(handle, offset, moveMethod);
		if (result != 0)
			Exception::Throw("fseek failed");
	}

	uint64_t tell() override
	{
		auto result = ftell(handle);
		if (result == -1)
			Exception::Throw("ftell failed");
		return result;
	}

	FILE* handle = nullptr;
};

std::shared_ptr<File> File::create_always(const std::string &filename)
{
	FILE* handle = fopen(filename.c_str(), "wb");
	if (handle == nullptr)
		Exception::Throw("Could not create " + filename);

	return std::make_shared<FileImpl>(handle);
}

std::shared_ptr<File> File::open_existing(const std::string &filename)
{
	FILE* handle = fopen(filename.c_str(), "rb");
	if (handle == nullptr)
		Exception::Throw("Could not open " + filename);

	return std::make_shared<FileImpl>(handle);
}

bool File::try_delete(const std::string& filename)
{
	return std::remove(filename.c_str()) == 0;
}

void File::delete_always(const std::string& filename)
{
	if (!try_delete(filename))
		throw std::runtime_error("Could not delete " + filename);
}

void File::rename(const std::string& sourceName, const std::string& destinationName)
{
	std::filesystem::rename(sourceName, destinationName);
}

#endif

void File::write_all_bytes(const std::string& filename, const void* data, size_t size)
{
	auto file = create_always(filename);
	file->write(data, size);
	file->close();
}

void File::write_all_text(const std::string& filename, const std::string& text)
{
	auto file = create_always(filename);
	file->write(text.data(), text.size());
	file->close();
}

Array<uint8_t> File::read_all_bytes(const std::string& filename)
{
	auto file = open_existing(filename);
	Array<uint8_t> buffer(file->size());
	file->read(buffer.data(), buffer.size());
	return buffer;
}

std::string File::read_all_text(const std::string& filename)
{
	auto file = open_existing(filename);
	auto size = file->size();
	if (size == 0) return {};
	std::string buffer;
	buffer.resize(size);
	file->read(&buffer[0], buffer.size());
	return buffer;
}

Array<std::string> File::read_all_lines(const std::string& filename)
{
	std::string text = read_all_text(filename);

	if (text.empty())
		return {};

	Array<std::string> result;

	std::string buffer;

	std::stringstream ss(text);

	while (std::getline(ss, buffer)) // Default delimiter is '\n'
		result.push_back(buffer);

	return result;
}

/////////////////////////////////////////////////////////////////////////////

#ifdef WIN32

void Directory::create(const std::string& name)
{
	// A trailing separator dropped, as the POSIX one's
	std::string path = name;
	while (path.size() > 1 && (path.back() == '\\' || path.back() == '/'))
		path.pop_back();
	BOOL result = CreateDirectory(to_utf16(path).c_str(), nullptr);
	if (result == FALSE)
	{
		DWORD error = GetLastError();
		if (error == ERROR_ALREADY_EXISTS)
		{
			return;
		}
		else if (error == ERROR_PATH_NOT_FOUND)
		{
			try
			{
				std::string parent = fs::path(path).parent_path().string();
				if (!parent.empty())
				{
					Directory::create(parent);
					if (CreateDirectory(to_utf16(path).c_str(), nullptr) == TRUE)
						return;
				}
			}
			catch (...)
			{
			}
		}
		throw std::runtime_error("Could not create directory for path " + path);
	}
}

fs::path Directory::localAppData()
{
	PWSTR path = nullptr;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &path)))
		throw std::runtime_error("SHGetKnownFolderPath(FOLDERID_LocalAppData) failed");
	try
	{
		std::string folder = from_utf16(path);
		CoTaskMemFree(path);
		return folder;
	}
	catch (...)
	{
		CoTaskMemFree(path);
		throw;
	}
}

#else

void Directory::create(const std::string& name)
{
	// A trailing separator dropped: with it the parent of "a/b/" is "a/b",
	// which the recursion made, and the folder's own mkdir then failed as
	// made already
	std::string dirname = name;
	while (dirname.size() > 1 && dirname.back() == '/')
		dirname.pop_back();
	if (mkdir(dirname.c_str(), 0777) == 0)
		return;
	if (errno == EEXIST)
		return;
	if (errno == ENOENT)
	{
		try
		{
			fs::path parent = fs::path(dirname).parent_path();
			if (!parent.empty())
			{
				Directory::create(parent.string());
				if (mkdir(dirname.c_str(), 0777) == 0)
					return;
			}
		}
		catch (...)
		{
		}
	}
	Exception::Throw("Could not create directory " + dirname);
}

fs::path Directory::localAppData()
{
	// We're trying to find "~/.config" here
	const char* homeDir = getenv("HOME");

	if (!homeDir)
		throw std::runtime_error("HOME environment variable is not set... somehow.");

	return fs::path(homeDir) / ".config";
}

#endif

static std::string& EngineUserDirOverride()
{
	static std::string path;
	return path;
}

fs::path Directory::engineUserDir()
{
	if (!EngineUserDirOverride().empty())
		return fs::path(EngineUserDirOverride());
	return localAppData() / "DXEngine";
}

void Directory::setEngineUserDir(const std::string& path)
{
	EngineUserDirOverride() = path;
}

bool Directory::hasEngineUserDir()
{
	return !EngineUserDirOverride().empty();
}

fs::path Directory::resolveCase(const fs::path& folder)
{
	fs::path resolved;
	for (const auto& part : folder)
	{
		fs::path next = resolved / part;
		std::error_code ec;
		if (!resolved.empty() && !fs::exists(next, ec) && fs::is_directory(resolved, ec))
		{
			for (const auto& entry : fs::directory_iterator(resolved, ec))
			{
				if (StrTools::equals_ignore_case(entry.path().filename().string(), part.string()))
				{
					next = entry.path();
					break;
				}
			}
		}
		resolved = next;
	}
	return resolved;
}

std::string OS::executable_path()
{
#if defined(WIN32)
	WCHAR exe_filename[_MAX_PATH];
	DWORD len = GetModuleFileName(nullptr, exe_filename, _MAX_PATH);
	if (len == 0 || len == _MAX_PATH)
		Exception::Throw("GetModuleFileName failed!");

	WCHAR drive[_MAX_DRIVE], dir[_MAX_DIR];
	_wsplitpath_s(exe_filename, drive, _MAX_DRIVE, dir, _MAX_DIR, NULL, 0, NULL, 0);

	return from_utf16(std::wstring(drive) + dir);
#else
	#ifndef PROC_EXE_PATH
	#define PROC_EXE_PATH "/proc/self/exe"
	#endif

	char exe_file[PATH_MAX];
	int size;
	struct stat sb;
	if (lstat(PROC_EXE_PATH, &sb) < 0)
	{
		Exception::Throw("get_exe_path: proc file system not accesible");
	}
	else
	{
		size = readlink(PROC_EXE_PATH, exe_file, PATH_MAX - 1); // a byte kept for the terminator written below
		if (size < 0)
		{
			Exception::Throw(strerror(errno));
		}
		else
		{
			exe_file[size] = '\0';
			return std::string(dirname(exe_file)) + "/";
		}
	}

	#endif
}
