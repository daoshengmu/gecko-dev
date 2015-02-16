
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
	ContentChild::GetSingleton()->SendPHelloPluginConstructor(this); // To activate the connection between parent/child

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
