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
		Assert(CurrentDomain() == GetDomain());
		for (int i = 1; i <= 20; i++) {
			printf("%" B_PRId32 ": %d\n", find_thread(NULL), i);
			snooze(second/10);
			if (i%5 == 0) CurrentDomain()->Yield();
		}
	}

	void Do2()
	{
		printf("Do2: %d\n", ++fCnt);
		snooze(second/10);
		if (fCnt < 20)
			CurrentDomain()->Schedule(CurrentDomain()->RootRequest());
	}
};

thread_id gThread1 = B_ERROR, gThread2 = B_ERROR;
ExternalPtr<Object1> gObj1;

static status_t ThreadEntry(void *arg)
{
	gObj1.Switch()->Do();
	return B_OK;
}

int main()
{
	gObj1 = MakeExternal<Object1>();

	gThread1 = find_thread(NULL);
	gThread2 = spawn_thread(ThreadEntry, "thread2", B_NORMAL_PRIORITY, NULL);
	printf("thread1: %" B_PRId32 "\n", gThread1);
	printf("thread2: %" B_PRId32 "\n", gThread2);
	resume_thread(gThread2);

	MakeAsyncRequestMth(gObj1, &Object1::Do2)->Schedule();
	gObj1.Switch()->Do();

	gObj1.GetDomain()->WaitForEmptyQueue();
	return 0;
}
