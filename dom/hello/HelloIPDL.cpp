/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HelloIPDL.h"
#include "mozilla/dom/HelloIPDLBinding.h"


namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(HelloIPDL)
NS_IMPL_CYCLE_COLLECTING_ADDREF(HelloIPDL)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HelloIPDL)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HelloIPDL)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//HelloIPDL::HelloIPDL()
//{}
HelloIPDL::HelloIPDL(nsISupports* aParent):mParent(aParent), ipdlChild(nullptr)
{

}

HelloIPDL::HelloIPDL(nsISupports* aParent, const nsAString& aStr):mParent(aParent), hStr(aStr), ipdlChild(nullptr)
{

}

HelloIPDL::~HelloIPDL()
{
    // Add |MOZ_COUNT_DTOR(HelloIPDL);| for a non-refcounted object.

	if ( ipdlChild )
	{
		//delete ipdlChild;	// Bug: it will cause the send/recv connection error.
		//ipdlChild = nullptr;
	}

	printf("~HelloIPDL() I am dead....");
}

JSObject*
HelloIPDL::WrapObject(JSContext* aCx)
{
  return HelloIPDLBinding::Wrap(aCx, this);
}

void
HelloIPDL::GetHelloStr(nsString& aRetVal) const
{
  aRetVal = hStr;
}

void
HelloIPDL::SetHelloStr(const nsAString& arg)
{
  hStr.Assign(arg);
}

void
HelloIPDL::GetReadonlyStr(nsString& aRetVal) const
{
  aRetVal = nsString(NS_LITERAL_STRING("This string can be read only!"));
}

void
HelloIPDL::SayHello(nsString& aRetVal)
{
  aRetVal = nsString(NS_LITERAL_STRING("HelloIPDL FireFox!"));

  ipdlChild = new HelloPluginChild();	// Bug: we need to find a way to delete it.

  if ( ipdlChild )
  {
	  ipdlChild->DoStuff();
  }

  printf( "[HelloIPDL] in SayHello()" );
}

/* static */already_AddRefed<HelloIPDL>
HelloIPDL::Constructor(const GlobalObject& global, ErrorResult& aRv)
{
	nsRefPtr<HelloIPDL> obj = new HelloIPDL(global.GetAsSupports());
	return obj.forget();
}

/* static */already_AddRefed<HelloIPDL>
HelloIPDL::Constructor(const GlobalObject& global, const nsAString& str, ErrorResult& aRv)
{
	nsRefPtr<HelloIPDL> obj = new HelloIPDL(global.GetAsSupports(), str);
	return obj.forget();
}


} // namespace dom
} // namespace mozilla

