
#Introduction

This sample refers from https://github.com/ChunMinChang/mozDevDoc/blob/master/Creating_a_IPDL.md

In this article, I would talk about the IPDL examples to make unit tests and how to use IPDL on Firefox browser tab. 

#Unit test

First of all, open your mozconfig, and append ```ac_add_options --enable-ipdl-tests``` to enable the ipdl unit tests.
At ```MOZ_CEN/ipc/ipdl/test/cxx```. Create a ```PTestFoo.ipdl``` file and copy the below code into your file.
```cpp
namespace mozilla {
namespace _ipdltest {

async protocol PTestFoo
{

child:
	
	Hello();

parent:
	
	World();
	__delete__();
	
};

} // namespace _ipdltest
} // namespace mozilla

```
Append ```pTestFoo.ipdl``` into MOZ_CEN/ipc/ipdl/test/cxx/moz.build. Like the below:
```cpp
IPDL_SOURCES += [
    ...
    'PTestFailedCtorSub.ipdl',
    'PTestFailedCtorSubsub.ipdl',
    'PTestFoo.ipdl',
    'PTestHandle.ipdl',
    ...
]
```
Build it.
<pre>
$ cd MOZ_CEN/
$ ./mach build
</pre>

After building, go to ```$OBJDIR/ipc/ipdl/_ipdlheaders/mozilla/_ipdltest``` you will find ```PTestFooChild.h``` and ```PTestFooParent.h```. Copy and paste the content of these files into your new files, ```testFoo.cpp``` and ```testFoo.h```, that you create at ```MOZ_CEN/ipc/ipdl/test/cxx```.

Next, we have to add some dummy functions for ipdl unit test. 

First, add these interfaces into your class ```TestFooParent``` in testFoo.h
```cpp
 static bool RunTestInProcesses() { return true; }
 static bool RunTestInThreads() { return false; }

 void Main();
```

Then, implement the ```Main()``` function in testFoo.cpp
```cpp
void
TestFooParent::Main()
{
	if ( !SendHello() )
		fail("Sending Hello Fail.");
}

```
Finally, we implement the test code in our functions to show the results. The below code is the final version of our ```testFoo.cpp```.

```cpp
#include "TestFoo.h"
#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

void
TestFooParent::Main()
{
	if ( !SendHello() )
		fail("Sending Hello Fail.");
}

bool
TestFooParent::RecvWorld()
{
	puts("[TestFooParent] in RecvWorld().");

	if ( !SendHello() )
		fail("Sending Hello Fail.");

	return true;
}

void
TestFooParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

MOZ_IMPLICIT TestFooParent::TestFooParent()
{
    MOZ_COUNT_CTOR(TestFooParent);
}

MOZ_IMPLICIT TestFooParent::~TestFooParent()
{
    MOZ_COUNT_DTOR(TestFooParent);
}

bool
TestFooChild::RecvHello()
{
	puts("[TestFooChild] in RecvHello().");

	if ( !SendWorld() )
		fail("Sending World Fail.");

    return true;
}

MOZ_IMPLICIT TestFooChild::TestFooChild()
{
    MOZ_COUNT_CTOR(TestFooChild);
}

MOZ_IMPLICIT TestFooChild::~TestFooChild()
{
    MOZ_COUNT_DTOR(TestFooChild);
}

} // namespace _ipdltest
} // namespace mozilla
```

Now, we can start to run the unit test
<pre>
$ cd $OBJDIR/dist/bin
$ ./run-mozilla.sh ./ipdlunittest TestFoo
</pre>
You can check my source code here, https://github.com/daoshengmu/gecko-dev/tree/practice/ipc/ipdl/test/cxx.

#WebIDL

In this WebIDL experiment, we want to show how to communicate between the content process and the chrome process on IPC tabs.

To begin with, we have to discuss on IPC tabs, who is parent? and who is child? Because they have different authority between them. If we gave them the wrong one, we couldn't achieve our expectation.

On IPC tabs, parent is chrome process, and child is content process  (https://developer.mozilla.org/en-US/docs/IPDL/Tutorial). Therefore, when we want to send message to chrome process on the tab. We have to instance the child IPDL not the parent one. If we understood this, the experiment would be easy to be implemented.

##Write IPDL plugin

Firstly, create a ```PHelloPlugin.ipdl``` file at ```MOZ_CEN/dom/hello/ipc```. Add the below code into this file:
```cpp
include protocol PContent;

namespace mozilla {
namespace dom {

async protocol PHelloPlugin
{
	manager PContent;
	
child:
	
	World();	
	
parent:

	Hello();
	 __delete__();	
	
};

} // namespace dom
} // namespace mozilla
```

And then, append ```PHelloPlugin.ipdl``` into ```MOZ_CEN/dom/hello/moz.build```.
```cpp
IPDL_SOURCES += [
    'ipc/PHelloPlugin.ipdl',
]
```

Build it.
<pre>
$ cd MOZ_CEN
$ ./mach build
</pre>

Secondly, we copy and paste the source code from ```$OBJDIR/ipc/ipdl/_ipdlheaders/mozilla/dom/PHelloPluginParent.h and PHelloPluginChild.h``` to ```MOZ_CEN/dom/hello/ipc/HelloPluginParent.h, HelloPluginParent.cpp, PHelloPluginChild.h, PHelloPluginChild.cpp```.

And we implement HelloPluginParent.cpp in this way to help us do the experiment
```cpp
bool HelloPluginParent::RecvHello()
{
	printf("[HelloPluginParent] in RecvHello()");

	if ( !SendWorld() )
		puts("[HelloPluginParent] SendWorld() fail");

	return true;
}
```

In ```HelloPluginChild.cpp```, except for the testing code, we add an interface ```void HelloPluginChild::DoStuff()``` to help us to get the function call from the browser tab. Moreover, we have to add ```ContentChild::GetSingleton()->SendPHelloPluginConstructor(this);``` in the ```HelloPlugChild``` constructor to activate the IPDL connection
```cpp
MOZ_IMPLICIT HelloPluginChild::HelloPluginChild()
:mActorDestroyed(false)
{
	// To activate the connection between parent/child
	ContentChild::GetSingleton()->SendPHelloPluginConstructor(this); 

	printf("Child is born.");
    MOZ_COUNT_CTOR(HelloPluginChild);
}
```

The final code of ```HelloPluginChild.cpp``` is like below:
```cpp
#include "mozilla/dom/ContentChild.h"
#include "HelloPluginChild.h"

// C++ file contents
namespace mozilla {
namespace dom {

void HelloPluginChild::DoStuff()
{
	printf("[HelloPluginChild] in DoStuff()");

	if ( !SendHello() )
		puts("[HelloPluginChild] in DoStuff(). Send Hello Fail");

}

void
HelloPluginChild::ActorDestroy(ActorDestroyReason aWhy)
{
	mActorDestroyed = true;
}

bool
HelloPluginChild::RecvWorld()
{
	puts("[HelloPluginChild] in RecvWorld()");

	if ( !SendHello() )
		puts("[HelloPluginChild] SendHello() fail");

	return true;
}

MOZ_IMPLICIT HelloPluginChild::HelloPluginChild()
:mActorDestroyed(false)
{
	// To activate the connection between parent/child
	ContentChild::GetSingleton()->SendPHelloPluginConstructor(this); 
	
	printf("Child is born.");
    MOZ_COUNT_CTOR(HelloPluginChild);
}

MOZ_IMPLICIT HelloPluginChild::~HelloPluginChild()
{
	printf("Child leaves.");

	if (!mActorDestroyed) {
		Send__delete__(this);
	}

    MOZ_COUNT_DTOR(HelloPluginChild);
}

} // namespace dom
} // namespace mozilla

```
Then, we add the files that we use into ```MOZ_CEN/dom/hello/ipc/moz.build```. 
```cpp
EXPORTS.mozilla.dom += [
    'HelloPluginChild.h',
    'HelloPluginParent.h',  
]

UNIFIED_SOURCES += [
    'HelloPluginChild.cpp',
    'HelloPluginParent.cpp',   
]

FAIL_ON_WARNINGS = True

LOCAL_INCLUDES += [
    '/dom/base',
]

include('/ipc/chromium/chromium-config.mozbuild')

FINAL_LIBRARY = 'xul'
```

## Write WebIDL

In the final step, we start to write our WebIDL code. We create ```HelloIPDL.webidl``` in ```MOZ_CEN/dom/webidl``` and add ```HelloIPDL.webidl``` into ```MOZ_CEN/dom/webidl/moz.build```
```cpp
HelloIPDL.webidl
[Constructor,Constructor(DOMString str)]
interface HelloIPDL {
    attribute DOMString helloStr;
    readonly attribute DOMString readonlyStr;
    DOMString sayHello();
};
```
After writing the WebIDL file, we rebuild our backend

<pre>
# Build backend : Generate a backend used to build the tree.
$ cd MOZ_CEN/
$ ./mach build-backend
</pre>

And generate the template file (cpp, h) 
<pre>
$ ./mach webidl-example HelloIPDL
</pre>

Create and complete the HelloIPDL.cpp and HelloIPDL.h by copying and pasting the content from $OBJDIR/dom/bindings/HelloIPDL-example.cpp HelloIPDL-example.h.

***HelloIPDL.h***
```cpp
#ifndef mozilla_dom_HelloIPDL_h
#define mozilla_dom_HelloIPDL_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/HelloPluginChild.h"

struct JSContext;

namespace mozilla {
namespace dom {

class HelloIPDL MOZ_FINAL : public nsISupports /* or NonRefcountedDOMObject if this is a non-refcounted object */,
                            public nsWrapperCache /* Change wrapperCache in the binding configuration if you don't want this */
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(HelloIPDL)

public:
  //Hello();
  HelloIPDL(nsISupports* aParent);
  HelloIPDL(nsISupports* aParent, const nsAString& aStr);

protected:
  ~HelloIPDL();

public:
  // TODO: return something sensible here, and change the return type
  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<HelloIPDL> Constructor(const GlobalObject& global, ErrorResult& aRv);
  static already_AddRefed<HelloIPDL> Constructor(const GlobalObject& global, const nsAString& str, ErrorResult& aRv);

  void GetHelloStr(nsString& aRetVal) const;

  void SetHelloStr(const nsAString& arg);

  void GetReadonlyStr(nsString& aRetVal) const;

  void SayHello(nsString& aRetVal);

private:
  nsCOMPtr<nsISupports> mParent;
  nsString hStr;

  HelloPluginChild* ipdlChild;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HelloIPDL_h
```
In this example, we want to emit message from child to parent when we click the button on the tab. Therefore, in the HelloIPDL::SayHello(), we instance ```HelloPluginChild``` and make it do something for us.

***HelloIPDL.cpp***
```cpp

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

  if ( ipdlChild )
  {
	  delete ipdlChild;
	  ipdlChild = nullptr;
  }

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

```
Finally, we add these files into our ```MOZ_CEN/dom/hello/moz.build```
```cpp
DIRS += [
    'ipc',
]
    
EXPORTS.mozilla.dom += [
    'HelloIPDL.h',
]

UNIFIED_SOURCES += [
    'HelloIPDL.cpp',
]
```
Build it.
<pre>
$ cd MOZ_CEN/
$ ./mach build
</pre>

##Make a html file to demo this example.
Create ```testHelloIPDL.html``` at ```MOZ_CEN``` and add code like below.
```html
<!doctype html>

<html lang="en">
<head>
    <meta charset="utf-8">
    <title>helloIPDL test</title>
    <script>
        funcInit = function(){
            // After DOM has been loaded....
            console.log("DOM has been loaded...");
            var btnElement = document.querySelector('#btn');
            btn.addEventListener('click', clickHandler);

        }
        document.addEventListener('DOMContentLoaded', funcInit);

        console.log("test page is starting...");

        var clickHandler = function(){
            console.log("click trigger");
            var h = new HelloIPDL();
            console.log("sayHello : " + h.sayHello());
        }

    </script>
</head>

<body>
    <H1>Test Page</H1>
    <button id="btn">click me</button>
</body>
</html>
```
Demo it.
<pre>
$ cd MOZ_CEN/
$ ./mach run testHelloIPDL.html
</pre>

You can check my source code here, https://github.com/daoshengmu/gecko-dev/tree/practice/dom/hello and 
https://github.com/daoshengmu/gecko-dev/tree/practice/dom/webidl.

#Reference:
IPDL unit test, https://wiki.mozilla.org/IPDL/Unit_test_generation.

IPDL tutorial, https://developer.mozilla.org/en-US/docs/IPDL/Tutorial.

