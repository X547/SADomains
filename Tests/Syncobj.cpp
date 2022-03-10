#include "Syncobj.h"
#include <AutoDeleter.h>
#include <stdio.h>


Syncobj::Syncobj(uint32 flags):
	fValue(0)
{
	(void)flags;
	//printf("+Syncobj\n");
}

Syncobj::~Syncobj()
{
	//printf("-Syncobj\n");
}


Syncobj::Point* Syncobj::FindPoint(uint64 value)
{
	Point *point = fPoints.Get();
	while (point != NULL && !(point->value >= value)) point = point->prev.Get();
	return point;
}


status_t Syncobj::Signal(uint64 value)
{
	BReference<Fence> fence(new Fence(), true);
	fence->fSignaled = true;
	ObjectDeleter<Point> point(new Point{.prev = fPoints.Detach(), .value = value, .fence = fence});
	fPoints.SetTo(point.Detach());
	return B_OK;

	fValue = value;
	for (auto it = fWaitLists.begin(); it != fWaitLists.end(); it++) {
		WaitList *waitList = *it;
		waitList->condVar.Release();
	}
	return B_OK;
}

status_t Syncobj::Reset()
{
	fValue = 0;
	return B_OK;
}

status_t Syncobj::Transfer(ExternalRef<Syncobj> src, uint64 dstPoint, uint64 srcPoint, uint32 flags)
{
	(void)src;
	(void)dstPoint;
	(void)srcPoint;
	(void)flags;
	//printf("[!] Syncobj::Transfer: not implemented\n");
	return B_OK;
}


bool Syncobj::WaitList::Check()
{
	if ((1 << syncobjWaitAll) & flags) {
		for (uint32 i = 0; i < count; i++) {
			if (!(syncobjs[i].Switch()->fValue >= points[i])) {
				return false;
			}
		}
		*signaled = 0;
		return true;
	} else {
		for (uint32 i = 0; i < count; i++) {
			if (syncobjs[i].Switch()->fValue >= points[i]) {
				*signaled = i;
				return true;
			}
		}
		return false;
	}
}

status_t Syncobj::Wait(ExternalRef<Syncobj> *syncobjs, uint64 *points, uint32 count, bigtime_t timeout, uint32 flags, uint32 &signaled)
{
	(void)timeout;

	signaled = 0;
	return B_OK; // !!!

	WaitList wait {
		.syncobjs = syncobjs,
		.points = points,
		.count = count,
		.flags = flags,
		.signaled = &signaled
	};
	for (uint32 i = 0; i < count; i++) {
		syncobjs[i].Switch()->fWaitLists.emplace(&wait);
	}
	while (!wait.Check()) {
		wait.condVar.Acquire();
	}
	for (uint32 i = 0; i < count; i++) {
		syncobjs[i].Switch()->fWaitLists.erase(&wait);
	}
	return B_OK;
}


void Syncobj::Dump()
{
	bool first = true;
	for (Point *point = fPoints.Get(); point != NULL; point = point->prev.Get()) {
		if (first) first = false; else printf(", ");
		printf("%" B_PRIu64 "(%d)", point->value, point->fence->fSignaled);
	}
	printf("\n");
}
