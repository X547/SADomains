#include "SADomains/Domains.h"
#include "SADomains/DomainPointers.h"
#include <stdio.h>
#include <OS.h>



class TestObject: public Object {
public:
	volatile int32 counter;

	TestObject(): counter(0) {}
};

int main()
{
	LockedPtr<Object> mainObjLocked = MakeExternal<Object>().Switch();

	enum {count = 10000000};
	bigtime_t dt1, dt2;
	{
		ExternalUniquePtr<TestObject> obj(MakeExternal<TestObject>());
		bigtime_t t0 = system_time();
		for (int32 i = 0; i < count; i++) {
			obj.Switch()->counter++;
		}
		bigtime_t t1 = system_time();
		dt1 = t1 - t0;
		printf("%g calls/sec\n", count/(double(dt1)/1000000.0));
	}

	{
		TestObject obj;
		RecursiveLock lock;
		enum {count = 1000000};
		bigtime_t t0 = system_time();
		for (int32 i = 0; i < count; i++) {
			lock.Acquire();
			obj.counter++;
			lock.Release();
		}
		bigtime_t t1 = system_time();
		dt2 = t1 - t0;
		printf("%g calls/sec\n", count/(double(dt2)/1000000.0));
	}
	printf("%g%%\n", double(dt1)*100.0/double(dt2));

	return 0;
}
