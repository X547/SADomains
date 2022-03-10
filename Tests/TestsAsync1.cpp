#include "SADomains/Domains.h"
#include <stdio.h>


class TestObject: public Object
{
public:
	void Do()
	{
		printf("+TestObject::Do()\n");
		MakeAsyncRequestMth(ExternalPtr<TestObject>(this), &TestObject::Do2)->Schedule();
		snooze(100000);
		printf("+Yield()\n");
		CurrentDomain()->Yield();
		printf("-Yield()\n");
		snooze(100000);
		printf("-TestObject::Do()\n");
	}
	
	void Do2()
	{
		printf("+TestObject::Do2()\n");
		snooze(100000);
		printf("-TestObject::Do2()\n");
	}
};


int main()
{
	auto p1 = MakeExternal<TestObject>();
	
	MakeAsyncRequestMth(p1, &TestObject::Do)->Schedule();
	
	p1.GetDomain()->WaitForEmptyQueue();
	return 0;
}
