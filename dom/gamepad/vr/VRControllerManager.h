/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VRControllerManager_h_
#define mozilla_dom_VRControllerManager_h_

namespace mozilla {
namespace dom {

class VRControllerManager {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRControllerManager)

  virtual bool Startup() = 0;
  virtual void Shutdown() = 0;
  virtual void HandleInput() = 0;

protected:
  VRControllerManager() : mInstalled(false) {}
  virtual ~VRControllerManager() {}

  bool mInstalled;
  uint32_t mChannel;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_VRControllerManager_h_