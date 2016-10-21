/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gamepad_GamepadPose_h
#define mozilla_dom_gamepad_GamepadPose_h

#include <stdint.h>
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class GamepadPose : public nsWrapperCache
{
public:
  explicit GamepadPose(nsISupports* aParent);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(GamepadPose)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(GamepadPose)

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool HasOrientation() const;
  bool HasPosition() const;
  void GetPosition(JSContext* aJSContext, 
                   JS::MutableHandle<JSObject*> aRetval,
                   ErrorResult& aRv);
  void GetLinearVelocity(JSContext* aJSContext, 
                         JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv);
  void GetLinearAcceleration(JSContext* aJSContext, 
                             JS::MutableHandle<JSObject*> aRetval,
                             ErrorResult& aRv);
  void GetOrientation(JSContext* aJSContext, 
                      JS::MutableHandle<JSObject*> aRetval,
                      ErrorResult& aRv);
  void GetAngularVelocity(JSContext* aJSContext, 
                          JS::MutableHandle<JSObject*> aRetval,
                          ErrorResult& aRv);
  void GetAngularAcceleration(JSContext* aJSContext, 
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv);
private:
  virtual ~GamepadPose();

  nsCOMPtr<nsISupports> mParent;
  bool mHasOrientation;
  bool mHasPosition;

  JS::Heap<JSObject*> mPosition;
  JS::Heap<JSObject*> mLinearVelocity;
  JS::Heap<JSObject*> mLinearAcceleration;
  JS::Heap<JSObject*> mOrientation;
  JS::Heap<JSObject*> mAngularVelocity;
  JS::Heap<JSObject*> mAngularAcceleration;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_gamepad_GamepadPose_h
