/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Hello.h"
#include "mozilla/dom/HelloBinding.h"

#include <cstdlib> // For rand() and srand()
#include <ctime> // For time()

namespace mozilla {
namespace dom {


// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(Hello)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Hello)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Hello)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Hello)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//Hello::Hello()
Hello::Hello(nsISupports* aParent):mParent(aParent)
{
}

Hello::Hello(nsISupports* aParent, const nsAString& aStr):mParent(aParent), hStr(aStr)
{
}

Hello::~Hello()
{
    // Add |MOZ_COUNT_DTOR(Hello);| for a non-refcounted object.
}

JSObject*
Hello::WrapObject(JSContext* aCx)
{
  return HelloBinding::Wrap(aCx, this);
}

void
Hello:: GetHelloStr(nsString& aRetVal) const
{
	aRetVal = hStr;
}

void
Hello::SetHelloStr(const nsAString& str)
{
	hStr.Assign(str);
}

void
Hello::GetReadonlyStr(nsString& aRetVal) const
{
	aRetVal = nsString(NS_LITERAL_STRING("This string can be read only!"));
}

void
Hello::SayHello(nsString& aRetVal)
{
	aRetVal = nsString(NS_LITERAL_STRING("Hello FireFox!"));
}

int32_t
Hello::RandNum()
{
	static bool bInit = false;

	if ( !bInit )
	{
		bInit = true;
		std::srand(std::time(0));	// Using current time as the rand seed generator
	}

	return std::rand() % 100;
}

int32_t
Hello::Add( int32_t a, int32_t b )
{
	return a + b;
}

int32_t
Hello::Sub( int32_t a, int32_t b )
{
	return a - b;
}

int32_t
Hello::Mul( int32_t a, int32_t b )
{
	return a * b;
}

double
Hello::Div( int32_t a, int32_t b, ErrorResult& aRv )
{
	if ( !b )
	{
		// If it is invalid value, return ERROR.
		aRv.Throw(NS_ERROR_ILLEGAL_VALUE);
	}
	else
	{
		return (double)a / b;
	}

}

/* static */already_AddRefed<Hello>
Hello::Constructor(const GlobalObject& global, ErrorResult& aRv)
{

	nsRefPtr<Hello> obj = new Hello(global.GetAsSupports());
	return obj.forget();
}

/* static */already_AddRefed<Hello>
Hello::Constructor(const GlobalObject& global, const nsAString& str, ErrorResult& aRv)
{
	nsRefPtr<Hello> obj = new Hello(global.GetAsSupports(), str);
	return obj.forget();
}









} // namespace dom
} // namespace mozilla
