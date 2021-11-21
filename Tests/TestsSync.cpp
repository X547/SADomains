#include "Domains.h"

class Object1: public Object
{};

int main()
{
	auto obj1 = MakeExternal<Object1>();
	auto obj2 = MakeExternal<Object1>();
	auto obj3 = MakeExternal<Object1>();
	auto obj4 = MakeExternal<Object1>();

	auto obj1Locked = obj1.Switch();
	auto obj2Locked = obj2.Switch();
	auto obj3Locked = obj3.Switch();
	auto obj4Locked = obj4.Switch();

	CurrentDomain()->Yield();

	return 0;
}
