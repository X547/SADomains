#include <stdio.h>
#include <OS.h>
#include "SADomains/Domains.h"

enum {
	second = 1000000,
};

class Object1: public Object
{
private:
	int fCnt;

public:
	Object1(): fCnt(0) {}

	void Do()
	{
		for (int i = 1; i <= 20; i++) {
			printf("%d\n", i);
			snooze(second/10);
			CurrentDomain()->Yield();
		}
	}

	void Do2()
	{
		printf("%d\n", ++fCnt);
		snooze(second/10);
		if (fCnt < 20)
			CurrentDomain()->Schedule(CurrentDomain()->RootRequest());
	}
};

class Object2: public Object
{
private:
	ExternalPtr<Object1> fObj1;

public:
	Object2(ExternalPtr<Object1> obj1): fObj1(obj1) {}

	void Do()
	{
		snooze(second/5);
		for (int j = 0; j < 3; j++) {
			auto o1 = fObj1.Lock();
			for (int i = 1; i <= 3; i++) {
				printf("Object2: %d\n", i);
				snooze(second/10);
			}
		}
	}
};

int main()
{
	BReference<Domain> mainDom(new Domain(), true);
	DomainSection sect(mainDom);

	auto obj1 = MakeExternal<Object1>();
	auto obj2 = MakeExternal<Object2>(obj1);

	MakeAsyncRequestMth(obj1, &Object1::Do)->Schedule();
	MakeAsyncRequestMth(obj2, &Object2::Do)->Schedule();

	obj1.GetDomain()->WaitForEmptyQueue();
	obj2.GetDomain()->WaitForEmptyQueue();

	return 0;
}
