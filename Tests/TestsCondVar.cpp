#include "SADomains/Domains.h"
#include "SADomains/DomainPointers.h"
#include "SADomains/DomainCondVars.h"
#include <stdio.h>
#include <OS.h>


enum {maxItemCount = 8};

class Queue: public Object {
	int fItems[maxItemCount];
	uint32 fFirst, fLen;
	DomainCondVar fEmptyCv, fFullCv;

public:
	Queue();

	void Insert(uint32 val);
	uint32 Remove();
};


Queue::Queue(): Object(),
	fFirst(0), fLen(0)
{}

void Queue::Insert(uint32 val)
{
	while (fLen >= maxItemCount) fFullCv.Acquire();
	fItems[(fFirst + fLen) % maxItemCount] = val;
	if (fLen == 0) fEmptyCv.Release();
	fLen++;
}

uint32 Queue::Remove()
{
	while (fLen == 0) fEmptyCv.Acquire();
	uint32 val = fItems[fFirst];
	fFirst = (fFirst + 1) % maxItemCount;
	if (fLen == maxItemCount) fFullCv.Release();
	fLen--;
	return val;
}

int main()
{
	LockedPtr<Object> mainObjLocked = MakeExternal<Object>().Switch();

	ExternalUniquePtr<Queue> queue(MakeExternal<Queue>());

	for (uint32 i = 0; i < 32; i++) {
		printf("Insert(%" B_PRId32 ")\n", i);
		queue.Switch()->Insert(i);

		if (i == maxItemCount - 1) {
			MakeAsyncRequest(ExternalPtr<Queue>(queue), [](Queue *queue) {
				uint32 val = queue->Remove();
				printf("Remove() -> %" B_PRId32 "\n", val);
				CurrentRootDomain()->Schedule(CurrentRootDomain()->CurrentRequest());
			})->Schedule();
		}
	}

	queue.GetDomain()->WaitForEmptyQueue();

	return 0;
}
