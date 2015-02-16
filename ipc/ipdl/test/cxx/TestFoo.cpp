
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

#include "IPDLUnitTests.h"      // fail etc.
