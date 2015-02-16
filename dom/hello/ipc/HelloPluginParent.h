
#ifndef mozilla_dom_HelloPluginParent_h
#define mozilla_dom_HelloPluginParent_h

#include "mozilla/dom/PHelloPluginParent.h"

namespace mozilla {
namespace dom {

class HelloPluginParent : public PHelloPluginParent
{
public:
	HelloPluginParent(): mActorDestroyed(false) {
    // launch child plugin process
		printf("Dad is born.");
		MOZ_COUNT_CTOR(HelloPluginParent);
	}

	~HelloPluginParent() {
	  MOZ_COUNT_DTOR(HelloPluginParent);

	  if (!mActorDestroyed) {

	  }
	}

	bool RecvHello();

//	virtual PHelloPluginRequestParent*
//	AllocPHelloPluginRequestParent(const HelloPluginRequestArgs& requestType);
//
//	virtual bool
//	DeallocPHelloPluginRequestParent(PHelloPluginRequestParent* aActor);

	virtual void
	ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE {
		mActorDestroyed = true;
	}

private:
	bool mActorDestroyed;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HelloPluginParent_h
