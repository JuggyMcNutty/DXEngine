
#include "Precomp.h"
#include "UBorderWindow.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Extension/Windows/UGC.h"

// The original's border window (Extension.dll XBorderWindow; dx-reverse-info/
// extension-dll.md, Border windows) frames its top shown child with nine
// textures and gives it the inside: within the child margins, and within the
// border too while its margins are from the border.

// The original's XBorderWindow::Init (0x100010c0): a normal style, the
// margins from the border; no textures, cursors or margins, not smoothed,
// tiled and not resizeable, as the class's defaults have them.
void UBorderWindow::InitDefaults()
{
	UWindow::InitDefaults();
	borderStyle() = (uint8_t)EDrawStyle::Normal;
	bMarginsFromBorder() = true;
	ComputeWindowBorders();
}

// The original's (XBorderWindow::ComputeWindowBorders 0x10001500): each
// side as thick as the largest of its textures -- the left from the two left
// corners and the left edge, the top from the two top corners and the top
// edge, and so on -- and none without a style.
void UBorderWindow::ComputeWindowBorders()
{
	leftMargin() = 0.0f;
	rightMargin() = 0.0f;
	TopMargin() = 0.0f;
	BottomMargin() = 0.0f;
	if ((EDrawStyle)borderStyle() == EDrawStyle::None)
		return;

	auto U = [](UTexture* tex) { return tex ? (float)tex->USize() : 0.0f; };
	auto V = [](UTexture* tex) { return tex ? (float)tex->VSize() : 0.0f; };
	leftMargin() = std::max({ U(borderTopLeft()), U(borderBottomLeft()), U(borderLeft()) });
	rightMargin() = std::max({ U(borderTopRight()), U(borderBottomRight()), U(borderRight()) });
	TopMargin() = std::max({ V(borderTopLeft()), V(borderTopRight()), V(borderTop()) });
	BottomMargin() = std::max({ V(borderBottomLeft()), V(borderBottomRight()), V(borderBottom()) });
}

// The setters lay the window out again, but EnableResizing and
// SetMoveCursors (0x10001480, 0x100014a0). From a script a texture or a
// margin not given is none or 0 (execSetBorders 0x10002260,
// execSetBorderMargins 0x10002400), a flag not given true.
void UBorderWindow::BaseMarginsFromBorder(std::optional<bool> bBorder)
{
	bMarginsFromBorder() = !bBorder || *bBorder;
	AskParentForReconfigure();
}

void UBorderWindow::EnableResizing(std::optional<bool> bResize)
{
	bResizeable() = !bResize || *bResize;
}

void UBorderWindow::SetBorderMargins(std::optional<float> NewLeft, std::optional<float> NewRight, std::optional<float> newTop, std::optional<float> newBottom)
{
	childLeftMargin() = NewLeft.value_or(0.0f);
	childRightMargin() = NewRight.value_or(0.0f);
	childTopMargin() = newTop.value_or(0.0f);
	childBottomMargin() = newBottom.value_or(0.0f);
	AskParentForReconfigure();
}

static UTexture* GivenTexture(const std::optional<UObject*>& tex)
{
	return tex ? UObject::Cast<UTexture>(*tex) : nullptr;
}

// SetBorders (0x10001280) computes the border's size again; SetBorderStyle,
// the script's, does not.
void UBorderWindow::SetBorders(std::optional<UObject*> bordTL, std::optional<UObject*> bordTR, std::optional<UObject*> bordBL, std::optional<UObject*> bordBR, std::optional<UObject*> bordL, std::optional<UObject*> bordR, std::optional<UObject*> bordT, std::optional<UObject*> bordB, std::optional<UObject*> newCenter)
{
	borderTopLeft() = GivenTexture(bordTL);
	borderTopRight() = GivenTexture(bordTR);
	borderBottomLeft() = GivenTexture(bordBL);
	borderBottomRight() = GivenTexture(bordBR);
	borderLeft() = GivenTexture(bordL);
	borderRight() = GivenTexture(bordR);
	borderTop() = GivenTexture(bordT);
	borderBottom() = GivenTexture(bordB);
	center() = GivenTexture(newCenter);
	ComputeWindowBorders();
	AskParentForReconfigure();
}

void UBorderWindow::SetMoveCursors(std::optional<UObject*> Move, std::optional<UObject*> hMove, std::optional<UObject*> vMove, std::optional<UObject*> tlMove, std::optional<UObject*> trMove)
{
	MoveCursor() = GivenTexture(Move);
	hMoveCursor() = GivenTexture(hMove);
	vMoveCursor() = GivenTexture(vMove);
	tlMoveCursor() = GivenTexture(tlMove);
	trMoveCursor() = GivenTexture(trMove);
}

// The margins around the top shown child: the child margins, and the
// border's within them while the margins are from the border.
void UBorderWindow::GetInsideMargins(float& left, float& right, float& top, float& bottom)
{
	left = childLeftMargin();
	right = childRightMargin();
	top = childTopMargin();
	bottom = childBottomMargin();
	if (bMarginsFromBorder())
	{
		left += leftMargin();
		right += rightMargin();
		top += TopMargin();
		bottom += BottomMargin();
	}
}

// The original's (XBorderWindow::ParentRequestedPreferredSize 0x10001a40):
// the top shown child's size, asked for the inside, with the margins around
// it, no smaller than the border; without one the size asked for stands.
void UBorderWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	UWindow* child = UObject::Cast<UWindow>(GetTopChild(true));
	if (!child)
		return;

	float left, right, top, bottom;
	GetInsideMargins(left, right, top, bottom);
	child->QueryPreferredSize(bWidthSpecified, preferredWidth - (left + right), &preferredWidth, bHeightSpecified, preferredHeight - (top + bottom), &preferredHeight);
	preferredWidth = std::max(preferredWidth + left + right, leftMargin() + rightMargin());
	preferredHeight = std::max(preferredHeight + top + bottom, TopMargin() + BottomMargin());
}

// The original's (XBorderWindow::ConfigurationChanged 0x10001bc0): the top
// shown child given the inside, any other placing itself by its alignment;
// the script is sent no ConfigurationChanged.
void UBorderWindow::ConfigurationChanged()
{
	UWindow* child = UObject::Cast<UWindow>(GetTopChild(true));
	if (!child)
		return;

	float left, right, top, bottom;
	GetInsideMargins(left, right, top, bottom);
	child->ConfigureChild(left, top, Width() - (left + right), Height() - (top + bottom));
}

// A child's request goes on up (XBorderWindow::ChildRequestedReconfiguration
// 0x100421d0 takes none).
bool UBorderWindow::ChildRequestedReconfiguration(UWindow* childWin)
{
	return false;
}

// The original's (XBorderWindow::Draw 0x10001ce0), which sends the script no
// DrawWindow: nothing without a style, else the nine textures in it, smoothed
// and stretched as asked, each side as thick as the border's size.
void UBorderWindow::DrawWindow(UGC* gc)
{
	if ((EDrawStyle)borderStyle() == EDrawStyle::None)
		return;

	gc->SetStyle((EDrawStyle)borderStyle());
	gc->EnableSmoothing(bSmoothBorder());

	// In DrawBorders' order (GC.uc: the four corners, then the four
	// edges, then the centre): the bottom-right corner before the left edge
	UObject* borders[9] =
	{
		borderTopLeft(),
		borderTopRight(),
		borderBottomLeft(),
		borderBottomRight(),
		borderLeft(),
		borderRight(),
		borderTop(),
		borderBottom(),
		center()
	};

	gc->DrawBorders(
		0.0f, 0.0f,
		Width(), Height(),
		leftMargin(), rightMargin(),
		TopMargin(), BottomMargin(), borders, bStretchBorder(), bStretchBorder());
}
