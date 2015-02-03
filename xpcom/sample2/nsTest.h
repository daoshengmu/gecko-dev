
#ifndef nsTest_h
#define nsTest_h

#include "nsITest.h"
#include "mozilla/Attributes.h"



// uid = 2efd97c9-7916-46a9-9362-88a5aa0e5df3
#define NS_TEST_CID \
{ 0x2efd97c9, 0x7916, 0x46a9, { 0x93, 0x62, 0x88, 0xa5, 0xaa, 0x0e, 0x5d, 0xf3 } }

#define NS_TEST_CONTRACTID "@mozilla.org/test;1"

class nsTest : public nsITest
{

public:
	NS_DECL_ISUPPORTS

	NS_DECL_NSITEST

	nsTest();

private:
	~nsTest();

	char *mValue;
};

#endif
