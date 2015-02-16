
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
Append ```pTestFoo.ipdl``` into moz.build. Like the below:
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

And then, we have to add some dummy functions for ipdl unit test. 

First, add these interfaces into your class ```TestFooParent``` in testFoo.h
```
 static bool RunTestInProcesses() { return true; }
 static bool RunTestInThreads() { return false; }

 void Main();
```

Following, implement the ```Main``` function in testFoo.cpp
```
void
TestFooParent::Main()
{
	if ( !SendHello() )
		fail("Sending Hello Fail.");
}

```


#Webidl


#Reference:
IPDL unit test, https://wiki.mozilla.org/IPDL/Unit_test_generation
