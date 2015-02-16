
#ifndef mozilla_dom_HelloPluginChild_h
#define mozilla_dom_HelloPluginChild_h

#include "mozilla/dom/PHelloPluginChild.h"

namespace mozilla {
namespace dom {

class HelloPluginChild : public PHelloPluginChild
{

public:
	MOZ_IMPLICIT HelloPluginChild();
	virtual ~HelloPluginChild();

//    virtual PHelloPluginRequestChild*
//	AllocPHelloPluginRequestChild(const HelloPluginRequestArgs& requestType);
//
//	virtual bool
//	DeallocPHelloPluginRequestChild(PHelloPluginRequestChild* aActor);

	void
	DoStuff();

	void
	ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

	bool
	RecvWorld();

private:
	bool mActorDestroyed;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HelloPluginChild_h

