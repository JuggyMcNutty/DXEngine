#pragma once

#include <dxwidgets/core/image.h>
#include <memory>
#include <string>
#include <vector>

// An icon group out of a Windows executable's resources, for the engine's
// windows: the original's show DeusEx.exe's group 128 (dx-reverse-info
// wizard.md, the frame). Of the group's entries, the best 32x32 and the best
// 16x16 -- each the most colours of its size, else the largest -- in that
// order, each RT_ICON's DIB decoded: a BITMAPINFOHEADER, the colour table,
// the colours bottom row first, then the AND mask, which leaves a pixel out
// where its bit is set. PNG icons, which Windows reads from Vista on, are not
// read. The file is the game's, never shipped: it is read where the game is.
class PEIcon
{
public:
	// The group's images, or none if the file cannot be read, has no such
	// group, or holds an icon of a kind not read.
	static std::vector<std::shared_ptr<Image>> Load(const std::string& path, int group);

	// The game's own icon: <game root>/System/<executable>.exe's group 128.
	static std::vector<std::shared_ptr<Image>> OfGame(const std::string& gameRootFolder, const std::string& gameExecutableName);
};
