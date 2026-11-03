
#include "Precomp.h"
#include "NUdpLink.h"
#include "VM/NativeFunc.h"
#include "Packages/IpDrv/UUdpLink.h"

void NUdpLink::RegisterFunctions()
{
	RegisterVMNativeFunc_3("UdpLink", "BindPort", &NUdpLink::BindPort, 0);
	RegisterVMNativeFunc_4("UdpLink", "ReadBinary", &NUdpLink::ReadBinary, 0);
	RegisterVMNativeFunc_3("UdpLink", "ReadText", &NUdpLink::ReadText, 0);
	RegisterVMNativeFunc_4("UdpLink", "SendBinary", &NUdpLink::SendBinary, 0);
	RegisterVMNativeFunc_3("UdpLink", "SendText", &NUdpLink::SendText, 0);
}

void NUdpLink::BindPort(UObject* Self, std::optional<int> Port, std::optional<bool> bUseNextAvailable, int& ReturnValue)
{
	ReturnValue = UObject::Cast<UUdpLink>(Self)->BindPort(Port ? *Port : 0, bUseNextAvailable ? *bUseNextAvailable : false);
}

void NUdpLink::ReadBinary(UObject* Self, IpAddr& Addr, int Count, uint8_t& B, int& ReturnValue)
{
	ReturnValue = UObject::Cast<UUdpLink>(Self)->ReadBinary(Addr, Count, &B);
}

// As TcpLink's: 8-bit text both ways, a character past 0xFF sent as 0x7F
void NUdpLink::ReadText(UObject* Self, IpAddr& Addr, ScriptString& Str, int& ReturnValue)
{
	std::string text;
	ReturnValue = UObject::Cast<UUdpLink>(Self)->ReadText(Addr, text);
	Str = Latin1ToScript(text);
}

void NUdpLink::SendBinary(UObject* Self, const IpAddr& Addr, int Count, uint8_t& B, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UUdpLink>(Self)->SendBinary(Addr, Count, &B);
}

void NUdpLink::SendText(UObject* Self, const IpAddr& Addr, const ScriptString& Str, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UUdpLink>(Self)->SendText(Addr, ScriptToLatin1(Str, 0x7F));
}
