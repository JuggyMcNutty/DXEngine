#pragma once

#include "systemdialogs/open_file_dialog.h"

class DBusOpenFileDialog : public OpenFileDialog
{
public:
	// The portal's method: OpenFile, or SaveFile for the save dialog
	DBusOpenFileDialog(std::string ownerHandle, std::string method = "OpenFile");

	std::string Filename() const override;
	std::vector<std::string> Filenames() const override;
	void SetMultiSelect(bool multiselect) override;
	void SetFilename(const std::string &filename) override;
	void SetDefaultExtension(const std::string& extension) override;
	void AddFilter(const std::string &filter_description, const std::string &filter_extension) override;
	void ClearFilters() override;
	void SetFilterIndex(int filter_index) override;
	void SetInitialDirectory(const std::string &path) override;
	void SetTitle(const std::string& newtitle) override;
	bool Show() override;

	// The portal's folder mode (directory, FileChooser 3), for the folder
	// dialog
	void SetDirectoryMode(bool enable) { directoryMode = enable; }

private:
	bool ShowPortal();
	static std::string unescapeUri(const std::string& s);
	
	std::string ownerHandle;
	std::string method;
	std::string title;
	std::string initialDirectory;
	std::string inputFilename;
	std::string defaultExt;
	std::vector<std::string> outputFilenames;
	bool multiSelect = false;
	bool directoryMode = false;

	struct Filter
	{
		std::string description;
		std::string extension;
	};
	std::vector<Filter> filters;
	int filter_index = 0;
};
