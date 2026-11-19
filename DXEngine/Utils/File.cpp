
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
#include <pwd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <unistd.h>
#endif
#include "Utils/Exception.h"
#include <cstring>
#include <sstream>
#include <cstdio>
#include <chrono>

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

	// WriteFile hands each write to the system: nothing is held in the process
	void flush() override
	{
	}

	void sync() override
	{
		if (handle != INVALID_HANDLE_VALUE && !FlushFileBuffers(handle))
			Exception::Throw("FlushFileBuffers failed");
	}

	// In chunks a DWORD holds, each of what is left (bugs.md ENG.34), the
	// pointer moved by what was written
	void write(const void *data, size_t size) override
	{
		size_t pos = 0;
		while (pos < size)
		{
			size_t writesize = std::min(size - pos, (size_t)0xffffffff);
			DWORD bytesWritten = 0;
			BOOL result = WriteFile(handle, (const uint8_t*)data + pos, (DWORD)writesize, &bytesWritten, nullptr);
			if (result == FALSE || bytesWritten == 0)
				Exception::Throw("WriteFile failed");
			pos += bytesWritten;
		}
	}

	void read(void *data, size_t size) override
	{
		size_t pos = 0;
		while (pos < size)
		{
			size_t readsize = std::min(size - pos, (size_t)0xffffffff);
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

	// The buffer flushed, then the file: an fsync's failure is the write's
	// (EIO, ENOSPC on a file system that allocates late), but for EINVAL, a
	// file that has nothing to sync
	void sync() override
	{
		if (!handle)
			return;
		if (fflush(handle) != 0)
			Exception::Throw("fflush failed");
		if (fsync(fileno(handle)) != 0 && errno != EINVAL)
			Exception::Throw("fsync failed");
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

	void flush() override
	{
		if (handle && fflush(handle) != 0)
			Exception::Throw("fflush failed");
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

void File::move_into_place(const std::string& tempName, const std::string& filename)
{
	rename(tempName, filename);
	Directory::sync(fs::path(filename).parent_path());
}

// Where a link leads, link after link, a relative one from its own folder:
// the path itself when it is none. One that does not end, a loop, throws, as
// a write through it fails.
static fs::path LinkEnd(const fs::path& path)
{
	fs::path at = path;
	for (int i = 0; i < 40; i++)
	{
		std::error_code ec;
		if (!fs::is_symlink(fs::symlink_status(at, ec)))
			return at;
		fs::path to = fs::read_symlink(at, ec);
		if (ec)
			break;
		at = to.is_absolute() ? to : at.parent_path() / to;
	}
	Exception::Throw("Could not follow the link " + path.string());
}

static std::string ProcessNumber()
{
#ifdef WIN32
	return std::to_string(GetCurrentProcessId());
#else
	return std::to_string(getpid());
#endif
}

// The temporaries of a file's that a write killed before its move left in
// its folder, .<name>.dxe-<process number>, a name of the engine's own that
// no other program's temporary bears, removed once a minute old: a live
// write's lasts a moment
static void RemoveStaleTemporaries(const fs::path& folder, const std::string& name)
{
	const std::string prefix = "." + name + ".dxe-";
	const auto now = fs::file_time_type::clock::now();
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(folder, ec))
	{
		const std::string entryName = entry.path().filename().string();
		if (entryName.size() <= prefix.size() || entryName.compare(0, prefix.size(), prefix) != 0)
			continue;
		if (entryName.find_first_not_of("0123456789", prefix.size()) != std::string::npos)
			continue;
		std::error_code fileError;
		if (!fs::is_regular_file(fs::symlink_status(entry.path(), fileError)))
			continue;
		const auto written = fs::last_write_time(entry.path(), fileError);
		if (!fileError && now - written > std::chrono::minutes(1))
			fs::remove(entry.path(), fileError);
	}
}

void File::replace_all_bytes(const std::string& filename, const void* data, size_t size)
{
	const fs::path target = LinkEnd(fs::path(filename));
	const fs::path folder = target.parent_path();
	const std::string name = target.filename().string();
	const std::string tempName = (folder / ("." + name + ".dxe-" + ProcessNumber())).string();

	std::shared_ptr<File> file;
	try
	{
		file = create_always(tempName);
	}
	catch (const std::exception&)
	{
#ifndef WIN32
		// A folder that takes new files (a full card, a quota) leaves the
		// file as it is: written in place there it would be emptied
		if (access(folder.empty() ? "." : folder.c_str(), W_OK) == 0)
			throw;
#endif
		// A folder that takes no new file, the file itself writable: written
		// in place, as before the temporary
		write_all_bytes(target.string(), data, size);
		return;
	}
	try
	{
		file->write(data, size);
		file->sync();
		file->close();
		file.reset();
		std::error_code ec;
		const fs::file_status old = fs::status(target, ec);
		if (!ec && fs::exists(old))
			fs::permissions(tempName, old.permissions(), ec);
		move_into_place(tempName, target.string());
	}
	catch (...)
	{
		file.reset();
		try_delete(tempName);
		throw;
	}
	RemoveStaleTemporaries(folder, name);
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
		if (error == ERROR_PATH_NOT_FOUND)
		{
			std::string parent = fs::path(path).parent_path().string();
			if (!parent.empty())
			{
				Directory::create(parent);
				if (CreateDirectory(to_utf16(path).c_str(), nullptr) == TRUE)
					return;
				error = GetLastError();
			}
		}
		// Made already; a file in its place is no folder (bugs.md ENG.55)
		if (error == ERROR_ALREADY_EXISTS)
		{
			DWORD attributes = GetFileAttributes(to_utf16(path).c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
				return;
			throw std::runtime_error("Could not create directory " + path + ": a file is there");
		}
		throw std::runtime_error("Could not create directory " + path + ": " + std::system_category().message((int)error));
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

void Directory::sync(const fs::path&)
{
}

void Directory::syncFiles(const fs::path& folder)
{
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(folder, ec))
	{
		if (!entry.is_regular_file(ec))
			continue;
		HANDLE handle = CreateFileW(entry.path().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
			continue;
		FlushFileBuffers(handle);
		CloseHandle(handle);
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
	int error = errno;
	if (error == ENOENT)
	{
		fs::path parent = fs::path(dirname).parent_path();
		if (!parent.empty())
		{
			Directory::create(parent.string());
			if (mkdir(dirname.c_str(), 0777) == 0)
				return;
			error = errno;
		}
	}
	// Made already; a file in its place is no folder (bugs.md ENG.55)
	if (error == EEXIST)
	{
		struct stat info = {};
		if (stat(dirname.c_str(), &info) == 0 && S_ISDIR(info.st_mode))
			return;
		Exception::Throw("Could not create directory " + dirname + ": a file is there");
	}
	Exception::Throw("Could not create directory " + dirname + ": " + strerror(error));
}

fs::path Directory::localAppData()
{
	// The configuration's home as freedesktop's base directories name it:
	// XDG_CONFIG_HOME where it is an absolute path, else HOME's .config, else
	// that of the home the user database gives, a service run with no HOME
	// (bugs.md ENG.48)
	const char* configHome = getenv("XDG_CONFIG_HOME");
	if (configHome && configHome[0] == '/')
		return fs::path(configHome);

	const char* homeDir = getenv("HOME");
	if (!homeDir || !homeDir[0])
	{
		const passwd* user = getpwuid(getuid());
		homeDir = user ? user->pw_dir : nullptr;
	}
	if (!homeDir || !homeDir[0])
		throw std::runtime_error("No folder for the engine's own: no HOME, no XDG_CONFIG_HOME and no home for the user; --userdir names one");

	return fs::path(homeDir) / ".config";
}

void Directory::sync(const fs::path& folder)
{
	int fd = open(folder.c_str(), O_RDONLY | O_DIRECTORY);
	if (fd < 0)
		return;
	(void)fsync(fd);
	close(fd);
}

void Directory::syncFiles(const fs::path& folder)
{
	// Each file by itself, then the folder: a syncfs would wait for whatever
	// else waits on the file system, another program's gigabytes among it
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(folder, ec))
	{
		if (!entry.is_regular_file(ec))
			continue;
		int fd = open(entry.path().c_str(), O_RDONLY);
		if (fd < 0)
			continue;
		(void)fsync(fd);
		close(fd);
	}
	sync(folder);
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
