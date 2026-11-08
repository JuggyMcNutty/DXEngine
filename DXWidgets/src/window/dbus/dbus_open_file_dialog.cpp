
#include "dbus_open_file_dialog.h"
#include "window/window.h"
#include <dbus/dbus.h>
#include <unistd.h>
#include <algorithm>

DBusOpenFileDialog::DBusOpenFileDialog(std::string ownerHandle, std::string method) : ownerHandle(ownerHandle), method(method)
{
}

std::string DBusOpenFileDialog::Filename() const
{
	return outputFilenames.empty() ? std::string() : outputFilenames.front();
}

std::vector<std::string> DBusOpenFileDialog::Filenames() const
{
	return outputFilenames;
}

void DBusOpenFileDialog::SetMultiSelect(bool multiselect)
{
	this->multiSelect = multiselect;
}

void DBusOpenFileDialog::SetFilename(const std::string &filename)
{
	inputFilename = filename;
}

void DBusOpenFileDialog::SetDefaultExtension(const std::string& extension)
{
	defaultExt = extension;
}

void DBusOpenFileDialog::AddFilter(const std::string &filter_description, const std::string &filter_extension)
{
	filters.push_back({ filter_description, filter_extension });
}

void DBusOpenFileDialog::ClearFilters()
{
	filters.clear();
}

void DBusOpenFileDialog::SetFilterIndex(int filter_index)
{
	this->filter_index = filter_index;
}

void DBusOpenFileDialog::SetInitialDirectory(const std::string &path)
{
	initialDirectory = path;
}

void DBusOpenFileDialog::SetTitle(const std::string &title)
{
	this->title = title;
}

// A filter as the portal takes it, (sa(us)): its name, then its patterns,
// each a glob (0), from the ;-list AddFilter takes, Windows' *.* any file
static void AppendFilter(DBusMessageIter* container, const std::string& description, const std::string& extension)
{
	DBusMessageIter filter = {};
	dbus_message_iter_open_container(container, DBUS_TYPE_STRUCT, nullptr, &filter);
	const char* name = description.c_str();
	dbus_message_iter_append_basic(&filter, DBUS_TYPE_STRING, &name);

	DBusMessageIter patterns = {};
	dbus_message_iter_open_container(&filter, DBUS_TYPE_ARRAY, "(us)", &patterns);
	size_t start = 0;
	while (start <= extension.size())
	{
		size_t end = extension.find(';', start);
		if (end == std::string::npos)
			end = extension.size();
		std::string glob = extension.substr(start, end - start);
		if (glob == "*.*")
			glob = "*";
		if (!glob.empty())
		{
			dbus_uint32_t type = 0;
			const char* value = glob.c_str();
			DBusMessageIter pattern = {};
			dbus_message_iter_open_container(&patterns, DBUS_TYPE_STRUCT, nullptr, &pattern);
			dbus_message_iter_append_basic(&pattern, DBUS_TYPE_UINT32, &type);
			dbus_message_iter_append_basic(&pattern, DBUS_TYPE_STRING, &value);
			dbus_message_iter_close_container(&patterns, &pattern);
		}
		start = end + 1;
	}
	dbus_message_iter_close_container(&filter, &patterns);
	dbus_message_iter_close_container(container, &filter);
}

// An option of the portal's call, {sv}, a string's or a bool's
static void AppendOption(DBusMessageIter* options, const char* key, const char* value)
{
	DBusMessageIter entry = {};
	dbus_message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	DBusMessageIter variant = {};
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(options, &entry);
}

static void AppendOption(DBusMessageIter* options, const char* key, dbus_bool_t value)
{
	DBusMessageIter entry = {};
	dbus_message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	DBusMessageIter variant = {};
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, DBUS_TYPE_BOOLEAN_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(options, &entry);
}

// One dialog at a time: a Show from a handler the wait runs (it processes
// the windows' events) answers nothing chosen
bool DBusOpenFileDialog::Show()
{
	static bool showing = false;
	if (showing)
		return false;
	showing = true;
	outputFilenames.clear();
	bool chosen = false;
	try
	{
		chosen = ShowPortal();
	}
	catch (...)
	{
		showing = false;
		throw;
	}
	showing = false;
	return chosen;
}

bool DBusOpenFileDialog::ShowPortal()
{
	// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileChooser.html
		
	dbus_bool_t bresult = {};
	DBusError error = {};
	dbus_error_init(&error);
		
	// A connection of the dialog's own: on the shared one the wait read and
	// dropped every other message its users had; one the bus's loss ends
	// the wait on, not the program (libdbus' exit on disconnect)
	DBusConnection* connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
	if (!connection)
	{
		dbus_error_free(&error);
		return false;
	}
	dbus_connection_set_exit_on_disconnect(connection, FALSE);
	// Closed as the call ends, before its last reference goes, as a private
	// connection must be: a reference of its own for that
	struct ConnectionClose
	{
		DBusConnection* connection;
		~ConnectionClose()
		{
			dbus_connection_close(connection);
			dbus_connection_unref(connection);
		}
	} connectionClose = { dbus_connection_ref(connection) };

	// The request's handle_token, its Response's object path known before
	// the call (the request path, by the bus name with its dots as
	// underscores), its match added before the call: a Response sent before
	// a match added after it was never seen, the wait never ending
	static unsigned int requests = 0;
	std::string token = "dxwidgets" + std::to_string((unsigned int)getpid()) + "_" + std::to_string(++requests);
	std::string sender = dbus_bus_get_unique_name(connection);
	if (!sender.empty() && sender.front() == ':')
		sender.erase(0, 1);
	std::replace(sender.begin(), sender.end(), '.', '_');
	std::string requestPath = "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
	std::string rule = "type='signal',interface='org.freedesktop.portal.Request',member='Response',path='" + requestPath + "'";
	dbus_bus_add_match(connection, rule.c_str(), nullptr);

	DBusMessage* request = dbus_message_new_method_call("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.FileChooser", method.c_str());
	if (!request)
	{
		dbus_connection_unref(connection);
		dbus_error_free(&error);
		return false;
	}

	const char* parentWindow = ownerHandle.c_str();
	const char* title = this->title.c_str();

	DBusMessageIter requestArgs = {};
	dbus_message_iter_init_append(request, &requestArgs);

	bresult = dbus_message_iter_append_basic(&requestArgs, DBUS_TYPE_STRING, &parentWindow);
	bresult = dbus_message_iter_append_basic(&requestArgs, DBUS_TYPE_STRING, &title);

	DBusMessageIter requestOptions = {};
	bresult = dbus_message_iter_open_container(&requestArgs, DBUS_TYPE_ARRAY, "{sv}", &requestOptions);

	// handle_token - s         race condition prevention for signal (/org/freedesktop/portal/desktop/request/SENDER/TOKEN)
	// accept_label - s         text label for the OK button
	// modal - b                makes dialog modal. Defaults to true. Not really sure what it means since nothing stayed modal on my computer
	// directory - b            open folder mode, added in version 3
	// filters - a(sa(us))      [('Images', [(0, '*.ico'), (1, 'image/png')]), ('Text', [(0, '*.txt')])]
	// current_filter - sa(us)  filter from filters that should be current filter
	// choices - a(ssa(ss)s)    list of serialized combo boxes to add to the file chooser
	// current_name - s         suggested name

	AppendOption(&requestOptions, "handle_token", token.c_str());

	// directory - b (OpenFile's, the folder dialog's)
	if (method == "OpenFile" && directoryMode)
		AppendOption(&requestOptions, "directory", (dbus_bool_t)TRUE);

	// current_folder - ay
	if (!initialDirectory.empty())
	{
		// Note: the docs unfortunately says "The portal implementation is free to ignore this option"
			
		const char* key = "current_folder";
		const char* value = initialDirectory.c_str();
		int valueCount = (int)initialDirectory.size() + 1;

		DBusMessageIter entry = {};
		bresult = dbus_message_iter_open_container(&requestOptions, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
		bresult = dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

		DBusMessageIter variant = {};
		DBusMessageIter array = {};
		bresult = dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "ay", &variant);
		bresult = dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &array);
		bresult = dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE, &value, valueCount);
		bresult = dbus_message_iter_close_container(&variant, &array);
		bresult = dbus_message_iter_close_container(&entry, &variant);

		bresult = dbus_message_iter_close_container(&requestOptions, &entry);
	}

	// current_name - s (SaveFile's suggested name)
	if (method == "SaveFile" && !inputFilename.empty())
	{
		const char* key = "current_name";
		const char* value = inputFilename.c_str();

		DBusMessageIter entry = {};
		bresult = dbus_message_iter_open_container(&requestOptions, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
		bresult = dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

		DBusMessageIter variant = {};
		bresult = dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant);
		bresult = dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value);
		bresult = dbus_message_iter_close_container(&entry, &variant);

		bresult = dbus_message_iter_close_container(&requestOptions, &entry);
	}

	// multiple - b (OpenFile's)
	if (method == "OpenFile")
	{
		const char* key = "multiple";
		dbus_bool_t value = multiSelect;

		DBusMessageIter entry = {};
		bresult = dbus_message_iter_open_container(&requestOptions, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
		bresult = dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

		DBusMessageIter variant = {};
		bresult = dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, DBUS_TYPE_BOOLEAN_AS_STRING, &variant);
		bresult = dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
		bresult = dbus_message_iter_close_container(&entry, &variant);

		bresult = dbus_message_iter_close_container(&requestOptions, &entry);
	}

	// filters - a(sa(us)), each AddFilter's
	if (!filters.empty())
	{
		const char* key = "filters";

		DBusMessageIter entry = {};
		bresult = dbus_message_iter_open_container(&requestOptions, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
		bresult = dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

		DBusMessageIter variant = {};
		DBusMessageIter array = {};
		bresult = dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant);
		bresult = dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(sa(us))", &array);
		for (const Filter& filter : filters)
			AppendFilter(&array, filter.description, filter.extension);
		bresult = dbus_message_iter_close_container(&variant, &array);
		bresult = dbus_message_iter_close_container(&entry, &variant);

		bresult = dbus_message_iter_close_container(&requestOptions, &entry);
	}

	// current_filter - sa(us), SetFilterIndex's
	if (filter_index >= 0 && (size_t)filter_index < filters.size())
	{
		const char* key = "current_filter";

		DBusMessageIter entry = {};
		bresult = dbus_message_iter_open_container(&requestOptions, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
		bresult = dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

		DBusMessageIter variant = {};
		bresult = dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "(sa(us))", &variant);
		AppendFilter(&variant, filters[filter_index].description, filters[filter_index].extension);
		bresult = dbus_message_iter_close_container(&entry, &variant);

		bresult = dbus_message_iter_close_container(&requestOptions, &entry);
	}

	bresult = dbus_message_iter_close_container(&requestArgs, &requestOptions);

	// No portal (none on the bus, nor one it starts): nothing chosen
	DBusMessage* response = dbus_connection_send_with_reply_and_block(connection, request, DBUS_TIMEOUT_USE_DEFAULT, &error);
	if (!response)
	{
		dbus_message_unref(request);
		dbus_connection_unref(connection);
		dbus_error_free(&error);
		return false;
	}

	const char* handle = nullptr;
	bresult = dbus_message_get_args(response, &error, DBUS_TYPE_OBJECT_PATH, &handle, DBUS_TYPE_INVALID);
	if (!bresult)
	{
		dbus_message_unref(response);
		dbus_message_unref(request);
		dbus_connection_unref(connection);
		dbus_error_free(&error);
		return false;
	}

	// A portal older than handle_token answers a path of its own, matched too
	std::string signalObjectPath = handle;
	std::string handleRule = "type='signal',interface='org.freedesktop.portal.Request',member='Response',path='" + signalObjectPath + "'";
	if (signalObjectPath != requestPath)
		dbus_bus_add_match(connection, handleRule.c_str(), nullptr);

	dbus_message_unref(response);
	dbus_message_unref(request);

	// The Response waited for with the windows' events processed meanwhile,
	// so they draw and their timers run, as Windows' dialogs run them, where
	// the wait blocked them all; every message on the connection is ours
	DBusMessage* signalmsg = nullptr;
	while (!signalmsg && dbus_connection_read_write(connection, 50))
	{
		while (DBusMessage* message = dbus_connection_pop_message(connection))
		{
			const char* path = dbus_message_get_path(message);
			if (!signalmsg && path && dbus_message_is_signal(message, "org.freedesktop.portal.Request", "Response") && (requestPath == path || signalObjectPath == path))
				signalmsg = message;
			else
				dbus_message_unref(message);
		}
		if (!signalmsg && DisplayBackend::Get())
			DisplayBackend::Get()->ProcessEvents();
	}

	// No response: the bus lost, which ends the wait, nothing chosen
	if (!signalmsg)
	{
		dbus_connection_unref(connection);
		dbus_error_free(&error);
		return false;
	}

	// Read the response

	dbus_uint32_t responseCode = 0;
	std::vector<std::string> uris;

	DBusMessageIter signalArgs;
	bresult = dbus_message_iter_init(signalmsg, &signalArgs);

	// response code - u
	if (dbus_message_iter_get_arg_type(&signalArgs) == DBUS_TYPE_UINT32)
	{
		dbus_message_iter_get_basic(&signalArgs, &responseCode);
	}
	dbus_message_iter_next(&signalArgs);

	// results - a{sv}
	if (dbus_message_iter_get_arg_type(&signalArgs) == DBUS_TYPE_ARRAY)
	{
		DBusMessageIter resultsArray = {};
		dbus_message_iter_recurse(&signalArgs, &resultsArray);
		while (true)
		{
			int type = dbus_message_iter_get_arg_type(&resultsArray);
			if (type != DBUS_TYPE_DICT_ENTRY)
				break;

			DBusMessageIter entry = {};
			dbus_message_iter_recurse(&resultsArray, &entry);

			const char* key = nullptr;
			dbus_message_iter_get_basic(&entry, &key);
			dbus_message_iter_next(&entry);

			DBusMessageIter value = {};
			dbus_message_iter_recurse(&entry, &value);

			std::string k = key;
			if (k == "uris" && dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_ARRAY) // as
			{
				DBusMessageIter uriArray = {};
				dbus_message_iter_recurse(&value, &uriArray);
				while (true)
				{
					int type = dbus_message_iter_get_arg_type(&uriArray);
					if (type != DBUS_TYPE_STRING)
						break;

					const char* uri = nullptr;
					dbus_message_iter_get_basic(&uriArray, &uri);
					uris.push_back(uri);

					dbus_message_iter_next(&uriArray);
				}
			}
			else if (k == "choices" && dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_ARRAY) // a(ss)
			{
				// An array of pairs of strings,
				// the first string being the ID of a combobox that was passed into this call,
				// the second string being the selected option.
			}
			else if (k == "current_filter" && dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRUCT) // sa(us)
			{
				// The filter that was selected.
				// This may match a filter in the filter list or another filter that was applied unconditionally.
			}
			
			dbus_message_iter_next(&entry);
			dbus_message_iter_next(&resultsArray);
		}
	}
	dbus_message_iter_next(&signalArgs);

	dbus_message_unref(signalmsg);
	dbus_connection_unref(connection);
	dbus_error_free(&error);

	if (responseCode != 0) // User cancelled
		return false;

	// A file URI's path each; another URI names no file here, and a choice
	// of none of them is none, not an empty name
	for (const std::string& uri : uris)
	{
		if (uri.size() > 7 && uri.substr(0, 7) == "file://")
			outputFilenames.push_back(unescapeUri(uri.substr(7)));
	}
		
	return !outputFilenames.empty();
}

// A file URI's path: each %XX the byte it stands for (RFC 3986), a space's
// %20 and a UTF-8 name's bytes alike
std::string DBusOpenFileDialog::unescapeUri(const std::string& s)
{
	auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
	std::string result;
	result.reserve(s.size());
	size_t i = 0;
	while (i < s.length())
	{
		if (s[i] == '%' && i + 2 < s.length() && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0)
		{
			result += (char)(hex(s[i + 1]) * 16 + hex(s[i + 2]));
			i += 3;
		}
		else
		{
			result += s[i];
			i++;
		}
	}
	return result;
}
