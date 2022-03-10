#include "SADomains/Domains.h"
#include <stdio.h>


class TestObject: public Object
{
public:
	TestObject() {printf("+TestObject()\n");}
	~TestObject() {printf("-TestObject()\n");}
	void Do()
	{
		printf("TestObject::Do()\n");
	}
};


int main()
{
	auto p1 = MakeExternal<TestObject>();
	
	MakeAsyncRequestMth(p1, &TestObject::Do)->Schedule();

	p1.Delete();
	
	return 0;
}
