#include "SADomains/Domains.h"
#include "Syncobj.h"
#include <stdio.h>

void Do1()
{
	ExternalRef<Syncobj> syncobj1(MakeExternal<Syncobj>(0), true);
	ExternalRef<Syncobj> syncobj2(MakeExternal<Syncobj>(0), true);

	ExternalRef<Syncobj> syncobjs[] {syncobj1, syncobj2};
	uint64 points[] {1, 2};
	uint32 signaled;

	MakeAsyncRequest(ExternalPtr<Syncobj>(syncobj2), [](Syncobj *syncobj) {
		for (int i = 0; i < 100; i++) CurrentDomain()->Yield();
		printf("Signal\n");
		syncobj->Signal(2);
	})->Schedule();

	MakeAsyncRequest(ExternalPtr<Syncobj>(syncobj1), [](Syncobj *syncobj) {
		for (int i = 0; i < 50; i++) CurrentDomain()->Yield();
		printf("Signal\n");
		syncobj->Signal(1);
	})->Schedule();

	Syncobj::Wait(syncobjs, points, B_COUNT_OF(syncobjs), B_INFINITE_TIMEOUT, 1 << syncobjWaitAll, signaled);
	printf("signaled: %" B_PRIu32 "\n", signaled);
}

void Do2()
{
	ExternalRef<Syncobj> syncobj1(MakeExternal<Syncobj>(0), true);
	
	syncobj1.Switch()->Signal(0);
	syncobj1.Switch()->Signal(10);
	syncobj1.Switch()->Signal(20);
	syncobj1.Switch()->Signal(30);

	syncobj1.Switch()->Dump();
}

int main()
{
	LockedPtr<Object> mainObjLocked = MakeExternal<Object>().Switch();
	
	Do2();
	
	return 0;
}
