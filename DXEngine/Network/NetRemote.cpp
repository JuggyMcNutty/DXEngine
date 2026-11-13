
#include "Precomp.h"
#include "NetRemote.h"
#include "NetChannel.h"
#include "NetDemo.h"
#include "NetDriver.h"
#include "NetSerialize.h"
#include "NetServer.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/UViewport.h"
#include "Utils/Logger.h"
#include "VM/Bytecode.h"
#include "VM/ExpressionEvaluator.h"
#include "VM/Frame.h"
#include "Engine.h"

bool NetReplicationCondition(UClass* cls, uint16_t replicationOffset, UActor* actor)
{
	if (!cls || !cls->Code)
		return false;
	int index = cls->Code->FindStatementIndex(replicationOffset);
	if (index < 0)
		return false;
	ExpressionEvalResult result = ExpressionEvaluator::Eval(cls->Code->Statements[index], actor, actor, nullptr);
	return result.Value.GetType() != ExpressionValueType::Nothing && result.Value.ToBool();
}

bool NetReplicationCondition(UFunction* rootFunction, UActor* actor)
{
	return NetReplicationCondition(UObject::TryCast<UClass>(rootFunction->Outer()), rootFunction->ReplicationOffset, actor);
}

bool NetOwnedHere(UActor* actor, NetConnection* connection)
{
	UActor* top = actor;
	while (top->Owner())
		top = top->Owner();
	UPlayerPawn* pawn = UObject::TryCast<UPlayerPawn>(top);

	// A demo's recording owns every actor in single player and on a server,
	// a client's the actors of its own player (UActorChannel::ReplicateActor,
	// Engine.dll 0x103fecf1-0x103fed74): so each goes to it as to its owner.
	if (connection->Driver->Demo && connection->Driver->ServerConnection.get() != connection)
	{
		if (actor->Level()->NetMode() != NM_Client)
			return true;
		return pawn && UObject::TryCast<UViewport>(pawn->Player()) != nullptr;
	}

	if (!pawn || !pawn->Player())
		return false;
	if (connection->Driver->ServerConnection.get() == connection)
		return UObject::TryCast<UViewport>(pawn->Player()) != nullptr;
	return connection->PlayerObject && static_cast<UObject*>(pawn->Player()) == connection->PlayerObject;
}

namespace
{
	// A call's bunch on the actor's channel: its field's number, then the
	// parameters in a frame of the called function's, each as the
	// declaration's parameter -- a bool its bit, any other a bit for whether
	// it is not zero, then its value --, reliable as the function is.
	void SendCall(NetActorChannel* channel, NetPackageMap::ClassNetCache* cache, NetPackageMap::FieldNetCache* field, UFunction* function, UFunction* root, CallArguments& args, bool reliable)
	{
		NetPackageMap& map = channel->Connection->PackageMap;
		NetOutBunch bunch(channel, false);
		bunch.Data.WriteInt(field->FieldNetIndex, cache->GetMaxIndex());

		Array<UProperty*> callParms;
		for (UProperty* prop : Frame::CallParms(function))
		{
			if (prop != function->ReturnParm)
				callParms.push_back(prop);
		}
		std::unique_ptr<uint64_t[]> frame(new uint64_t[((size_t)function->StructSize + 7) / 8 + 1]());
		uint8_t* data = reinterpret_cast<uint8_t*>(frame.get());
		for (UProperty* prop : callParms)
			prop->ConstructArray(data + prop->DataOffset.DataOffset);
		// Each as the original's frame holds it, an out one's variable as it
		// was reached (CallArguments::SetStart)
		for (size_t i = 0; i < callParms.size() && i < args.size(); i++)
		{
			ExpressionValue& arg = args.Start(i);
			if (arg.GetType() != ExpressionValueType::Nothing)
				ExpressionValue::Variable(data, callParms[i]).Store(arg);
		}

		size_t index = 0;
		for (UField* f = root->Children; f; f = f->Next)
		{
			UProperty* prop = UObject::TryCast<UProperty>(f);
			if (!prop)
				continue;
			if ((prop->PropFlags & (PropertyFlags::Parm | PropertyFlags::ReturnParm)) != PropertyFlags::Parm)
				break;
			size_t parmIndex = index++;
			if (parmIndex >= callParms.size() || map.ObjectToIndex(prop) == -1)
				continue;

			const void* value = data + callParms[parmIndex]->DataOffset.DataOffset;
			if (UObject::TryCast<UBoolProperty>(prop))
			{
				NetWriteItem(prop, bunch.Data, map, value);
			}
			else
			{
				bool send = !NetIsZero(prop, value);
				bunch.Data.WriteBit(send);
				if (send)
					NetWriteItem(prop, bunch.Data, map, value);
			}
		}

		for (UProperty* prop : callParms)
			prop->DestructArray(data + prop->DataOffset.DataOffset);

		bunch.bReliable = reliable;
		if (!bunch.Data.IsError())
			channel->SendBunch(bunch, true);
		else
			LogMessage("Net: remote function bunch overflowed");
	}

	// A demo's recording takes a call for the net as the original's
	// ProcessDemoRecFunction (Engine.dll 0x103e6400): one not static, of an
	// actor not sent once to keep -- on a client, one of the server's --,
	// whose first declaration's statement holds with bDemoRecording set, and
	// on a client bClientDemoRecording, the roles exchanged, as a server
	// would send it. The actor goes first where the demo has not had it. The
	// original evaluates the parameters for the demo and again for the call
	// itself; here they are the call's own, evaluated once.
	void RecordDemoCall(NetDemoDriver* demo, UFunction* function, UActor* actor, CallArguments& args)
	{
		if (!AnyFlags(function->FuncFlags, FunctionFlags::Net) || actor->bNetTemporary())
			return;
		bool client = actor->Level()->NetMode() == NM_Client;
		if (client && actor->Role() == ROLE_Authority)
			return;

		UFunction* root = function;
		while (UFunction* super = UObject::TryCast<UFunction>(root->BaseField))
			root = super;

		if (client)
			std::swap(actor->Role(), actor->RemoteRole());
		actor->bDemoRecording() = true;
		actor->bClientDemoRecording() = client;
		bool wanted = NetReplicationCondition(root, actor);
		actor->bDemoRecording() = false;
		actor->bClientDemoRecording() = false;
		if (client)
			std::swap(actor->Role(), actor->RemoteRole());
		actor->bClientDemoNetFunc() = false;

		NetConnection* connection = demo->RecordConnection();
		if (!wanted || !connection)
			return;
		NetPackageMap::ClassNetCache* cache = connection->PackageMap.GetClassNetCache(actor->Class);
		NetPackageMap::FieldNetCache* field = cache ? cache->GetFromField(root) : nullptr;
		if (!field)
			return;
		NetActorChannel* channel = connection->FindActorChannel(actor);
		if (!channel)
		{
			channel = static_cast<NetActorChannel*>(connection->CreateChannel(ChannelType::Actor, true));
			if (!channel)
				return;
			channel->SetChannelActor(actor);
		}
		if (channel->OpenPacketId == -1)
			channel->ReplicateActor();
		if (channel->Closing)
			return;
		SendCall(channel, cache, field, function, root, args, AnyFlags(function->FuncFlags, FunctionFlags::NetReliable));
	}
}

bool NetProcessRemoteFunction(UFunction* function, UObject* instance, CallArguments& args)
{
	UActor* actor = UObject::TryCast<UActor>(instance);
	if (!actor || AnyFlags(function->FuncFlags, FunctionFlags::Static) || actor->bDeleteMe())
		return false;

	// A simulated proxy runs only the functions marked simulated.
	bool absorb = actor->Role() <= ROLE_SimulatedProxy && !AnyFlags(function->FuncFlags, FunctionFlags::Simulated);

	// The actor's level's demo: played back, nothing goes anywhere; recorded,
	// the call goes into it, then on as ever (AActor::ProcessRemoteFunction,
	// Engine.dll 0x103e5d90).
	if (NetDemoDriver* demo = engine->LevelDemoDriver.get(); demo && actor->XLevel() == engine->Level)
	{
		if (demo->ServerConnection)
			return absorb;
		RecordDemoCall(demo, function, actor, args);
	}

	if (actor->Level()->NetMode() == NM_Standalone)
		return false;
	if (!AnyFlags(function->FuncFlags, FunctionFlags::Net))
		return absorb;

	// A server's call goes to the client whose player owns the actor, if
	// any; a client's to the server.
	bool server = actor->Role() == ROLE_Authority;
	NetConnection* connection = nullptr;
	if (server)
	{
		UActor* top = actor;
		while (top->Owner())
			top = top->Owner();
		UPlayerPawn* pawn = UObject::TryCast<UPlayerPawn>(top);
		connection = pawn ? NetConnectionOfPlayer(pawn->Player()) : nullptr;
		if (!connection)
			return absorb;
	}

	// The condition is the first declaration's.
	UFunction* root = function;
	while (UFunction* super = UObject::TryCast<UFunction>(root->BaseField))
		root = super;
	if (!NetReplicationCondition(root, actor))
		return absorb;

	if (!server)
		connection = engine->LevelNetDriver ? engine->LevelNetDriver->ServerConnection.get() : nullptr;
	if (!connection)
		return absorb;

	// An unreliable call is dropped when the connection has no room.
	bool reliable = AnyFlags(function->FuncFlags, FunctionFlags::NetReliable);
	if (!reliable && !connection->IsNetReady(false))
		return true;

	NetPackageMap& map = connection->PackageMap;
	NetPackageMap::ClassNetCache* cache = map.GetClassNetCache(actor->Class);
	NetPackageMap::FieldNetCache* field = cache ? cache->GetFromField(root) : nullptr;
	if (!field)
		return true;

	// A client calls only on an actor the server gave it; a server opens a
	// channel for the actor, and sends the actor first, if the client has
	// none.
	NetActorChannel* channel = connection->FindActorChannel(actor);
	if (!channel)
	{
		if (!server)
			return true;
		channel = static_cast<NetActorChannel*>(connection->CreateChannel(ChannelType::Actor, true));
		if (!channel)
			return true;
		channel->SetChannelActor(actor);
	}
	if (channel->OpenPacketId == -1)
	{
		if (!server)
			return true;
		channel->ReplicateActor();
	}
	if (channel->Closing)
		return true;

	SendCall(channel, cache, field, function, root, args, reliable);
	return true;
}
