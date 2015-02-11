
#ifndef mozilla_dom_HelloPluginParent_h
#define mozilla_dom_HelloPluginParent_h

#include "mozilla/dom/PHelloPluginParent.h"

namespace mozilla {
namespace dom {

class HelloPluginParent : public PHelloPluginParent
{
public:
	HelloPluginParent() {
    // launch child plugin process
		printf("Dad is born.");
		MOZ_COUNT_CTOR(HelloPluginParent);
	}

	~HelloPluginParent() {
	  MOZ_COUNT_DTOR(HelloPluginParent);
	}

	virtual PHelloPluginRequestParent*
	AllocPHelloPluginRequestParent(const HelloPluginRequestArgs& requestType);

	virtual bool
	DeallocPHelloPluginRequestParent(PHelloPluginRequestParent* aActor);

	virtual void
	ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE {

	}

protected:
	bool RecvReady() {
	//mObservers.Notify("ready for action");
		printf("I receive my son's message.");
		return true;
	}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HelloPluginParent_h
