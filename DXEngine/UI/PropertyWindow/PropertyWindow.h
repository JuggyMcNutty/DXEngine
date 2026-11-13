#pragma once

#include <dxwidgets/core/widget.h>
#include "PropertyRows.h"
#include <functional>

class PropertyList;

// A property window as the original's (bugs.md D.81, WIN.38): the Advanced
// Options of PREFERENCES and EDITACTOR's inspector, a tree of rows -- a
// name, a value, the selected row's editor and buttons -- over the game's
// window. Its place is the config's [WindowPositions] line of its name,
// kept as it goes and written with the config.
class PropertyWindow : public Widget
{
public:
	PropertyWindow(Widget* owner, const std::string& title, const std::string& positionName, std::vector<std::unique_ptr<PropertyRow>> rows);
	~PropertyWindow();

	// The original's Show(1) and SetFocus: shown, given the focus
	void Present();

	// Told as the window goes, by its close box or its owner's going
	std::function<void(PropertyWindow*)> OnDestroyed;

protected:
	void OnClose() override;
	void OnGeometryChanged() override;

private:
	void KeepPosition();

	PropertyList* List = nullptr;
	std::string PositionName;
};
