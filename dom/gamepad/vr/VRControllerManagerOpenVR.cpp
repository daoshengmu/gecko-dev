/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlink.h"
#include "nsString.h"
#include "mozilla/Preferences.h"
#include "gfxPrefs.h"
#include "openvr.h"

#include "VRControllerManagerOpenVR.h"

namespace {
extern "C" {
typedef uint32_t(VR_CALLTYPE * pfn_VR_InitInternal)(::vr::HmdError *peError, ::vr::EVRApplicationType eApplicationType);
typedef void (VR_CALLTYPE * pfn_VR_ShutdownInternal)();
typedef bool (VR_CALLTYPE * pfn_VR_IsRuntimeInstalled)();
typedef void * (VR_CALLTYPE * pfn_VR_GetGenericInterface)(const char *pchInterfaceVersion, ::vr::HmdError *peError);
} // extern "C"
} // namespace

static pfn_VR_InitInternal vr_InitInternal = nullptr;
static pfn_VR_ShutdownInternal vr_ShutdownInternal = nullptr;
static pfn_VR_IsRuntimeInstalled vr_IsRuntimeInstalled = nullptr;
static pfn_VR_GetGenericInterface vr_GetGenericInterface = nullptr;

namespace mozilla {
namespace dom {

bool
LoadOpenVRRuntime(const char* aPath)
{
  static PRLibrary *openvrLib = nullptr;

  openvrLib = PR_LoadLibrary(aPath);
  if (!openvrLib)
    return false;

#define REQUIRE_FUNCTION(_x) do {                                       \
  *(void **)&vr_##_x = (void *) PR_FindSymbol(openvrLib, "VR_" #_x);  \
  if (!vr_##_x) { printf_stderr("VR_" #_x " symbol missing\n"); return false; } \
  } while (0)

  REQUIRE_FUNCTION(InitInternal);
  REQUIRE_FUNCTION(ShutdownInternal);
  REQUIRE_FUNCTION(IsRuntimeInstalled);
  REQUIRE_FUNCTION(GetGenericInterface);

#undef REQUIRE_FUNCTION

  return true;
}

class VRControllerServiceRunnable final : public Runnable
{
public:
  explicit VRControllerServiceRunnable(VRControllerManager* aVRControllerManager)
   : mControllerManager(aVRControllerManager) {}

  NS_IMETHOD Run() override
  {
    mControllerManager->HandleInput();
    NS_DispatchToCurrentThread(new VRControllerServiceRunnable(mControllerManager));
    return NS_OK;
  }

private:
  ~VRControllerServiceRunnable() {}

  VRControllerManager MOZ_NON_OWNING_REF *mControllerManager;
};

VRContollerManagerOpenVR::VRContollerManagerOpenVR()
  : mTrackedControllerCount(0), mVRSystem(nullptr)
{
}

/*static*/ already_AddRefed<VRContollerManagerOpenVR>
VRContollerManagerOpenVR::Create(const nsString& aPath)
{
  if (!gfxPrefs::VREnabled() || !gfxPrefs::VROpenVREnabled()) {
    return nullptr;
  }

  if (!LoadOpenVRRuntime(NS_ConvertUTF16toUTF8(aPath).get())) {
    return nullptr;
  }

  RefPtr<VRContollerManagerOpenVR> manager = new VRContollerManagerOpenVR();
  return manager.forget();
}

bool
VRContollerManagerOpenVR::Startup()
{
  if (mInstalled)
    return true;

  if (!vr_IsRuntimeInstalled())
    return false;

  // Loading the OpenVR Runtime
  vr::EVRInitError err = vr::VRInitError_None;

  vr_InitInternal(&err, vr::VRApplication_Scene);
  if (err != vr::VRInitError_None) {
    return false;
  }

  mVRSystem = (vr::IVRSystem *)vr_GetGenericInterface(vr::IVRSystem_Version, &err);
  if ((err != vr::VRInitError_None) || !mVRSystem) {
    vr_ShutdownInternal();
    return false;
  }

  ScanForDevices();

  // Start watching gamepad event in loop
  NS_NewThread(getter_AddRefs(mMonitorThread), new VRControllerServiceRunnable(this));

  mInstalled = true;
  return true;
}

void
VRContollerManagerOpenVR::Shutdown()
{
  mMonitorThread->Shutdown();

  mInstalled = false;
  RefPtr<GamepadPlatformService> service =
    GamepadPlatformService::GetParentService();

  if (!service) {
    return;
  }

  for (auto iter = mGamepads.Iter(); !iter.Done(); iter.Next()) {
    service->RemoveGamepad(mChannel, mGamepads.Get(iter.Key()));
  }

  mGamepads.Clear();
}

VRContollerManagerOpenVR::~VRContollerManagerOpenVR()
{
  Shutdown();
}

void
VRContollerManagerOpenVR::HandleInput()
{
  RefPtr<GamepadPlatformService> service =
    GamepadPlatformService::GetParentService();

  if (!service) {
    return;
  }

  // Process SteamVR controller state
  for (vr::TrackedDeviceIndex_t trackedDevice = 0;
       trackedDevice < vr::k_unMaxTrackedDeviceCount; trackedDevice++ ) {
    vr::VRControllerState_t state;

    if (mVRSystem->GetTrackedDeviceClass(trackedDevice)
        != vr::TrackedDeviceClass_Controller) {
      continue;
    }

    if (mVRSystem->GetControllerState(trackedDevice, &state)) {
      if (state.ulButtonPressed) {
        // TODO: Convert the button mask to an ID button
        service->NewButtonEvent(mChannel,
                                mGamepads.Get(trackedDevice),
                                0,
                                true);
      }
    }
  }

  return;
}

void
VRContollerManagerOpenVR::ScanForDevices()
{
  mTrackedControllerCount = 0;

  // Basically, we would have HMDs in the tracked devices, but we are just interested in the controllers.
  for ( vr::TrackedDeviceIndex_t trackedDevice = vr::k_unTrackedDeviceIndex_Hmd + 1;
        trackedDevice < vr::k_unMaxTrackedDeviceCount; ++trackedDevice ) {
    if (!mVRSystem->IsTrackedDeviceConnected(trackedDevice)) {
      continue;
    }

    if (mVRSystem->GetTrackedDeviceClass(trackedDevice) != vr::TrackedDeviceClass_Controller) {
      continue;
    }

		RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
    MOZ_ASSERT(service);

    mChannel = service->GetCurrentChannelId();
    uint32_t index = service->AddGamepad(mChannel, "OpenVR Gamepad",
                                         GamepadMappingType::_empty,
                                         kOpenVRControllerButtons,
                                         kOpenVRControllerAxes);
    mGamepads.Put(trackedDevice, index);
    ++mTrackedControllerCount;
  }
}

} // namespace dom
} // namespace mozilla