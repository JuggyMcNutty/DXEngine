
#include "Precomp.h"
#include "WidgetResourceData.h"
#include "Utils/File.h"
#include <miniz.h>
#include "Utils/Exception.h"
#include <cstdlib>
#include <string>
#include <dxwidgets/core/theme.h>
#include <dxwidgets/window/window.h>

class ResourceLoaderPK3 : public ResourceLoader
{
public:
	ResourceLoaderPK3()
	{
		const auto pk3PathStr = (fs::path{ OS::executable_path() } / "DXEngine.pk3").string();
		mz_bool result = mz_zip_reader_init_file(&widgetResources, pk3PathStr.c_str(), 0);
#ifndef WIN32
		// On Linux, DXEngine.pk3 can additionally be put in some other folders given below.
		if (!result)
			result = mz_zip_reader_init_file(&widgetResources, "/usr/share/dxengine/DXEngine.pk3", 0);
		// The data home (XDG_DATA_HOME, else HOME's .local/share) and HOME's
		// .dxengine, each only where its variable is set: a service with no
		// HOME built a path from a null pointer
		char* home = std::getenv("HOME");
		char* xdg_data_home = std::getenv("XDG_DATA_HOME");
		if (!result && (xdg_data_home || home))
		{
			std::string directory = xdg_data_home ? (fs::path(xdg_data_home) / "dxengine").string() : (fs::path(home) / ".local/share/dxengine").string();
			result = mz_zip_reader_init_file(&widgetResources, (fs::path(directory) / "DXEngine.pk3").c_str(), 0);
		}
		if (!result && home)
		{
			const std::string directory = fs::path(home) / ".dxengine";
			result = mz_zip_reader_init_file(&widgetResources, (fs::path(directory) / "DXEngine.pk3").c_str(), 0);
		}
#endif
		if (!result)
			Exception::Throw("Could not open DXEngine.pk3");
	}

	~ResourceLoaderPK3()
	{
		mz_zip_reader_end(&widgetResources);
	}

	std::vector<SingleFontData> LoadFont(const std::string& name) override
	{
		std::vector<SingleFontData> fonts;
		fonts.resize(3);
		TryLoadWidgetFile("noto/notosans-regular.ttf", fonts[0].fontdata);
		TryLoadWidgetFile("noto/notosansarmenian-regular.ttf", fonts[1].fontdata);
		TryLoadWidgetFile("noto/notosansgeorgian-regular.ttf", fonts[2].fontdata);
		return fonts;
	}

	std::vector<uint8_t> ReadAllBytes(const std::string& filename) override
	{
		std::vector<uint8_t> buffer;
		if (!TryLoadWidgetFile(filename, buffer))
			Exception::Throw("Could not load " + filename);
		return buffer;
	}

private:
	bool TryLoadWidgetFile(const std::string& name, std::vector<uint8_t>& buffer)
	{
		mz_bool result;
		mz_zip_archive_file_stat stat;
		mz_uint32 fileIndex;

		buffer.clear();

		result = mz_zip_reader_locate_file_v2(&widgetResources, name.c_str(), nullptr, 0, &fileIndex);
		if (!result)
			return false;

		result = mz_zip_reader_file_stat(&widgetResources, fileIndex, &stat);
		if (!result)
			return false;

		buffer.resize(stat.m_uncomp_size);
		result = mz_zip_reader_extract_file_to_mem(&widgetResources, name.c_str(), buffer.data(), buffer.size(), 0);
		if (!result)
			return false;

		return true;
	}

	mz_zip_archive widgetResources = {};
};

static std::unique_ptr<DisplayBackend> backend;

void InitWidgetResources(const std::string& theme)
{
	backend = DisplayBackend::TryCreateBackend();
	DisplayBackend::Set(std::move(backend));

	ResourceLoader::Set(std::make_unique<ResourceLoaderPK3>());

	auto bytes = ResourceData::ReadAllBytes("theme/Theme.css");
	std::string stylesheet((const char*)bytes.data(), bytes.size());
	WidgetTheme::SetTheme(std::make_unique<StylesheetTheme>(stylesheet, theme));
}

void DeinitWidgetResources()
{
	ResourceLoader::Set(nullptr);
	backend.reset();
}
