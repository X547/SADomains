#include <stdio.h>

#include "SADomains/Domains.h"


class DeleteRequest: public AsyncRequest
{
public:
	DeleteRequest(ExternalPtr<Object> ptr): AsyncRequest(ptr) {}

	void Do(Object *ptr) final
	{
		delete ptr;
	}

};


class TestObject: public Object
{
public:
	int32 counter;

	TestObject(): counter(0) {}
	void Do()
	{
		printf("%p.Do(%" B_PRId32 ")\n", this, counter);
		counter++;
		printf("(1)\n");
		CurrentDomain()->Yield();
		printf("(2)\n");
		if (counter < 10000)
			CurrentDomain()->Schedule(CurrentDomain()->CurrentRequest());
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

void MakeTestReq(ExternalPtr<Object> ptr)
{
	ptr.GetDomain()->Schedule(new TestRequest(ptr));
}


class TestObject2: public Object
{
public:
	int val;

	void Do()
	{
		printf("(1)\n");
		printf("(2)\n");
		printf("(3)\n");
	}
};



int main()
{
	BReference<Domain> mainDom(new Domain(), true);
	DomainSection sect(mainDom);

	auto p1 = MakeExternal<TestObject>();
	auto p2 = MakeExternal<TestObject>();

	//p2.Lock()->Do();

	MakeTestReq(p1);
	MakeTestReq(p2);

/*
	snooze(10000);

	auto lockP1 = p1.Lock();
	p1.GetDomain()->Cancel(curRequest);
	lockP1.Unset();
*/
	p1.GetDomain()->WaitForEmptyQueue();
	p2.GetDomain()->WaitForEmptyQueue();
	return 0;
}
