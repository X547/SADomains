#include "SADomains/Domains.h"
#include <stdio.h>


struct TestTask: public Object
{
	bool run, done;

	TestTask(): run(true), done(false) {}

	void Do()
	{
		while (run) {
			CurrentDomain()->Yield();
		}
		done = true;
	}

	void Do2()
	{
		if (run) {
			CurrentRootDomain()->Schedule(CurrentRootDomain()->CurrentRequest());
			return;
		}
		done = true;
	}

};


int main()
{
	auto task = MakeExternal<TestTask>();
	MakeAsyncRequestMth(task, &TestTask::Do2)->Schedule();
	printf("[WAIT]"); fgetc(stdin);
	task.Switch()->run = false;
	while (!task.Switch()->done) {}
	task.Delete();
	return 0;
}
