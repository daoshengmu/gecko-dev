
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
	void CallDad();

protected:
	virtual bool RecvInit(const nsCString& pluginPath);

	virtual bool RecvShutdown();
//
//private:
//  PRLibrary* mPluginLibrary;
};

} // End of namespace mozilla
} // End of namespace dom


#endif // mozilla_dom_HelloPluginChild_h

