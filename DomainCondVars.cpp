#include "DomainCondVars.h"
#include "Locks/AutoLock.h"


status_t DomainCondVar::Acquire(uint32 flags, bigtime_t timeout)
{
	// TODO: implement timeout
	(void)flags;
	(void)timeout;

	WaitItem waitItem;
	waitItem.dom = CurrentRootDomain();
	waitItem.req = waitItem.dom->CurrentRequest();
	{
		AutoLock lock(fLocker);
		fWaitItems.Insert(&waitItem);
	}
	Domain::Wait();
	return B_OK;
}

void DomainCondVar::Release(bool releaseAll)
{
	if (releaseAll) {
		DoublyLinkedList<WaitItem> list;
		{
			AutoLock lock(fLocker);
			list.MoveFrom(&fWaitItems);
		}
		for (;;) {
			WaitItem *item = list.RemoveHead();
			if (item == NULL) break;
			item->dom->Schedule(item->req);
		}
	} else {
		WaitItem *item;
		{
			AutoLock lock(fLocker);
			item = fWaitItems.RemoveHead();
		}
		if (item != NULL) {
			item->dom->Schedule(item->req);
		}
	}
}
