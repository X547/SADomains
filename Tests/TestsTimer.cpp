#include "SADomains/Domains.h"
#include "Locks/Timer.h"
#include <stdio.h>


template <typename Type>
class TimerAsyncRequestMthTmpl: public Timer, public AsyncRequest
{
private:
	void (Type::*fMethod)();

public:
	TimerAsyncRequestMthTmpl(ExternalPtr<Type> ptr, void (Type::*method)()):
		Timer(),
		AsyncRequest(ExternalPtr<Object>(ptr)),
		fMethod(method)
	{}

	void Do() final
	{
		Schedule();
	}

	void Do(Object *ptr) final
	{
		(((Type*)ptr)->*fMethod)();
	}

};

template <typename Type>
Timer *MakeTimerAsyncRequestMth(ExternalPtr<Type> ptr, void (Type::*Method)())
{
	return new TimerAsyncRequestMthTmpl<Type>(ptr, Method);
}


class TestObject: public Object
{
private:
	uint32 fCount;

public:
	TestObject(): fCount(0) {}

	void Do()
	{
		printf("TestObject::Do(): %" B_PRIu32 "\n", fCount++);
		Timer *timer = MakeTimerAsyncRequestMth(ExternalPtr<TestObject>(this), &TestObject::Do);
		gTimerRoster.Schedule(timer, system_time() + 10);
	}
};


int main()
{
	auto p1 = MakeExternal<TestObject>();
	
	MakeAsyncRequestMth(p1, &TestObject::Do)->Schedule();
	
	fgetc(stdin);
	p1.GetDomain()->WaitForEmptyQueue();
	return 0;
}
