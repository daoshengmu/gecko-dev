/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Hello_h
#define mozilla_dom_Hello_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#include "nsCOMPtr.h" // For already_AddRefed
#include "mozilla/dom/BindingDeclarations.h" // For GlobalObject

struct JSContext;

namespace mozilla {
namespace dom {

class Hello MOZ_FINAL : public nsISupports /* or NonRefcountedDOMObject if this is a non-refcounted object */,
                        public nsWrapperCache /* Change wrapperCache in the binding configuration if you don't want this */
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Hello)

public:
  //Hello();
  Hello(nsISupports* aParent);
  Hello(nsISupports* aParent, const nsAString& aStr);

protected:
  ~Hello();

private:
  nsCOMPtr<nsISupports> mParent;
  nsString hStr;

public:
  // TODO: return something sensible here, and change the return type
 // Hello* GetParentObject() const;
  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<Hello> Constructor(const GlobalObject& global, ErrorResult& aRv);
  static already_AddRefed<Hello> Constructor(const GlobalObject& global, const nsAString& str, ErrorResult& aRv);

  void GetHelloStr(nsString& aRetVal) const;

  void SetHelloStr(const nsAString& arg);

  void GetReadonlyStr(nsString& aRetVal) const;

  void SayHello(nsString& aRetVal);

  int32_t RandNum();

  int32_t Add(int32_t a, int32_t b);

  int32_t Mul(int32_t a, int32_t b);

  int32_t Sub(int32_t a, int32_t b);

  double Div(int32_t a, int32_t b, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Hello_h
