#include "Domains.h"
#include <stdio.h>

class Object2: public Object
{
public:
	void DoAsync()
	{
		printf("+Object2::DoAsync()\n");
	}

	void Do()
	{
		MakeAsyncRequestMth(ExternalPtr<Object2>(this), &Object2::DoAsync)->Schedule();
		printf("+Object2::Do()\n");
		CurrentDomain()->Yield();
		printf("-Object2::Do()\n");
	}
};

class Object1: public Object
{
private:
	ExternalPtr<Object2> obj2;

public:
	Object1():
		obj2(MakeExternal<Object2>())
	{}
	
	void Do()
	{
		printf("+Object1::Do()\n");
		obj2.Switch()->Do();
		printf("-Object1::Do()\n");
	}
};

int main()
{
	ExternalPtr<Object1> obj1 = MakeExternal<Object1>();
	obj1.Switch()->Do();

	obj1.GetDomain()->WaitForEmptyQueue();
	return 0;
}
