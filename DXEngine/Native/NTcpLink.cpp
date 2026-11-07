
#include "Precomp.h"
#include "NTcpLink.h"
#include "VM/NativeFunc.h"
#include "Packages/IpDrv/UTcpLink.h"
#include <algorithm>

void NTcpLink::RegisterFunctions()
{
	RegisterVMNativeFunc_3("TcpLink", "BindPort", &NTcpLink::BindPort, 0);
	RegisterVMNativeFunc_1("TcpLink", "Close", &NTcpLink::Close, 0);
	RegisterVMNativeFunc_1("TcpLink", "IsConnected", &NTcpLink::IsConnected, 0);
	RegisterVMNativeFunc_1("TcpLink", "Listen", &NTcpLink::Listen, 0);
	RegisterVMNativeFunc_2("TcpLink", "Open", &NTcpLink::Open, 0);
	RegisterVMNativeFunc_3("TcpLink", "ReadBinary", &NTcpLink::ReadBinary, 0);
	RegisterVMNativeFunc_2("TcpLink", "ReadText", &NTcpLink::ReadText, 0);
	RegisterVMNativeFunc_3("TcpLink", "SendBinary", &NTcpLink::SendBinary, 0);
	RegisterVMNativeFunc_2("TcpLink", "SendText", &NTcpLink::SendText, 0);
}

void NTcpLink::BindPort(UObject* Self, std::optional<int> Port, std::optional<bool> bUseNextAvailable, int& ReturnValue)
{
	ReturnValue = UObject::Cast<UTcpLink>(Self)->BindPort(Port ? *Port : 0, bUseNextAvailable ? *bUseNextAvailable : false);
}

void NTcpLink::Close(UObject* Self, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UTcpLink>(Self)->Close();
}

void NTcpLink::IsConnected(UObject* Self, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UTcpLink>(Self)->IsConnected();
}

void NTcpLink::Listen(UObject* Self, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UTcpLink>(Self)->Listen();
}

void NTcpLink::Open(UObject* Self, const IpAddr& Addr, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UTcpLink>(Self)->Open(Addr);
}

void NTcpLink::ReadBinary(UObject* Self, int Count, uint8_t& B, int& ReturnValue)
{
	ReturnValue = UObject::Cast<UTcpLink>(Self)->ReadBinary(Count, &B);
}

// The bytes come and go as 8-bit text: each byte a character, and a
// character past 0xFF sent as 0x7F, as the original's ToAnsi narrows it.
// Where nothing was read the string stays as it was, as the original's
// leaves it (IpDrv.dll 0x107081b0).
void NTcpLink::ReadText(UObject* Self, ScriptString& Str, int& ReturnValue)
{
	std::string text;
	int count = UObject::Cast<UTcpLink>(Self)->ReadText(text);
	if (count >= 0)
		Str = Latin1ToScript(text);
	ReturnValue = std::max(count, 0);
}

void NTcpLink::SendBinary(UObject* Self, int Count, uint8_t& B, int& ReturnValue)
{
	ReturnValue = UObject::Cast<UTcpLink>(Self)->SendBinary(Count, &B);
}

void NTcpLink::SendText(UObject* Self, const ScriptString& Str, int& ReturnValue)
{
	ReturnValue = UObject::Cast<UTcpLink>(Self)->SendText(ScriptToLatin1(Str, 0x7F));
}
