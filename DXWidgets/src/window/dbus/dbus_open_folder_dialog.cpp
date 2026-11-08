
#include "dbus_open_folder_dialog.h"
#include "dbus_open_file_dialog.h"

DBusOpenFolderDialog::DBusOpenFolderDialog(std::string ownerHandle) : ownerHandle(ownerHandle)
{
}

// The portal's OpenFile in its folder mode (directory, FileChooser 3),
// through the open dialog's call: its own copy of the call blocked the
// windows' events, and lost a Response sent before its match was added
bool DBusOpenFolderDialog::Show()
{
	DBusOpenFileDialog dialog(ownerHandle, "OpenFile");
	dialog.SetDirectoryMode(true);
	dialog.SetTitle(title);
	dialog.SetInitialDirectory(initialDirectory);
	selectedPath.clear();
	if (!dialog.Show())
		return false;
	selectedPath = dialog.Filename();
	return !selectedPath.empty();
}

std::string DBusOpenFolderDialog::SelectedPath() const
{
	return selectedPath;
}

void DBusOpenFolderDialog::SetInitialDirectory(const std::string& path)
{
	initialDirectory = path;
}

void DBusOpenFolderDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}
