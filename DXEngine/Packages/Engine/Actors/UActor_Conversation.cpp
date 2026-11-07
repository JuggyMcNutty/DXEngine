
#include "Precomp.h"
#include "UActor.h"
#include "Package/PackageManager.h"
#include "Packages/ConSys/UConversation.h"
#include "Packages/ConSys/UConversationList.h"
#include "Packages/ConSys/UConListItem.h"
#include "Packages/ConSys/UConItem.h"
#include "Packages/DeusEx/UDeusExLevelInfo.h"
#include "Engine.h"

void UActor::DeusExConBindEvents()
{
	// The original's ConBindEvents (DeusEx.dll) and ConSys's
	// DConversationList::BindConversations: the list is the one the level's
	// ConversationPackage names, a bark binds by the actor's bark name, any
	// other conversation by its name only, and each item counts into its
	// conversation's ownerRefCount. Every actor binds, whatever its names:
	// the three ConBindEvents test neither (DeusEx.dll 0x1000ea50,
	// 0x10014320, 0x10019140).
	UDeusExLevelInfo* info = engine->DeusExLevelInfo;
	if (!info)
	{
		LogMessage("Conversations cannot be bound: no DeusExLevelInfo");
		return;
	}
	if (info->MissionNumber() < 0)
		return;

	// <package>Text.ConList_MissionNN, loaded without a warning (LOAD_NoWarn,
	// ADeusExPlayer::ConBindEvents DeusEx.dll 0x10014320): a package or a
	// list that is not there binds nothing and keeps the old list, an empty
	// ConversationPackage too, as Text.ConList_MissionNN is none.
	std::string pkgPrefix = ScriptToUTF8(info->ConversationPackage());
	if (NameString(pkgPrefix) == "DeusExConversations")
		pkgPrefix = "DeusExCon";
	NameString pkgName(pkgPrefix + "Text");
	if (!engine->packages->HasPackage(pkgName))
		return;

	char listName[32];
	snprintf(listName, sizeof(listName), "ConList_Mission%02d", info->MissionNumber());
	auto mission = UObject::Cast<UConversationList>(engine->packages->GetPackage(pkgName)->GetUObject("ConversationList", listName));
	if (!mission)
		return;

	// PurgeBoundConversations (ConSys.dll 0x10010cf0): the old list's
	// conversations lose this owner, and its items are deleted at once,
	// gone to the script as CriticalDelete's are.
	for (UConListItem* item = UObject::Cast<UConListItem>(ConListItems()); item; item = item->Next())
	{
		if (item->con())
			item->con()->ownerRefCount()--;
		item->Flags |= ObjectFlags::EliminateObject;
	}
	ConListItems() = nullptr;

	UClass* clsConListItem = engine->packages->FindClass("ConSys.ConListItem");
	Package* listPackage = mission->package;
	ScriptString bindName = BindName();
	ScriptString barkBindName = BarkBindName();

	for (UConItem* item = mission->conversations(); item; item = item->Next())
	{
		auto conversation = UObject::Cast<UConversation>(item->ConObject());
		if (!conversation)
			continue;

		// A bark ("_Bark" in its name, case sensitive) is owned by the
		// actor's bark name, or its name when it has none; any other
		// conversation by its name, never its bark name. Each name is
		// compared as a string, without regard to case (appStricmp, ConSys.dll
		// 0x10010be0 to 0x10010c77), an empty one as any other: a
		// conversation with no owner is every actor's with no BindName, a
		// bark with none every actor's with no bark name (no stock
		// conversation has none).
		bool isBark = conversation->conName().ToString().find("_Bark") != std::string::npos;
		const ScriptString& owner = conversation->conOwnerName();
		bool owned = (isBark && ScriptEqualsIgnoreCase(barkBindName, owner))
			|| (ScriptEqualsIgnoreCase(bindName, owner) && (!isBark || barkBindName.empty()));
		if (!owned)
			continue;

		// Each goes to the front, so the list runs from the mission list's
		// last conversation to its first. Made in the list's package,
		// ConListItemN and public, as the original's (0x10010b70).
		UConListItem* newItem = UObject::Cast<UConListItem>(listPackage->NewObject(listPackage->MakeUniqueObjectName(clsConListItem), clsConListItem, ObjectFlags::Public, true));
		newItem->con() = conversation;
		newItem->Next() = UObject::Cast<UConListItem>(ConListItems());
		ConListItems() = newItem;
		conversation->ownerRefCount()++;
	}
}
