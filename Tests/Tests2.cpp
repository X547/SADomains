#include <stdio.h>

#include "SADomains/Domains.h"


class TestRoster: public Object
{
public:
	void Do(int32 id, int32 val)
	{
		Assert(CurrentDomain() == GetDomain());
		static int count = 0;
		count++;
		Assert(count <= 1);
		printf("%" B_PRId32 ": TestRoster{\n", id);
		printf("%" B_PRId32 ": TestRoster: %" B_PRId32 "\n", id, val);
		printf("%" B_PRId32 ": TestRoster}\n", id);
		snooze(100000);
		count--;
		CurrentDomain()->Yield();
		count++;
		Assert(count <= 1);
		printf("%" B_PRId32 ": TestRoster2{\n", id);
		printf("%" B_PRId32 ": TestRoster2: %" B_PRId32 "\n", id, val);
		printf("%" B_PRId32 ": TestRoster2}\n", id);
		snooze(100000);
		count--;
	}
};


auto testRoster = MakeExternal<TestRoster>();


class TestObject: public Object
{
public:
	int32 id;
	int32 counter;

	TestObject(int32 id): id(id), counter(0) {}
	void Do()
	{
		Assert(CurrentDomain() == GetDomain());
		//while (counter < 10) {
			testRoster.Switch()->Do(id, counter);
			counter++;
		//}
		if (counter < 10) {
			CurrentDomain()->Schedule(CurrentDomain()->CurrentRequest());
		} else {
			printf("%" B_PRId32 ".Done\n", id);
		}
	}
};


class TestRequest: public AsyncRequest
{

public:
	TestRequest(ExternalPtr<Object> ptr): AsyncRequest(ptr) {}

	void Do(Object *ptr) final
	{
		TestObject *obj = dynamic_cast<TestObject*>(ptr);
		obj->Do();
	}

};


int main()
{
	LockedPtr<Object> mainObjLocked = MakeExternal<Object>().Switch();

	auto p1 = MakeExternal<TestObject>(1);
	auto p2 = MakeExternal<TestObject>(2);

	MakeAsyncRequestMth(p1, &TestObject::Do)->Schedule();
	MakeAsyncRequestMth(p2, &TestObject::Do)->Schedule();

	//(new TestRequest(p1))->Schedule();
	//(new TestRequest(p2))->Schedule();

	p1.GetDomain()->WaitForEmptyQueue();
	p2.GetDomain()->WaitForEmptyQueue();

	return 0;
}
