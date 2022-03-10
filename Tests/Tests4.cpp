#include "SADomains/Domains.h"

class Object1: public Object
{
private:
	int fCnt;

public:
	Object1(): fCnt(0)
	{}
	
	void Do()
	{
		fCnt++;
	};
};

int main()
{
	auto obj1 = MakeExternal<Object1>();

	{
		auto obl1Locked = obj1.Lock();
		for (int i = 0; i < 40000000; i++)
			MakeAsyncRequestMth(obj1, &Object1::Do)->Schedule();
	}
	obj1.GetDomain()->WaitForEmptyQueue();
	return 0;
}
