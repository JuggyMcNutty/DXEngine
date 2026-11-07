
#include "dbus_save_file_dialog.h"
#include "dbus_open_file_dialog.h"

DBusSaveFileDialog::DBusSaveFileDialog(std::string ownerHandle) : ownerHandle(ownerHandle)
{
}

// The portal's SaveFile, through the open dialog's call with that method:
// this was a stub answering false, so the error window's Save Report saved
// nothing and said nothing on every Linux window. Its filters go with it,
// and a name chosen without an extension takes the default one, as
// Windows' dialog gives it.
bool DBusSaveFileDialog::Show()
{
	DBusOpenFileDialog dialog(ownerHandle, "SaveFile");
	dialog.SetTitle(title);
	dialog.SetInitialDirectory(initial_directory);
	dialog.SetFilename(initial_filename);
	for (const Filter& filter : filters)
		dialog.AddFilter(filter.description, filter.extension);
	dialog.SetFilterIndex(filterindex);
	if (!dialog.Show())
		return false;
	filename = dialog.Filename();
	size_t name = filename.find_last_of('/');
	if (!filename.empty() && !defaultext.empty() && filename.find('.', name == std::string::npos ? 0 : name + 1) == std::string::npos)
		filename += "." + defaultext;
	return !filename.empty();
}

std::string DBusSaveFileDialog::Filename() const
{
	return filename;
}

void DBusSaveFileDialog::SetFilename(const std::string& filename)
{
	initial_filename = filename;
}

void DBusSaveFileDialog::AddFilter(const std::string& filter_description, const std::string& filter_extension)
{
	Filter f;
	f.description = filter_description;
	f.extension = filter_extension;
	filters.push_back(std::move(f));
}

void DBusSaveFileDialog::ClearFilters()
{
	filters.clear();
}

void DBusSaveFileDialog::SetFilterIndex(int filter_index)
{
	filterindex = filter_index;
}

void DBusSaveFileDialog::SetInitialDirectory(const std::string& path)
{
	initial_directory = path;
}

void DBusSaveFileDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}

void DBusSaveFileDialog::SetDefaultExtension(const std::string& extension)
{
	defaultext = extension;
}
