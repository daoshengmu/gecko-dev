
#include "GreetChild.h"

// C++ file contents
namespace mozilla {
namespace dom {
bool
GreetChild::RecvHello()
{
    return false;
}

MOZ_IMPLICIT GreetChild::GreetChild()
{
    MOZ_COUNT_CTOR(GreetChild);
}

MOZ_IMPLICIT GreetChild::~GreetChild()
{
    MOZ_COUNT_DTOR(GreetChild);
}

void GreetChild::CallDad()
{
	puts("[GreetChild] in CallDad()");
	//bool res = SendWorld();
	//puts((res)? "Send successfully":"Send failed");
}

} // namespace dom
} // namespace mozilla
