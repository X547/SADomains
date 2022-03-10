#include "SADomains/Domains.h"


struct Object1: public Object
{
};

struct Object2: public Object1
{
};

struct RefObject1: public RefObject
{
	int val;
};


int main()
{
	Object1 *obj1 = new Object1();
	Object2 *obj2 = new Object2();
	ExternalPtr<Object1> extPtr1;
	extPtr1 = NULL;
	extPtr1 = obj1;
	extPtr1 = obj2;
	extPtr1 = MakeExternal<Object1>();
	extPtr1 = MakeExternal<Object2>();

	ExternalPtr<Object1> extPtr2(NULL);
	ExternalPtr<Object1> extPtr3(obj1);
	ExternalPtr<Object1> extPtr4(obj2);
	ExternalPtr<Object1> extPtr5(extPtr1);
	ExternalPtr<Object1> extPtr6(MakeExternal<Object1>());
	ExternalPtr<Object1> extPtr7(MakeExternal<Object2>());

	ExternalRef<RefObject1> extRef1(NULL);
	ExternalRef<RefObject1> extRef2(MakeExternal<RefObject1>(), true);
	extRef2.Switch()->val = 123;

	ExternalUniquePtr<Object1> uniqPtr1;
	ExternalUniquePtr<Object1> uniqPtr2(NULL);
	ExternalUniquePtr<Object1> uniqPtr3(obj1);
	ExternalUniquePtr<Object1> uniqPtr4(MakeExternal<Object1>());
}
