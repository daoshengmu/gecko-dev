/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VRControllerOpenVRManager_h_
#define mozilla_dom_VRControllerOpenVRManager_h_

// OpenVR Interfaces
namespace vr {
class IVRSystem;
};

namespace mozilla {
namespace dom {

class VRContollerManagerOpenVR : public VRControllerManager
{
public:
  // TODO: aPath argument can be removed when we can get the correct OpenVR runtime
  // from the system.
  static already_AddRefed<VRContollerManagerOpenVR> Create(const nsString& aPath);

  virtual bool Startup() override;
  virtual void Shutdown() override;
  virtual void HandleInput() override;

private:
  VRContollerManagerOpenVR();
  ~VRContollerManagerOpenVR();

  void ScanForDevices();

  nsCOMPtr<nsIThread> mMonitorThread;
  // Hashtable for converting tracked devices to gamepad index.
  nsDataHashtable<nsUint32HashKey, uint32_t> mGamepads;

  int mTrackedControllerCount;
  uint32_t mChannel;
  vr::IVRSystem *mVRSystem;

  const uint32_t kOpenVRControllerButtons = 8;
  const uint32_t kOpenVRControllerAxes = 5;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_VRControllerOpenVRManager_h_
