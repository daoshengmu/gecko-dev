
#include "HelloPluginChild.h"
#include "mozilla/dom/HelloPluginRequestChild.h"

// C++ file contents
namespace mozilla {
namespace dom {


void
HelloPluginChild::CallDad()
{

	printf("I am your child. I call my dad.");

	puts("[HelloPluginChild] in CallDad()");
	bool res = SendReady();
	puts((res)? "Send successfully":"Send failed");
}

bool
HelloPluginChild::RecvInit(const nsCString& pluginPath)
{
	printf("I am your child.");
	SendReady();
    return false;
}

PHelloPluginRequestChild*
HelloPluginChild::AllocPHelloPluginRequestChild(const HelloPluginRequestArgs& requestType)
{
    return 0;
}

bool
HelloPluginChild::DeallocPHelloPluginRequestChild(PHelloPluginRequestChild* aActor)
{
    return false;
}

bool
HelloPluginChild::RecvShutdown()
{
	printf("Bye! Dad.");
    return false;
}

MOZ_IMPLICIT HelloPluginChild::HelloPluginChild()
{
	printf("Child is born.");
    MOZ_COUNT_CTOR(HelloPluginChild);
}

MOZ_IMPLICIT HelloPluginChild::~HelloPluginChild()
{
	printf("Child leaves.");
    MOZ_COUNT_DTOR(HelloPluginChild);
}

} // namespace dom
} // namespace mozilla
