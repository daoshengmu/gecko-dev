
#include "mozilla/dom/ContentChild.h"
#include "HelloPluginChild.h"
//#include "mozilla/dom/HelloPluginRequestChild.h"

// C++ file contents
namespace mozilla {
namespace dom {


//bool
//HelloPluginChild::RecvInit(const nsCString& pluginPath)
//{
//	printf("[HelloPluginChild] in RecvInit()");
//	//SendReady();
//    return false;
//}

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

//PHelloPluginRequestChild*
//HelloPluginChild::AllocPHelloPluginRequestChild(const HelloPluginRequestArgs& requestType)
//{
//    return 0;
//}
//
//bool
//HelloPluginChild::DeallocPHelloPluginRequestChild(PHelloPluginRequestChild* aActor)
//{
//    return false;
//}
//
//bool
//HelloPluginChild::RecvShutdown()
//{
//	printf("Bye! Dad.");
//    return false;
//}

MOZ_IMPLICIT HelloPluginChild::HelloPluginChild()
:mActorDestroyed(false)
{
	ContentChild::GetSingleton()->SendPHelloPluginConstructor(this); // !!!

	printf("Child is born.");
    MOZ_COUNT_CTOR(HelloPluginChild);
}

MOZ_IMPLICIT HelloPluginChild::~HelloPluginChild()
{
	printf("Child leaves.");
    MOZ_COUNT_DTOR(HelloPluginChild);

    if (!mActorDestroyed) {
   //    Send__delete__(this);
    }
}

} // namespace dom
} // namespace mozilla
