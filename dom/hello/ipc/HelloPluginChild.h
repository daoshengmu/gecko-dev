
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

    virtual PHelloPluginRequestChild*
	AllocPHelloPluginRequestChild(const HelloPluginRequestArgs& requestType);

	virtual bool
	DeallocPHelloPluginRequestChild(PHelloPluginRequestChild* aActor);
	void CallDad();

protected:
	virtual bool RecvInit(const nsCString& pluginPath);

	virtual bool RecvShutdown();
//
//private:
//  PRLibrary* mPluginLibrary;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HelloPluginChild_h

