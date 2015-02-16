
This sample refers from https://github.com/ChunMinChang/mozDevDoc/blob/master/Creating_a_IPDL.md

#Unit test

First of all, open your mozconfig, and append ```ac_add_options --enable-ipdl-tests``` to enable the ipdl unit tests.
At MOZ_CEN/ipc/ipdl/test/cxx. Create a pTestFoo.ipdl file and copy the below code into your file.
```
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
```
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

After building, go to ```obj-YOUR-TARGET-XXXX/ipc/ipdl/_ipdlheaders/mozilla/_ipdltest``` you will find ```PTestFooChild.h``` and ```PTestFooParent.h```. Copy and paste the content of these files into your new files, ```testFoo.cpp``` and ```testFoo.h```, that you create at ```MOZ_CEN/ipc/ipdl/test/cxx```.

Next, we have to add some dummy functions for ipdl unit test. 

First, add these interfaces into your class ```TestFooParent``` in testFoo.h
```
 static bool RunTestInProcesses() { return true; }
 static bool RunTestInThreads() { return false; }

 void Main();
```

Then, implement the ```Main``` function in testFoo.cpp
```
void
TestFooParent::Main()
{
	if ( !SendHello() )
		fail("Sending Hello Fail.");
}

```

Finally, we implement the test code in our functions to show the results. The below code is the final version of our ```testFoo.cpp```.

```
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
```
cd $OBJDIR/dist/bin
 ./run-mozilla.sh ./ipdlunittest TestFoo
```


#WebIDL

In this WebIDL experiment, we want to show how to communicate between the content process and the chrome process on IPC tabs.

To begin with, we have to discuss on IPC tabs, who is parent? and who is child? Because they have different authority between them. If we gave them the wrong one, we couldn't succeed our experiment.

On IPC tabs, parent is chrome process, and child is content process  (https://developer.mozilla.org/en-US/docs/IPDL/Tutorial). Therefore, when we want to send message to chrome process on the tab. We have to instance the child IPDL not the parent one. If we understood it, the experiment would be easy to be implemented.




#Reference:
IPDL unit test, https://wiki.mozilla.org/IPDL/Unit_test_generation
IPDL tutorial, https://developer.mozilla.org/en-US/docs/IPDL/Tutorial

