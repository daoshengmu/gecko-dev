/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWrapperCache.h"

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/GamepadPoseBinding.h"
#include "mozilla/dom/GamepadPose.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(GamepadPose)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(GamepadPose)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mPosition = nullptr;
  tmp->mLinearVelocity = nullptr;
  tmp->mLinearAcceleration = nullptr;
  tmp->mOrientation = nullptr;
  tmp->mAngularVelocity = nullptr;
  tmp->mAngularAcceleration = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(GamepadPose)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(GamepadPose)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPosition)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLinearVelocity)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLinearAcceleration)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mOrientation)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAngularVelocity)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAngularAcceleration)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(GamepadPose, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(GamepadPose, Release)

GamepadPose::GamepadPose(nsISupports* aParent)
  : mParent(aParent),
    mHasPosition(false),
    mHasOrientation(false),
    mPosition(nullptr),
    mLinearVelocity(nullptr),
    mLinearAcceleration(nullptr),
    mOrientation(nullptr),
    mAngularVelocity(nullptr),
    mAngularAcceleration(nullptr)
{
  mozilla::HoldJSObjects(this);
}

GamepadPose::~GamepadPose()
{
  mozilla::DropJSObjects(this);
}

/* virtual */ JSObject*
GamepadPose::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GamepadPoseBinding::Wrap(aCx, this, aGivenProto);
}

bool GamepadPose::HasOrientation() const
{
	return mHasOrientation;
}

bool GamepadPose::HasPosition() const
{
  return mHasPosition;
}

void GamepadPose::GetPosition(JSContext* aJSContext, 
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv)
{
 // return mHasPosition;
}

void GamepadPose::GetLinearVelocity(JSContext* aJSContext, 
                                    JS::MutableHandle<JSObject*> aRetval,
                                    ErrorResult& aRv)
{
 // return mHasPosition;
}

void GamepadPose::GetLinearAcceleration(JSContext* aJSContext, 
                                        JS::MutableHandle<JSObject*> aRetval,
                                        ErrorResult& aRv)
{
 // return mHasPosition;
}

void GamepadPose::GetOrientation(JSContext* aJSContext, 
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aRv)
{
 // return mHasPosition;
}

void GamepadPose::GetAngularVelocity(JSContext* aJSContext, 
                                     JS::MutableHandle<JSObject*> aRetval,
                                     ErrorResult& aRv)
{
 // return mHasPosition;
}

void GamepadPose::GetAngularAcceleration(JSContext* aJSContext, 
                                         JS::MutableHandle<JSObject*> aRetval,
                                         ErrorResult& aRv)
{
 // return mHasPosition;
}

} // namespace dom
} // namespace mozilla
