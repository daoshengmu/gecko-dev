
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
		MOZ_COUNT_CTOR(HelloPluginParent);
	}

	~HelloPluginParent() {
	  MOZ_COUNT_DTOR(HelloPluginParent);
	}

	virtual void
	ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE {

	}

protected:
	bool RecvReady() {
	//mObservers.Notify("ready for action");
	  NS_WARNING("HelloPluginParent::RecvReady");
	  return true;
	}
};

} // End of namespace mozilla
} // End of namespace dom

#endif // mozilla_dom_HelloPluginParent_h
