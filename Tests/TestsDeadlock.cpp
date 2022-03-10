#include <OS.h>
#include "SADomains/Domains.h"
#include "Locks/AutoLock.h"
#include <stdio.h>

enum {
	second = 1000000,
};

class Object1: public Object
{
};

class Object2: public Object
{
public:
	int fId;
	ExternalPtr<Object1> fObj1;
	ExternalPtr<Object1> fObj2;

	void Do()
	{
		{AutoLock lock(gPrintLock); printf("%d: fObj1.Lock()\n", fId);}
		auto obj1Locked = fObj1.Lock();
		snooze(second/100);
		{AutoLock lock(gPrintLock); printf("%d: fObj2.Lock()\n", fId);}
		auto obj2Locked = fObj2.Lock();
		{AutoLock lock(gPrintLock); printf("%d: -Do()\n", fId);}
	}
};

int main()
{
	auto obj1 = MakeExternal<Object1>();
	auto obj2 = MakeExternal<Object1>();

	auto obj3 = MakeExternal<Object2>();
	auto obj4 = MakeExternal<Object2>();

	{
		auto obj3Locked = obj3.Lock();
		obj3Locked->fId = 1;
		obj3Locked->fObj1 = obj1;
		obj3Locked->fObj2 = obj2;
	}
	{
		auto obj4Locked = obj4.Lock();
		obj4Locked->fId = 2;
		obj4Locked->fObj1 = obj2;
		obj4Locked->fObj2 = obj1;
	}

	MakeAsyncRequestMth(obj3, &Object2::Do)->Schedule();
	MakeAsyncRequestMth(obj4, &Object2::Do)->Schedule();

	obj3.GetDomain()->WaitForEmptyQueue();
	obj4.GetDomain()->WaitForEmptyQueue();

	return 0;
}
