cmake_minimum_required(VERSION 3.15)

project(DXEngine)

add_subdirectory(Thirdparty/openmpt)
add_subdirectory(DXGPU)
add_subdirectory(DXWidgets)

if (WIN32)
	# OpenAL Soft as OpenAL32.dll beside the engine (LGPL, so a library of its
	# own): its library alone -- none of its programs or install rules, no
	# PipeWire, nor the steps that run sed, dlltool or git while building.
	set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
	set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
	set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "" FORCE)
	set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
	set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
	set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "" FORCE)
	set(ALSOFT_UPDATE_BUILD_VERSION OFF CACHE BOOL "" FORCE)
	set(ALSOFT_BUILD_IMPORT_LIB OFF CACHE BOOL "" FORCE)
	set(ALSOFT_BACKEND_PIPEWIRE OFF CACHE BOOL "" FORCE)
	add_subdirectory(Thirdparty/openal-soft)
endif()

if (MINGW)
	# The third-party libraries' own warnings under MinGW -- OpenMPT's check
	# for MSVC's multi-threaded C library among them -- ignored, as the MSVC
	# block below ignores theirs: not ours to fix.
	target_compile_options(openmpt PRIVATE -w)
	target_compile_options(OpenAL PRIVATE -w)
	target_compile_options(common PRIVATE -w)
endif()

project(DXEngine)

set(DXCOMMON_SOURCES
	DXEngine/Precomp.cpp
	DXEngine/Precomp.h
	DXEngine/GameApp.cpp
	DXEngine/GameApp.h
	DXEngine/Engine.cpp
	DXEngine/EngineGC.cpp
	DXEngine/Engine.h
	DXEngine/GameFolder.cpp
	DXEngine/GameFolder.h
	DXEngine/LauncherSettings.cpp
	DXEngine/LauncherSettings.h
	DXEngine/GamepadInput.cpp
	DXEngine/GamepadInput.h
	DXEngine/LauncherLine.cpp
	DXEngine/LauncherLine.h
	DXEngine/OriginalCommandLine.cpp
	DXEngine/OriginalCommandLine.h
	DXEngine/GameWindow.cpp
	DXEngine/GameWindow.h
	DXEngine/Timeline.cpp
	DXEngine/Timeline.h
	DXEngine/UE1GameDatabase.h
	DXEngine/UE1GameDatabase.cpp
	DXEngine/Utils/CommandLine.cpp
	DXEngine/Utils/CommandLine.h
	DXEngine/Utils/Convert.cpp
	DXEngine/Utils/Convert.h
	DXEngine/Utils/Exception.cpp
	DXEngine/Utils/Exception.h
	DXEngine/Utils/File.cpp
	DXEngine/Utils/File.h
	DXEngine/Utils/Logger.cpp
	DXEngine/Utils/Logger.h
	DXEngine/Utils/JsonValue.cpp
	DXEngine/Utils/JsonValue.h
	DXEngine/Utils/SHA1Sum.cpp
	DXEngine/Utils/SHA1Sum.h
	DXEngine/Utils/PEIcon.cpp
	DXEngine/Utils/PEIcon.h
	DXEngine/Utils/MD5.cpp
	DXEngine/Utils/MD5.h
	DXEngine/Utils/StrTools.cpp
	DXEngine/Utils/StrTools.h
	DXEngine/Utils/Random.cpp
	DXEngine/Utils/Random.h
	DXEngine/Utils/UTF16.cpp
	DXEngine/Utils/UTF16.h
	DXEngine/Utils/UTF8Reader.cpp
	DXEngine/Utils/UTF8Reader.h
	DXEngine/Utils/Array.h
	DXEngine/Utils/CrashReporter.cpp
	DXEngine/Utils/CrashReporter.h
	DXEngine/Utils/AlignedAlloc.h
	DXEngine/Render/VisibleActor.cpp
	DXEngine/Render/VisibleActor.h
	DXEngine/Render/VisibleBrush.cpp
	DXEngine/Render/VisibleBrush.h
	DXEngine/Render/VisibleCorona.cpp
	DXEngine/Render/VisibleCorona.h
	DXEngine/Render/VisibleDecal.cpp
	DXEngine/Render/VisibleDecal.h
	DXEngine/Render/VisibleFrame.cpp
	DXEngine/Render/VisibleFrame.h
	DXEngine/Render/VisibleIteratorItem.cpp
	DXEngine/Render/VisibleIteratorItem.h
	DXEngine/Render/VisibleMesh.cpp
	DXEngine/Render/VisibleMesh.h
	DXEngine/Render/VisibleNode.cpp
	DXEngine/Render/VisibleNode.h
	DXEngine/Render/VisiblePortal.cpp
	DXEngine/Render/VisiblePortal.h
	DXEngine/Render/VisibleSprite.cpp
	DXEngine/Render/VisibleSprite.h
	DXEngine/Render/VisibleTranslucent.h
	DXEngine/Render/RenderSubsystem.cpp
	DXEngine/Render/RenderSubsystem.h
	DXEngine/Render/RenderCanvas.cpp
	DXEngine/Render/RenderScene.cpp
	DXEngine/Render/RenderWindow.cpp
	DXEngine/Render/BspClipper.cpp
	DXEngine/Render/BspClipper.h
	DXEngine/VM/NativeFunc.cpp
	DXEngine/VM/Frame.cpp
	DXEngine/VM/ExpressionValue.h
	DXEngine/VM/StructValue.h
	DXEngine/VM/ArrayValue.h
	DXEngine/VM/ScriptCall.cpp
	DXEngine/VM/Bytecode.cpp
	DXEngine/VM/Bytecode.h
	DXEngine/VM/ScriptCall.h
	DXEngine/VM/ExpressionEvaluator.h
	DXEngine/VM/Frame.h
	DXEngine/VM/NativeFunc.h
	DXEngine/VM/Expression.h
	DXEngine/VM/ExpressionEvaluator.cpp
	DXEngine/VM/ExpressionVisitor.h
	DXEngine/VM/Iterator.cpp
	DXEngine/VM/Iterator.h
	DXEngine/VM/PrintPrettyExpression.cpp
	DXEngine/VM/PrintPrettyExpression.h
	DXEngine/Audio/AudioSource.h
	DXEngine/Audio/AudioSource.cpp
	DXEngine/Audio/AudioDevice.cpp
	DXEngine/Audio/AudioDevice.h
	DXEngine/Audio/GalaxyMixer.cpp
	DXEngine/Audio/GalaxyMixer.h
	DXEngine/Native/NStatLog.h
	DXEngine/Native/NZoneInfo.cpp
	DXEngine/Native/NNavigationPoint.cpp
	DXEngine/Native/NLevelInfo.h
	DXEngine/Native/NScriptedTexture.cpp
	DXEngine/Native/NPawn.cpp
	DXEngine/Native/NGameInfo.h
	DXEngine/Native/NGameInfo.cpp
	DXEngine/Native/NStatLogFile.h
	DXEngine/Native/NWarpZoneInfo.cpp
	DXEngine/Native/NScriptedTexture.h
	DXEngine/Native/NObject.h
	DXEngine/Native/NActor.cpp
	DXEngine/Native/NPlayerPawn.h
	DXEngine/Native/NStatLog.cpp
	DXEngine/Native/NStatLogFile.cpp
	DXEngine/Native/NPawn.h
	DXEngine/Native/NConsole.h
	DXEngine/Native/NWarpZoneInfo.h
	DXEngine/Native/NLevelInfo.cpp
	DXEngine/Native/NCanvas.h
	DXEngine/Native/NCommandlet.h
	DXEngine/Native/NConsole.cpp
	DXEngine/Native/NNavigationPoint.h
	DXEngine/Native/NCanvas.cpp
	DXEngine/Native/NPlayerPawn.cpp
	DXEngine/Native/NCommandlet.cpp
	DXEngine/Native/NDecal.cpp
	DXEngine/Native/NActor.h
	DXEngine/Native/NObject.cpp
	DXEngine/Native/NDecal.h
	DXEngine/Native/NZoneInfo.h
	DXEngine/Native/NInternetLink.cpp
	DXEngine/Native/NInternetLink.h
	DXEngine/Native/NTcpLink.cpp
	DXEngine/Native/NTcpLink.h
	DXEngine/Native/NUdpLink.cpp
	DXEngine/Native/NUdpLink.h
	DXEngine/Native/NDebugInfo.cpp
	DXEngine/Native/NDebugInfo.h
	DXEngine/Native/NDeusExPlayer.cpp
	DXEngine/Native/NDeusExPlayer.h
	DXEngine/Native/NScriptedPawn.cpp
	DXEngine/Native/NScriptedPawn.h
	DXEngine/Native/NDeusExDecoration.cpp
	DXEngine/Native/NDeusExDecoration.h
	DXEngine/Native/NDumpLocation.cpp
	DXEngine/Native/NDumpLocation.h
	DXEngine/Native/NDeusExSaveInfo.cpp
	DXEngine/Native/NDeusExSaveInfo.h
	DXEngine/Native/NGameDirectory.cpp
	DXEngine/Native/NGameDirectory.h
	DXEngine/Native/NParticleIterator.cpp
	DXEngine/Native/NParticleIterator.h
	DXEngine/Native/NPlayerPawnExt.cpp
	DXEngine/Native/NPlayerPawnExt.h
	DXEngine/Native/NBorderWindow.cpp
	DXEngine/Native/NBorderWindow.h
	DXEngine/Native/NButtonWindow.cpp
	DXEngine/Native/NButtonWindow.h
	DXEngine/Native/NCheckboxWindow.cpp
	DXEngine/Native/NCheckboxWindow.h
	DXEngine/Native/NClipWindow.cpp
	DXEngine/Native/NClipWindow.h
	DXEngine/Native/NComputerWindow.cpp
	DXEngine/Native/NComputerWindow.h
	DXEngine/Native/NConEvent.cpp
	DXEngine/Native/NConEvent.h
	DXEngine/Native/NConEventRandomLabel.cpp
	DXEngine/Native/NConEventRandomLabel.h
	DXEngine/Native/NConversation.cpp
	DXEngine/Native/NConversation.h
	DXEngine/Native/NDeusExTextParser.cpp
	DXEngine/Native/NDeusExTextParser.h
	DXEngine/Native/NEditWindow.cpp
	DXEngine/Native/NEditWindow.h
	DXEngine/Native/NExtensionObject.cpp
	DXEngine/Native/NExtensionObject.h
	DXEngine/Native/NExtString.cpp
	DXEngine/Native/NExtString.h
	DXEngine/Native/NFlagBase.cpp
	DXEngine/Native/NFlagBase.h
	DXEngine/Native/NGC.cpp
	DXEngine/Native/NGC.h
	DXEngine/Native/NLargeTextWindow.cpp
	DXEngine/Native/NLargeTextWindow.h
	DXEngine/Native/NListWindow.cpp
	DXEngine/Native/NListWindow.h
	DXEngine/Native/NModalWindow.cpp
	DXEngine/Native/NModalWindow.h
	DXEngine/Native/NRadioBoxWindow.cpp
	DXEngine/Native/NRadioBoxWindow.h
	DXEngine/Native/NRootWindow.cpp
	DXEngine/Native/NRootWindow.h
	DXEngine/Native/NScaleManagerWindow.cpp
	DXEngine/Native/NScaleManagerWindow.h
	DXEngine/Native/NScaleWindow.cpp
	DXEngine/Native/NScaleWindow.h
	DXEngine/Native/NScrollAreaWindow.cpp
	DXEngine/Native/NScrollAreaWindow.h
	DXEngine/Native/NTextLogWindow.cpp
	DXEngine/Native/NTextLogWindow.h
	DXEngine/Native/NTextWindow.cpp
	DXEngine/Native/NTextWindow.h
	DXEngine/Native/NTileWindow.cpp
	DXEngine/Native/NTileWindow.h
	DXEngine/Native/NToggleWindow.cpp
	DXEngine/Native/NToggleWindow.h
	DXEngine/Native/NViewportWindow.cpp
	DXEngine/Native/NViewportWindow.h
	DXEngine/Native/NWindow.cpp
	DXEngine/Native/NWindow.h
	DXEngine/RenderDevice/RenderDevice.cpp
	DXEngine/RenderDevice/RenderDevice.h
	DXEngine/RenderDevice/Vulkan/BufferManager.cpp
	DXEngine/RenderDevice/Vulkan/BufferManager.h
	DXEngine/RenderDevice/Vulkan/CachedTexture.h
	DXEngine/RenderDevice/Vulkan/CommandBufferManager.cpp
	DXEngine/RenderDevice/Vulkan/CommandBufferManager.h
	DXEngine/RenderDevice/Vulkan/DescriptorSetManager.cpp
	DXEngine/RenderDevice/Vulkan/DescriptorSetManager.h
	DXEngine/RenderDevice/Vulkan/FileResource.cpp
	DXEngine/RenderDevice/Vulkan/FileResource.h
	DXEngine/RenderDevice/Vulkan/FramebufferManager.cpp
	DXEngine/RenderDevice/Vulkan/FramebufferManager.h
	DXEngine/RenderDevice/Vulkan/RenderPassManager.cpp
	DXEngine/RenderDevice/Vulkan/RenderPassManager.h
	DXEngine/RenderDevice/Vulkan/SamplerManager.cpp
	DXEngine/RenderDevice/Vulkan/SamplerManager.h
	DXEngine/RenderDevice/Vulkan/SceneTextures.cpp
	DXEngine/RenderDevice/Vulkan/SceneTextures.h
	DXEngine/RenderDevice/Vulkan/ShaderManager.cpp
	DXEngine/RenderDevice/Vulkan/ShaderManager.h
	DXEngine/RenderDevice/Vulkan/TextureManager.cpp
	DXEngine/RenderDevice/Vulkan/TextureManager.h
	DXEngine/RenderDevice/Vulkan/TextureUploader.cpp
	DXEngine/RenderDevice/Vulkan/TextureUploader.h
	DXEngine/RenderDevice/Vulkan/UploadManager.cpp
	DXEngine/RenderDevice/Vulkan/UploadManager.h
	DXEngine/RenderDevice/Vulkan/VulkanRenderDevice.cpp
	DXEngine/RenderDevice/Vulkan/VulkanRenderDevice.h
	DXEngine/RenderDevice/OpenGL/GLRenderDevice.cpp
	DXEngine/RenderDevice/OpenGL/GLRenderDevice.h
	DXEngine/RenderDevice/OpenGL/GLFileResource.cpp
	DXEngine/RenderDevice/OpenGL/GLFileResource.h
	DXEngine/RenderDevice/OpenGL/GLHandles.h
	DXEngine/RenderDevice/OpenGL/GLTextureManager.cpp
	DXEngine/RenderDevice/OpenGL/GLTextureManager.h
	DXEngine/RenderDevice/OpenGL/GLTextureUploader.cpp
	DXEngine/RenderDevice/OpenGL/GLTextureUploader.h
	DXEngine/RenderDevice/OpenGL/GLUploadManager.cpp
	DXEngine/RenderDevice/OpenGL/GLUploadManager.h
	DXEngine/RenderDevice/OpenGL/GLCachedTexture.h
	DXEngine/RenderDevice/OpenGL/gl_load/gl_load.c
	DXEngine/RenderDevice/OpenGL/gl_load/gl_load.h
	DXEngine/RenderDevice/Software/SoftwareCommands.cpp
	DXEngine/RenderDevice/Software/SoftwareCommands.h
	DXEngine/RenderDevice/Software/SoftwarePipelines.h
	DXEngine/RenderDevice/Software/SoftwarePipelinesScene.cpp
	DXEngine/RenderDevice/Software/SoftwarePipelinesTile.cpp
	DXEngine/RenderDevice/Software/SoftwarePresent.cpp
	DXEngine/RenderDevice/Software/SoftwarePresent.h
	DXEngine/RenderDevice/Software/SoftwareRasterizer.cpp
	DXEngine/RenderDevice/Software/SoftwareRasterizer.h
	DXEngine/RenderDevice/Software/SoftwareReference.cpp
	DXEngine/RenderDevice/Software/SoftwareReference.h
	DXEngine/RenderDevice/Software/SoftwareRenderDevice.cpp
	DXEngine/RenderDevice/Software/SoftwareRenderDevice.h
	DXEngine/RenderDevice/Software/SoftwareSampler.h
	DXEngine/RenderDevice/Software/SoftwareSetup.cpp
	DXEngine/RenderDevice/Software/SoftwareSetup.h
	DXEngine/RenderDevice/Software/SoftwareSimd.h
	DXEngine/RenderDevice/Software/SoftwareTextureManager.cpp
	DXEngine/RenderDevice/Software/SoftwareTextureManager.h
	DXEngine/RenderDevice/Software/SoftwareTextureUploader.cpp
	DXEngine/RenderDevice/Software/SoftwareTextureUploader.h
	DXEngine/RenderDevice/Software/SoftwareWorkers.cpp
	DXEngine/RenderDevice/Software/SoftwareWorkers.h
	DXEngine/Package/Package.cpp
	DXEngine/Package/Package.h
	DXEngine/Package/PackageManager.cpp
	DXEngine/Package/PackageManager.h
	DXEngine/Package/PackageStream.cpp
	DXEngine/Package/PackageStream.h
	DXEngine/Package/PackageWriter.cpp
	DXEngine/Package/PackageWriter.h
	DXEngine/Package/PackageFlags.h
	DXEngine/Package/PackageTables.h
	DXEngine/Package/ObjectFlags.h
	DXEngine/Package/ObjectStream.h
	DXEngine/Package/IniFile.h
	DXEngine/Package/IniFile.cpp
	DXEngine/Package/IniProperty.cpp
	DXEngine/Package/IniProperty.h
	DXEngine/Package/NameString.cpp
	DXEngine/Package/NameString.h
	DXEngine/Math/FrustumPlanes.cpp
	DXEngine/Math/FrustumPlanes.h
	DXEngine/Math/halffloat.cpp
	DXEngine/Math/halffloat.h
	DXEngine/Math/hsb.cpp
	DXEngine/Math/hsb.h
	DXEngine/Math/mat.cpp
	DXEngine/Math/mat.h
	DXEngine/Math/quaternion.h
	DXEngine/Math/vec.h
	DXEngine/Math/rotator.h
	DXEngine/Math/bbox.h
	DXEngine/Math/coords.h
	DXEngine/Math/scale.h
	DXEngine/GC/GC.cpp
	DXEngine/GC/GC.h
	DXEngine/PropertyOffsets.cpp
	DXEngine/PropertyOffsets.h
	DXEngine/ObjectTravelInfo.cpp
	DXEngine/ObjectTravelInfo.h
	DXEngine/UnrealURL.cpp
	DXEngine/UnrealURL.h
	DXEngine/Packages/ConSys/UConAudioList.cpp
	DXEngine/Packages/ConSys/UConAudioList.h
	DXEngine/Packages/ConSys/UConCamera.h
	DXEngine/Packages/ConSys/UConChoice.h
	DXEngine/Packages/ConSys/UConFlagRef.h
	DXEngine/Packages/ConSys/UConItem.h
	DXEngine/Packages/ConSys/UConLight.h
	DXEngine/Packages/ConSys/UConListItem.h
	DXEngine/Packages/ConSys/UConObject.h
	DXEngine/Packages/ConSys/UConSpeech.h
	DXEngine/Packages/ConSys/UConversation.cpp
	DXEngine/Packages/ConSys/UConversation.h
	DXEngine/Packages/ConSys/UConversationList.h
	DXEngine/Packages/ConSys/UConversationMissionList.h
	DXEngine/Packages/ConSys/Events/UConEvent.cpp
	DXEngine/Packages/ConSys/Events/UConEvent.h
	DXEngine/Packages/ConSys/Events/UConEventAddCredits.h
	DXEngine/Packages/ConSys/Events/UConEventAddGoal.h
	DXEngine/Packages/ConSys/Events/UConEventAddNote.h
	DXEngine/Packages/ConSys/Events/UConEventAddSkillPoints.h
	DXEngine/Packages/ConSys/Events/UConEventAnimation.h
	DXEngine/Packages/ConSys/Events/UConEventCheckFlag.h
	DXEngine/Packages/ConSys/Events/UConEventCheckObject.h
	DXEngine/Packages/ConSys/Events/UConEventCheckPersona.h
	DXEngine/Packages/ConSys/Events/UConEventChoice.h
	DXEngine/Packages/ConSys/Events/UConEventComment.h
	DXEngine/Packages/ConSys/Events/UConEventEnd.h
	DXEngine/Packages/ConSys/Events/UConEventJump.h
	DXEngine/Packages/ConSys/Events/UConEventMoveCamera.h
	DXEngine/Packages/ConSys/Events/UConEventRandomLabel.cpp
	DXEngine/Packages/ConSys/Events/UConEventRandomLabel.h
	DXEngine/Packages/ConSys/Events/UConEventSetFlag.h
	DXEngine/Packages/ConSys/Events/UConEventSpeech.h
	DXEngine/Packages/ConSys/Events/UConEventTrade.h
	DXEngine/Packages/ConSys/Events/UConEventTransferObject.h
	DXEngine/Packages/ConSys/Events/UConEventTrigger.h
	DXEngine/Packages/ConSys/History/UConHistory.h
	DXEngine/Packages/ConSys/History/UConHistoryEvent.h
	DXEngine/Packages/Core/UClass.cpp
	DXEngine/Packages/Core/UClass.h
	DXEngine/Packages/Core/UCommandlet.h
	DXEngine/Packages/Core/UConst.cpp
	DXEngine/Packages/Core/UConst.h
	DXEngine/Packages/Core/UEnum.cpp
	DXEngine/Packages/Core/UEnum.h
	DXEngine/Packages/Core/UField.cpp
	DXEngine/Packages/Core/UField.h
	DXEngine/Packages/Core/UFunction.cpp
	DXEngine/Packages/Core/UFunction.h
	DXEngine/Packages/Core/ULanguage.h
	DXEngine/Packages/Core/UObject.cpp
	DXEngine/Packages/Core/UObject.h
	DXEngine/Packages/Core/UPackage.h
	DXEngine/Packages/Core/USimpleCommandlet.h
	DXEngine/Packages/Core/UState.cpp
	DXEngine/Packages/Core/UState.h
	DXEngine/Packages/Core/UStruct.cpp
	DXEngine/Packages/Core/UStruct.h
	DXEngine/Packages/Core/USubsystem.h
	DXEngine/Packages/Core/UTextBuffer.cpp
	DXEngine/Packages/Core/UTextBuffer.h
	DXEngine/Packages/Core/Properties/UArrayProperty.cpp
	DXEngine/Packages/Core/Properties/UArrayProperty.h
	DXEngine/Packages/Core/Properties/UBoolProperty.cpp
	DXEngine/Packages/Core/Properties/UBoolProperty.h
	DXEngine/Packages/Core/Properties/UByteProperty.cpp
	DXEngine/Packages/Core/Properties/UByteProperty.h
	DXEngine/Packages/Core/Properties/UClassProperty.cpp
	DXEngine/Packages/Core/Properties/UClassProperty.h
	DXEngine/Packages/Core/Properties/UFixedArrayProperty.cpp
	DXEngine/Packages/Core/Properties/UFixedArrayProperty.h
	DXEngine/Packages/Core/Properties/UFloatProperty.cpp
	DXEngine/Packages/Core/Properties/UFloatProperty.h
	DXEngine/Packages/Core/Properties/UIntProperty.cpp
	DXEngine/Packages/Core/Properties/UIntProperty.h
	DXEngine/Packages/Core/Properties/UMapProperty.cpp
	DXEngine/Packages/Core/Properties/UMapProperty.h
	DXEngine/Packages/Core/Properties/UNameProperty.cpp
	DXEngine/Packages/Core/Properties/UNameProperty.h
	DXEngine/Packages/Core/Properties/UObjectProperty.cpp
	DXEngine/Packages/Core/Properties/UObjectProperty.h
	DXEngine/Packages/Core/Properties/UPointerProperty.cpp
	DXEngine/Packages/Core/Properties/UPointerProperty.h
	DXEngine/Packages/Core/Properties/UProperty.cpp
	DXEngine/Packages/Core/Properties/UProperty.h
	DXEngine/Packages/Core/Properties/UStringProperty.cpp
	DXEngine/Packages/Core/Properties/UStringProperty.h
	DXEngine/Packages/Core/Properties/UStrProperty.cpp
	DXEngine/Packages/Core/Properties/UStrProperty.h
	DXEngine/Packages/Core/Properties/UStructProperty.cpp
	DXEngine/Packages/Core/Properties/UStructProperty.h
	DXEngine/Packages/DeusEx/UDeusExDecoration.cpp
	DXEngine/Packages/DeusEx/UDeusExDecoration.h
	DXEngine/Packages/DeusEx/UDeusExLevelInfo.h
	DXEngine/Packages/DeusEx/UDeusExPlayer.cpp
	DXEngine/Packages/DeusEx/UDeusExPlayer.h
	DXEngine/Packages/DeusEx/UDeusExSaveInfo.cpp
	DXEngine/Packages/DeusEx/UDeusExSaveInfo.h
	DXEngine/Packages/DeusEx/UGameDirectory.cpp
	DXEngine/Packages/DeusEx/UGameDirectory.h
	DXEngine/Packages/DeusEx/ULaserIterator.cpp
	DXEngine/Packages/DeusEx/ULaserIterator.h
	DXEngine/Packages/DeusEx/UParticleIterator.cpp
	DXEngine/Packages/DeusEx/UParticleIterator.h
	DXEngine/Packages/DeusEx/UScriptedPawn.cpp
	DXEngine/Packages/DeusEx/UScriptedPawn.h
	DXEngine/Packages/DeusExText/UDeusExTextParser.cpp
	DXEngine/Packages/DeusExText/UDeusExTextParser.h
	DXEngine/Packages/Engine/UCanvas.h
	DXEngine/Packages/Engine/UClient.h
	DXEngine/Packages/Engine/UConsole.h
	DXEngine/Packages/Engine/UEventManager.cpp
	DXEngine/Packages/Engine/UEventManager.h
	DXEngine/Packages/Engine/UPlayer.h
	DXEngine/Packages/Engine/URenderIterator.cpp
	DXEngine/Packages/Engine/URenderIterator.h
	DXEngine/Packages/Engine/UDXEngineClient.cpp
	DXEngine/Packages/Engine/UDXEngineClient.h
	DXEngine/Packages/Engine/UViewport.h
	DXEngine/Packages/Engine/Actors/UActor.cpp
	DXEngine/Packages/Engine/Actors/UActor.h
	DXEngine/Packages/Engine/Actors/UActor_Animation.cpp
	DXEngine/Packages/Engine/Actors/UActor_Base.cpp
	DXEngine/Packages/Engine/Actors/UActor_Conversation.cpp
	DXEngine/Packages/Engine/Actors/UActor_Owner.cpp
	DXEngine/Packages/Engine/Actors/UActor_Phys.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysFalling.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysFlying.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysInterpolating.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysMovingBrush.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysProjectile.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysRolling.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysRotating.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysSpider.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysSwimming.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysTrailer.cpp
	DXEngine/Packages/Engine/Actors/UActor_PhysWalking.cpp
	DXEngine/Packages/Engine/Actors/UActor_Render.cpp
	DXEngine/Packages/Engine/Actors/UActor_Touch.cpp
	DXEngine/Packages/Engine/Actors/UActor_Zone.cpp
	DXEngine/Packages/Engine/Actors/UDecal.cpp
	DXEngine/Packages/Engine/Actors/UDecal.h
	DXEngine/Packages/Engine/Actors/UHUD.h
	DXEngine/Packages/Engine/Actors/ULight.h
	DXEngine/Packages/Engine/Actors/UMenu.h
	DXEngine/Packages/Engine/Actors/UProjectile.h
	DXEngine/Packages/Engine/Actors/USpawnNotify.h
	DXEngine/Packages/Engine/Actors/Brush/UBrush.h
	DXEngine/Packages/Engine/Actors/Brush/UMover.h
	DXEngine/Packages/Engine/Actors/Decoration/UCarcass.h
	DXEngine/Packages/Engine/Actors/Decoration/UDecoration.h
	DXEngine/Packages/Engine/Actors/Info/UGameInfo.h
	DXEngine/Packages/Engine/Actors/Info/UGameReplicationInfo.h
	DXEngine/Packages/Engine/Actors/Info/UInfo.h
	DXEngine/Packages/Engine/Actors/Info/UInternetInfo.h
	DXEngine/Packages/Engine/Actors/Info/ULevelInfo.cpp
	DXEngine/Packages/Engine/Actors/Info/ULevelInfo.h
	DXEngine/Packages/Engine/Actors/Info/UMutator.h
	DXEngine/Packages/Engine/Actors/Info/UPlayerReplicationInfo.h
	DXEngine/Packages/Engine/Actors/Info/UReplicationInfo.h
	DXEngine/Packages/Engine/Actors/Info/USavedMove.h
	DXEngine/Packages/Engine/Actors/Info/USkyZoneInfo.h
	DXEngine/Packages/Engine/Actors/Info/UStatLog.h
	DXEngine/Packages/Engine/Actors/Info/UStatLogFile.h
	DXEngine/Packages/Engine/Actors/Info/UWarpZoneInfo.cpp
	DXEngine/Packages/Engine/Actors/Info/UWarpZoneInfo.h
	DXEngine/Packages/Engine/Actors/Info/UZoneInfo.h
	DXEngine/Packages/Engine/Actors/Inventory/UInventory.h
	DXEngine/Packages/Engine/Actors/Inventory/UWeapon.h
	DXEngine/Packages/Engine/Actors/Keypoint/UInterpolationPoint.h
	DXEngine/Packages/Engine/Actors/Keypoint/UKeypoint.h
	DXEngine/Packages/Engine/Actors/Keypoint/Ulocationid.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/UButtonMarker.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/UInventorySpot.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/ULiftCenter.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/ULiftExit.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/UPathNode.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/UPlayerStart.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/UTeleporter.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/UTriggerMarker.h
	DXEngine/Packages/Engine/Actors/NavigationPoint/UWarpZoneMarker.h
	DXEngine/Packages/Engine/Actors/Pawn/UCamera.h
	DXEngine/Packages/Engine/Actors/Pawn/UPawn.h
	DXEngine/Packages/Engine/Actors/Pawn/UPawn_AI.cpp
	DXEngine/Packages/Engine/Actors/Pawn/UPawn_Path.cpp
	DXEngine/Packages/Engine/Actors/Pawn/UPawn_Reach.cpp
	DXEngine/Packages/Engine/Actors/Pawn/UPawn_ReachDeusEx.cpp
	DXEngine/Packages/Engine/Actors/Pawn/UPawn_Tick.cpp
	DXEngine/Packages/Engine/Actors/Pawn/UPawn_Zone.cpp
	DXEngine/Packages/Engine/Actors/Pawn/UPlayerPawn.cpp
	DXEngine/Packages/Engine/Actors/Pawn/UPlayerPawn.h
	DXEngine/Packages/Engine/Actors/Pawn/UScout.h
	DXEngine/Packages/Engine/Actors/Triggers/UTrigger.h
	DXEngine/Packages/Engine/Actors/Triggers/UTriggers.h
	DXEngine/Packages/Engine/Network/UActorChannel.h
	DXEngine/Packages/Engine/Network/UChannel.h
	DXEngine/Packages/Engine/Network/UControlChannel.h
	DXEngine/Packages/Engine/Network/UDemoPlayPendingLevel.h
	DXEngine/Packages/Engine/Network/UDemoRecConnection.h
	DXEngine/Packages/Engine/Network/UFileChannel.h
	DXEngine/Packages/Engine/Network/UNetConnection.h
	DXEngine/Packages/Engine/Network/UNetPendingLevel.h
	DXEngine/Packages/Engine/Network/UPendingLevel.h
	DXEngine/Packages/Engine/Resources/UFont.cpp
	DXEngine/Packages/Engine/Resources/UFont.h
	DXEngine/Packages/Engine/Resources/UMusic.cpp
	DXEngine/Packages/Engine/Resources/UMusic.h
	DXEngine/Packages/Engine/Resources/UPalette.cpp
	DXEngine/Packages/Engine/Resources/UPalette.h
	DXEngine/Packages/Engine/Resources/UPrimitive.cpp
	DXEngine/Packages/Engine/Resources/UPrimitive.h
	DXEngine/Packages/Engine/Resources/USound.cpp
	DXEngine/Packages/Engine/Resources/USound.h
	DXEngine/Packages/Engine/Resources/Level/UBspNodes.cpp
	DXEngine/Packages/Engine/Resources/Level/UBspNodes.h
	DXEngine/Packages/Engine/Resources/Level/UBspSurfs.cpp
	DXEngine/Packages/Engine/Resources/Level/UBspSurfs.h
	DXEngine/Packages/Engine/Resources/Level/ULevel.cpp
	DXEngine/Packages/Engine/Resources/Level/ULevel.h
	DXEngine/Packages/Engine/Resources/Level/ULevelBase.cpp
	DXEngine/Packages/Engine/Resources/Level/ULevelBase.h
	DXEngine/Packages/Engine/Resources/Level/ULevelSummary.h
	DXEngine/Packages/Engine/Resources/Level/UModel.cpp
	DXEngine/Packages/Engine/Resources/Level/UModel.h
	DXEngine/Packages/Engine/Resources/Level/UPolys.cpp
	DXEngine/Packages/Engine/Resources/Level/UPolys.h
	DXEngine/Packages/Engine/Resources/Level/UVectors.cpp
	DXEngine/Packages/Engine/Resources/Level/UVectors.h
	DXEngine/Packages/Engine/Resources/Level/UVerts.cpp
	DXEngine/Packages/Engine/Resources/Level/UVerts.h
	DXEngine/Packages/Engine/Resources/Mesh/ULodMesh.cpp
	DXEngine/Packages/Engine/Resources/Mesh/ULodMesh.h
	DXEngine/Packages/Engine/Resources/Mesh/UMesh.cpp
	DXEngine/Packages/Engine/Resources/Mesh/UMesh.h
	DXEngine/Packages/Engine/Resources/Textures/UBitmap.h
	DXEngine/Packages/Engine/Resources/Textures/FireEngine.cpp
	DXEngine/Packages/Engine/Resources/Textures/FireEngine.h
	DXEngine/Packages/Engine/Resources/Textures/UFireTexture.cpp
	DXEngine/Packages/Engine/Resources/Textures/UFireTexture.h
	DXEngine/Packages/Engine/Resources/Textures/UFractalTexture.cpp
	DXEngine/Packages/Engine/Resources/Textures/UFractalTexture.h
	DXEngine/Packages/Engine/Resources/Textures/UIceTexture.cpp
	DXEngine/Packages/Engine/Resources/Textures/UIceTexture.h
	DXEngine/Packages/Engine/Resources/Textures/UScriptedTexture.cpp
	DXEngine/Packages/Engine/Resources/Textures/UScriptedTexture.h
	DXEngine/Packages/Engine/Resources/Textures/UTexture.cpp
	DXEngine/Packages/Engine/Resources/Textures/UTexture.h
	DXEngine/Packages/Engine/Resources/Textures/UWaterTexture.cpp
	DXEngine/Packages/Engine/Resources/Textures/UWaterTexture.h
	DXEngine/Packages/Engine/Resources/Textures/UWaveTexture.cpp
	DXEngine/Packages/Engine/Resources/Textures/UWaveTexture.h
	DXEngine/Packages/Engine/Resources/Textures/UWetTexture.cpp
	DXEngine/Packages/Engine/Resources/Textures/UWetTexture.h
	DXEngine/Packages/Engine/Subsystems/UAudioSubsystem.h
	DXEngine/Packages/Engine/Subsystems/UEditorEngine.h
	DXEngine/Packages/Engine/Subsystems/UEngine.h
	DXEngine/Packages/Engine/Subsystems/UGameEngine.h
	DXEngine/Packages/Engine/Subsystems/UNetDriver.h
	DXEngine/Packages/Engine/Subsystems/URenderBase.h
	DXEngine/Packages/Engine/Subsystems/URenderDevice.h
	DXEngine/Packages/Engine/Subsystems/UDXEngineAudioDevice.cpp
	DXEngine/Packages/Engine/Subsystems/UDXEngineAudioDevice.h
	DXEngine/Packages/Engine/Subsystems/UDXEngineNetworkDevice.cpp
	DXEngine/Packages/Engine/Subsystems/UDXEngineNetworkDevice.h
	DXEngine/Packages/Engine/Subsystems/UDXEngineRenderDevice.cpp
	DXEngine/Packages/Engine/Subsystems/UDXEngineRenderDevice.h
	DXEngine/Packages/Extension/UExtensionObject.h
	DXEngine/Packages/Extension/UExtString.cpp
	DXEngine/Packages/Extension/UExtString.h
	DXEngine/Packages/Extension/UPlayerPawnExt.cpp
	DXEngine/Packages/Extension/UPlayerPawnExt.h
	DXEngine/Packages/Extension/Flags/UFlag.h
	DXEngine/Packages/Extension/Flags/UFlagBase.cpp
	DXEngine/Packages/Extension/Flags/UFlagBase.h
	DXEngine/Packages/Extension/Flags/UFlagBool.h
	DXEngine/Packages/Extension/Flags/UFlagByte.h
	DXEngine/Packages/Extension/Flags/UFlagFloat.h
	DXEngine/Packages/Extension/Flags/UFlagInt.h
	DXEngine/Packages/Extension/Flags/UFlagName.h
	DXEngine/Packages/Extension/Flags/UFlagRotator.h
	DXEngine/Packages/Extension/Flags/UFlagVector.h
	DXEngine/Packages/Extension/Windows/UBorderWindow.cpp
	DXEngine/Packages/Extension/Windows/UBorderWindow.h
	DXEngine/Packages/Extension/Windows/UComputerWindow.cpp
	DXEngine/Packages/Extension/Windows/UComputerWindow.h
	DXEngine/Packages/Extension/Windows/UGC.cpp
	DXEngine/Packages/Extension/Windows/UGC.h
	DXEngine/Packages/Extension/Windows/UListWindow.cpp
	DXEngine/Packages/Extension/Windows/UListWindow.h
	DXEngine/Packages/Extension/Windows/UScaleManagerWindow.cpp
	DXEngine/Packages/Extension/Windows/UScaleManagerWindow.h
	DXEngine/Packages/Extension/Windows/UScaleWindow.cpp
	DXEngine/Packages/Extension/Windows/UScaleWindow.h
	DXEngine/Packages/Extension/Windows/UScrollAreaWindow.cpp
	DXEngine/Packages/Extension/Windows/UScrollAreaWindow.h
	DXEngine/Packages/Extension/Windows/UTileWindow.cpp
	DXEngine/Packages/Extension/Windows/UTileWindow.h
	DXEngine/Packages/Extension/Windows/UViewportWindow.cpp
	DXEngine/Packages/Extension/Windows/UViewportWindow.h
	DXEngine/Packages/Extension/Windows/UWindow.cpp
	DXEngine/Packages/Extension/Windows/UWindow.h
	DXEngine/Packages/Extension/Windows/TabGroup/UClipWindow.cpp
	DXEngine/Packages/Extension/Windows/TabGroup/UClipWindow.h
	DXEngine/Packages/Extension/Windows/TabGroup/UModalWindow.cpp
	DXEngine/Packages/Extension/Windows/TabGroup/UModalWindow.h
	DXEngine/Packages/Extension/Windows/TabGroup/URadioBoxWindow.cpp
	DXEngine/Packages/Extension/Windows/TabGroup/URadioBoxWindow.h
	DXEngine/Packages/Extension/Windows/TabGroup/URootWindow.cpp
	DXEngine/Packages/Extension/Windows/TabGroup/URootWindow.h
	DXEngine/Packages/Extension/Windows/TabGroup/UTabGroupWindow.cpp
	DXEngine/Packages/Extension/Windows/TabGroup/UTabGroupWindow.h
	DXEngine/Packages/Extension/Windows/Text/UButtonWindow.cpp
	DXEngine/Packages/Extension/Windows/Text/UButtonWindow.h
	DXEngine/Packages/Extension/Windows/Text/UCheckboxWindow.cpp
	DXEngine/Packages/Extension/Windows/Text/UCheckboxWindow.h
	DXEngine/Packages/Extension/Windows/Text/UEditWindow.cpp
	DXEngine/Packages/Extension/Windows/Text/UEditWindow.h
	DXEngine/Packages/Extension/Windows/Text/ULargeTextWindow.cpp
	DXEngine/Packages/Extension/Windows/Text/ULargeTextWindow.h
	DXEngine/Packages/Extension/Windows/Text/UTextLogWindow.cpp
	DXEngine/Packages/Extension/Windows/Text/UTextLogWindow.h
	DXEngine/Packages/Extension/Windows/Text/UTextWindow.cpp
	DXEngine/Packages/Extension/Windows/Text/UTextWindow.h
	DXEngine/Packages/Extension/Windows/Text/UToggleWindow.cpp
	DXEngine/Packages/Extension/Windows/Text/UToggleWindow.h
	DXEngine/Packages/IpDrv/UInternetLink.cpp
	DXEngine/Packages/IpDrv/UInternetLink.h
	DXEngine/Packages/IpDrv/UTcpLink.cpp
	DXEngine/Packages/IpDrv/UTcpLink.h
	DXEngine/Packages/IpDrv/UUdpLink.cpp
	DXEngine/Packages/IpDrv/UUdpLink.h
	DXEngine/Network/NetBits.cpp
	DXEngine/Network/NetBits.h
	DXEngine/Network/NetChannel.cpp
	DXEngine/Network/NetChannel.h
	DXEngine/Network/NetClient.cpp
	DXEngine/Network/NetClient.h
	DXEngine/Network/NetConnection.cpp
	DXEngine/Network/NetConnection.h
	DXEngine/Network/NetDriver.cpp
	DXEngine/Network/NetDriver.h
	DXEngine/Network/NetPackageMap.cpp
	DXEngine/Network/NetPackageMap.h
	DXEngine/Network/NetRemote.cpp
	DXEngine/Network/NetRemote.h
	DXEngine/Network/NetSerialize.cpp
	DXEngine/Network/NetSerialize.h
	DXEngine/Network/NetServer.cpp
	DXEngine/Network/NetServer.h
	DXEngine/Collision/TopLevel/CollisionSystem.cpp
	DXEngine/Collision/TopLevel/CollisionSystem.h
	DXEngine/Collision/TopLevel/CollisionHit.h
	DXEngine/Collision/TopLevel/CollisionTest.h
	DXEngine/Collision/TopLevel/TraceTest.cpp
	DXEngine/Collision/TopLevel/TraceTest.h
	DXEngine/Collision/TopLevel/OverlapTest.cpp
	DXEngine/Collision/TopLevel/OverlapTest.h
	DXEngine/Collision/BottomLevel/TraceRayModel.cpp
	DXEngine/Collision/BottomLevel/TraceRayModel.h
	DXEngine/Collision/BottomLevel/TraceAABBModel.cpp
	DXEngine/Collision/BottomLevel/TraceAABBModel.h
	DXEngine/Collision/BottomLevel/OverlapAABBModel.cpp
	DXEngine/Collision/BottomLevel/OverlapAABBModel.h
	DXEngine/Light/LightSystem.cpp
	DXEngine/Light/LightSystem.h
	DXEngine/Light/LightSystem_Light.cpp
	DXEngine/Light/LightSystem_Fog.cpp
	DXEngine/Light/LightActorTree.cpp
	DXEngine/Light/LightActorTree.h
	DXEngine/Light/LightEffect.cpp
	DXEngine/Light/LightEffect.h
	DXEngine/Light/LightmapAtlas.cpp
	DXEngine/Light/LightmapAtlas.h
	DXEngine/Light/LightmapBuilder.cpp
	DXEngine/Light/LightmapBuilder.h
	DXEngine/Light/Shadowmap.cpp
	DXEngine/Light/Shadowmap.h
	DXEngine/Light/FogmapBuilder.cpp
	DXEngine/Light/FogmapBuilder.h
	DXEngine/UI/WidgetResourceData.cpp
	DXEngine/UI/WidgetResourceData.h
	DXEngine/UI/ErrorWindow/ErrorWindow.cpp
	DXEngine/UI/ErrorWindow/ErrorWindow.h
)

set(DXCOMMON_WIN32_SOURCES
	DXEngine/DXEngine.natvis
	DXEngine/Utils/ComPtr.h
	DXEngine/RenderDevice/D3D11/D3D11RenderDevice.cpp
	DXEngine/RenderDevice/D3D11/D3D11RenderDevice.h
	DXEngine/RenderDevice/D3D11/D3D11FileResource.cpp
	DXEngine/RenderDevice/D3D11/D3D11FileResource.h
	DXEngine/RenderDevice/D3D11/D3D11TextureManager.cpp
	DXEngine/RenderDevice/D3D11/D3D11TextureManager.h
	DXEngine/RenderDevice/D3D11/D3D11TextureUploader.cpp
	DXEngine/RenderDevice/D3D11/D3D11TextureUploader.h
	DXEngine/RenderDevice/D3D11/D3D11UploadManager.cpp
	DXEngine/RenderDevice/D3D11/D3D11UploadManager.h
	DXEngine/RenderDevice/D3D11/D3D11CachedTexture.h
)

set(THIRDPARTY_SOURCES
	Thirdparty/dr_wav.h
	Thirdparty/minimp3_ex.h
	Thirdparty/minimp3.h
	Thirdparty/MurmurHash3/MurmurHash3.cpp
	Thirdparty/MurmurHash3/MurmurHash3.h
	Thirdparty/stb_vorbis.h
	Thirdparty/TinySHA1/TinySHA1.hpp
	Thirdparty/miniz/miniz.c
	Thirdparty/miniz/miniz.h
	Thirdparty/kissfft/kiss_fft.c
	Thirdparty/kissfft/kiss_fft.h
	Thirdparty/kissfft/kiss_fftr.c
	Thirdparty/kissfft/kiss_fftr.h
)

set(THIRDPARTY_WIN32_SOURCES
)

set(THIRDPARTY_UNIX_SOURCES
)

set(DXCOMMON_LIBS
	dxgpu
	dxwidgets
	openmpt
)

set(DXCOMMON_WIN32_LIBS
	OpenAL
	dbghelp
	shlwapi
	comctl32
	ws2_32
	d3d11
	d3dcompiler
	dxgi
	winmm
	psapi
)

set(DXCOMMON_UNIX_LIBS
	openal
)

set(DXENGINE_SOURCES
	DXEngine/MainGame.cpp
)

source_group("DXEngine" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/.+")
source_group("DXEngine\\Audio" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Audio/.+")
source_group("DXEngine\\Math" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Math/.+")
source_group("DXEngine\\Native" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Native/.+")
source_group("DXEngine\\Network" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Network/.+")
source_group("DXEngine\\Package" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Package/.+")
source_group("DXEngine\\RenderDevice" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/RenderDevice/.+")
source_group("DXEngine\\RenderDevice/Vulkan" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/RenderDevice/Vulkan/.+")
source_group("DXEngine\\RenderDevice/OpenGL" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/RenderDevice/OpenGL/.+")
source_group("DXEngine\\RenderDevice/OpenGL/gl_load" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/RenderDevice/OpenGL/gl_load/.+")
source_group("DXEngine\\RenderDevice/D3D11" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/RenderDevice/D3D11/.+")
source_group("DXEngine\\RenderDevice/Software" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/RenderDevice/Software/.+")
source_group("DXEngine\\Render" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Render/.+")
source_group("DXEngine\\UI" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/UI/.+")
source_group("DXEngine\\UI\\ErrorWindow" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/UI/ErrorWindow/.+")
source_group("DXEngine\\Packages" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/.+")
source_group("DXEngine\\Packages\\ConSys" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/ConSys/.+")
source_group("DXEngine\\Packages\\ConSys\\Events" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/ConSys/Events/.+")
source_group("DXEngine\\Packages\\ConSys\\History" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/ConSys/History/.+")
source_group("DXEngine\\Packages\\Core" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Core/.+")
source_group("DXEngine\\Packages\\Core\\Properties" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Core/Properties/.+")
source_group("DXEngine\\Packages\\DeusEx" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/DeusEx/.+")
source_group("DXEngine\\Packages\\DeusExText" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/DeusExText/.+")
source_group("DXEngine\\Packages\\Engine" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/.+")
source_group("DXEngine\\Packages\\Engine\\Actors" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Actors/.+")
source_group("DXEngine\\Packages\\Engine\\Actors\\Brush" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Actors/Brush/.+")
source_group("DXEngine\\Packages\\Engine\\Actors\\Decoration" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Actors/Decoration/.+")
source_group("DXEngine\\Packages\\Engine\\Actors\\Info" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Actors/Info/.+")
source_group("DXEngine\\Packages\\Engine\\Actors\\Inventory" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Actors/Inventory/.+")
source_group("DXEngine\\Packages\\Engine\\Actors\\Keypoint" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Actors/Keypoint/.+")
source_group("DXEngine\\Packages\\Engine\\Actors\\NavigationPoint" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Actors/NavigationPoint/.+")
source_group("DXEngine\\Packages\\Engine\\Actors\\Pawn" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Actors/Pawn/.+")
source_group("DXEngine\\Packages\\Engine\\Actors\\Triggers" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Actors/Triggers/.+")
source_group("DXEngine\\Packages\\Engine\\Network" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Network/.+")
source_group("DXEngine\\Packages\\Engine\\Resources" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Resources/.+")
source_group("DXEngine\\Packages\\Engine\\Resources\\Level" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Resources/Level/.+")
source_group("DXEngine\\Packages\\Engine\\Resources\\Mesh" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Resources/Mesh/.+")
source_group("DXEngine\\Packages\\Engine\\Resources\\Textures" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Resources/Textures/.+")
source_group("DXEngine\\Packages\\Engine\\Subsystems" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Engine/Subsystems/.+")
source_group("DXEngine\\Packages\\Extension" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Extension/.+")
source_group("DXEngine\\Packages\\Extension\\Flags" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Extension/Flags/.+")
source_group("DXEngine\\Packages\\Extension\\Windows" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Extension/Windows/.+")
source_group("DXEngine\\Packages\\Extension\\Windows\\TabGroup" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Extension/Windows/TabGroup/.+")
source_group("DXEngine\\Packages\\Extension\\Windows\\Text" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/Extension/Windows/Text/.+")
source_group("DXEngine\\Packages\\IpDrv" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Packages/IpDrv/.+")
source_group("DXEngine\\Collision" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Collision/.+")
source_group("DXEngine\\Collision\\TopLevel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Collision/TopLevel/.+")
source_group("DXEngine\\Collision\\BottomLevel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Collision/BottomLevel/.+")
source_group("DXEngine\\Light" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Light/.+")
source_group("DXEngine\\Utils" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Utils/.+")
source_group("DXEngine\\GC" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/GC/.+")
source_group("DXEngine\\VM" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/VM/.+")


source_group("Thirdparty" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty/.+")
source_group("Thirdparty\\miniz" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty/miniz/.+")
source_group("Thirdparty\\kissfft" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty/kissfft/.+")

include_directories(DXEngine DXWidgets/include Thirdparty DXGPU/include Thirdparty/openmpt Thirdparty/openal-soft/include Thirdparty/miniz)

if(WIN32)
	set(DXCOMMON_SOURCES ${DXCOMMON_SOURCES} ${DXCOMMON_WIN32_SOURCES})
	set(DXENGINE_SOURCES ${DXENGINE_SOURCES} Win32Resources/DXEngine.rc Win32Resources/DXEngine.manifest)
	set(DXCOMMON_LIBS ${DXCOMMON_LIBS} ${DXCOMMON_WIN32_LIBS})
	set(THIRDPARTY_SOURCES ${THIRDPARTY_SOURCES} ${THIRDPARTY_WIN32_SOURCES})
	add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS)
else()
	set(DXCOMMON_SOURCES ${DXCOMMON_SOURCES})
	set(DXCOMMON_LIBS ${DXCOMMON_LIBS} ${DXCOMMON_UNIX_LIBS} ${CMAKE_DL_LIBS})
	set(THIRDPARTY_SOURCES ${THIRDPARTY_SOURCES} ${THIRDPARTY_UNIX_SOURCES})
	add_definitions(-DUNIX -D_UNIX)
	add_link_options(-pthread)
endif()

if(MSVC)
	# Use all cores for compilation
	set(CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}")

	# Ignore warnings in third party code
	set_source_files_properties(${THIRDPARTY_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4244 /wd4267 /wd4005 /wd4018 /wd4101 -D_CRT_SECURE_NO_WARNINGS")
endif()

# The engine's arithmetic the same bits on every machine: no multiply and
# add fused into one, as GCC fuses them on aarch64 by default. The software
# device's lanes need it; so do the traces, which take a mover's face for the
# level's by its normal to the bit (TraceTester), a normal summed from two
# products.
if(NOT MSVC)
	add_compile_options(-ffp-contract=off)
endif()

add_library(DXCommon STATIC ${DXCOMMON_SOURCES} ${THIRDPARTY_SOURCES})
set_target_properties(DXCommon PROPERTIES CXX_STANDARD 20)
target_precompile_headers(DXCommon PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/DXEngine/Precomp.h>)
include(cmake/version.cmake)

add_executable(DXEngine WIN32 ${DXENGINE_SOURCES})
if(MINGW)
	# wWinMain, as MSVC takes it.
	target_link_options(DXEngine PRIVATE -municode)
endif()
target_link_libraries(DXEngine DXCommon ${DXCOMMON_LIBS})
set_target_properties(DXEngine PROPERTIES CXX_STANDARD 20)

function( create_zip_source_groups FOLDER_NAME ZIP_DIR )
	file(GLOB ZIP_SRCS ${ZIP_DIR}/*)
	foreach(ZIP_SRC ${ZIP_SRCS})
		if(IS_DIRECTORY ${ZIP_SRC})
			get_filename_component(DIRNAME ${ZIP_SRC} NAME)
			list(REMOVE_ITEM ZIP_SRCS ${ZIP_SRC})
			create_zip_source_groups( ${FOLDER_NAME}\\${DIRNAME} ${ZIP_SRC} )
		endif()
		source_group(${FOLDER_NAME} FILES ${ZIP_SRCS})
	endforeach()
endfunction()

function( add_zip ZIP_NAME ZIP_DIR )

	# zipdir runs on the BUILD machine to pack the resource zip, so when
	# cross-compiling the in-tree target is the wrong architecture and cannot be
	# executed. Point ZIPDIR_EXECUTABLE at a host-built one in that case.
	if (ZIPDIR_EXECUTABLE)
		set(ZIPDIR_COMMAND "${ZIPDIR_EXECUTABLE}")
		set(ZIPDIR_DEPENDS "")
	else()
		set(ZIPDIR_COMMAND zipdir)
		set(ZIPDIR_DEPENDS zipdir)
	endif()

	# Find the files in the directory, again at build time when one comes or goes
	file(GLOB_RECURSE ZIP_SRCS CONFIGURE_DEPENDS ${ZIP_DIR}/*)

	# The list of files is an input of its own, written when it changes: a
	# file deleted, or renamed with its mtime kept, left the zip newer than
	# every input and it was not packed again, the old entry kept
	string(REPLACE ";" "\n" ZIP_LIST "${ZIP_SRCS}")
	file(WRITE ${CMAKE_BINARY_DIR}/${ZIP_NAME}.list.new "${ZIP_LIST}\n")
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/${ZIP_NAME}.list.new ${CMAKE_BINARY_DIR}/${ZIP_NAME}.list)
	file(REMOVE ${CMAKE_BINARY_DIR}/${ZIP_NAME}.list.new)

	# Add command for building the zip file, again whenever one of them changes
	add_custom_command( OUTPUT ${CMAKE_BINARY_DIR}/${ZIP_NAME}
		COMMAND ${ZIPDIR_COMMAND} ${CMAKE_BINARY_DIR}/${ZIP_NAME} ${ZIP_DIR}
		DEPENDS ${ZIPDIR_DEPENDS} ${ZIP_SRCS} ${CMAKE_BINARY_DIR}/${ZIP_NAME}.list )

	# Create target project for the zip file
	# Touch zipdir executable to force rebuild of the zip file if any of the resource files change
	if (ZIPDIR_EXECUTABLE)
		add_custom_target( Resources ALL
			DEPENDS ${CMAKE_BINARY_DIR}/${ZIP_NAME}
			SOURCES ${ZIP_SRCS})
	else()
		add_custom_target( Resources ALL
			COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_FILE:zipdir>
			DEPENDS ${CMAKE_BINARY_DIR}/${ZIP_NAME}
			SOURCES ${ZIP_SRCS})
	endif()

	# Create IDE source gorups
	create_zip_source_groups("Source Files" ${ZIP_DIR})

endfunction()

set(ZIPDIR_SOURCES
	tools/zipdir/Zipdir.cpp
	tools/zipdir/ZipWriter.cpp
	tools/zipdir/ZipWriter.h
	tools/zipdir/DataBuffer.cpp
	tools/zipdir/DataBuffer.h
	tools/zipdir/IODevice.h
	tools/zipdir/MemoryDevice.cpp
	tools/zipdir/MemoryDevice.h
	tools/zipdir/FilePath.cpp
	tools/zipdir/FilePath.h
	tools/zipdir/File.cpp
	tools/zipdir/File.h
	tools/zipdir/Directory.cpp
	tools/zipdir/Directory.h
	tools/zipdir/UTF16.cpp
	tools/zipdir/UTF16.h
	Thirdparty/miniz/miniz.c
	Thirdparty/miniz/miniz.h
)

if (NOT ZIPDIR_EXECUTABLE)
	add_executable(zipdir ${ZIPDIR_SOURCES})
	set_target_properties(zipdir PROPERTIES CXX_STANDARD 20)
	set_target_properties(zipdir PROPERTIES C_STANDARD 17)
endif()

add_zip(DXEngine.pk3 ${CMAKE_CURRENT_SOURCE_DIR}/Resources)

add_dependencies(DXEngine Resources)

if(WIN32)
	add_custom_command(TARGET DXEngine POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL::OpenAL>" $<TARGET_FILE_DIR:DXEngine>)
endif()

add_custom_command(TARGET DXEngine POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${CMAKE_BINARY_DIR}/DXEngine.pk3"
	$<TARGET_FILE_DIR:DXEngine>
)

if(MSVC)
	# Create stripped PDBs for nightly builds
	set_target_properties(DXEngine PROPERTIES LINK_FLAGS_RELWITHDEBINFO /PDBSTRIPPED:${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/DXEngine-stripped.pdb)
endif()
