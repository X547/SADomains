#pragma once

#include "Domains.h"
#include <private/kernel/util/DoublyLinkedList.h>


class DomainCondVar {
private:
	struct WaitItem: public DoublyLinkedListLinkImpl<WaitItem> {
		Domain *dom;
		Request *req;
		int val;
	};

	Mutex fLocker;
	DoublyLinkedList<WaitItem> fWaitItems;

public:
	status_t Acquire(uint32 flags = 0, bigtime_t timeout = B_INFINITE_TIMEOUT);
	void Release(bool releaseAll = false);
};
