
#include "GreetParent.h"

// C++ file contents
namespace mozilla {
namespace dom {
bool
GreetParent::RecvWorld()
{
	//return false;
	puts("[GreetParent] in RecvWorld()");
	return true;
}

void
GreetParent::DoSomething()
{
	puts("[GreetParent] in DoSomething()");
	//SendHello();
}

void
GreetParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

MOZ_IMPLICIT GreetParent::GreetParent()
{
    MOZ_COUNT_CTOR(GreetParent);
}

MOZ_IMPLICIT GreetParent::~GreetParent()
{
    MOZ_COUNT_DTOR(GreetParent);
}

} // namespace dom
} // namespace mozilla

