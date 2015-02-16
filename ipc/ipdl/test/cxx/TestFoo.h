
#ifndef mozilla_ipdltest_TestFoo_h
#define mozilla_ipdltest_TestFoo_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestFooParent.h"
#include "mozilla/_ipdltest/PTestFooChild.h"


// Header file contents
namespace mozilla {
namespace _ipdltest {

class TestFooParent :
    public PTestFooParent
{

public:

    MOZ_IMPLICIT TestFooParent();
    virtual ~TestFooParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

	void Main();
	virtual void
	ActorDestroy(ActorDestroyReason aWhy);
	virtual bool
	RecvWorld();

};

class TestFooChild :
    public PTestFooChild
{

public:

    virtual bool
    RecvHello();

    MOZ_IMPLICIT TestFooChild();
    virtual ~TestFooChild();
};


} // namespace _ipdltest
} // namespace mozilla

#endif // mozilla_ipdltest_TestFoo_h

