
#include "HelloPluginChild.h"

namespace mozilla {
namespace dom {

// C++ file contents
void
HelloPluginChild::CallDad()
{

	puts("[HelloPluginChild] in CallDad()");
	bool res = SendReady();
	puts((res)? "Send successfully":"Send failed");
}

bool
HelloPluginChild::RecvInit(const nsCString& pluginPath)
{
	SendReady();
    return false;
}

bool
HelloPluginChild::RecvShutdown()
{
    return false;
}

MOZ_IMPLICIT HelloPluginChild::HelloPluginChild()
{
    MOZ_COUNT_CTOR(HelloPluginChild);
}

MOZ_IMPLICIT HelloPluginChild::~HelloPluginChild()
{
    MOZ_COUNT_DTOR(HelloPluginChild);
}

} // namespace dom
} // namespace mozilla
