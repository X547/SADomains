#pragma once

#include <Referenceable.h>
#include "SADomains/Domains.h"
#include "SADomains/DomainCondVars.h"
#include <private/shared/AutoDeleter.h>
#include <private/kernel/util/DoublyLinkedList.h>

#include <set>


enum {
	syncobjCreateSignaled,
};

enum {
	syncobjWaitAll,
	syncobjWaitForSubmit,
	syncobjWaitAvailable,

	syncobjWaitReady = 31,
};


class Syncobj: public RefObject {
private:
	struct Fence: public BReferenceable {
		bool fSignaled;
		
	};

	struct Point {
		ObjectDeleter<Point> prev;
		uint64 value;
		BReference<Fence> fence;
	};

	struct WaitList {
		ExternalRef<Syncobj> *syncobjs;
		uint64 *points;
		uint32 count;
		uint32 flags;
		uint32 *signaled;
		DomainCondVar condVar;
		
		bool Check();
	};

	ObjectDeleter<Point> fPoints;
	uint64 fValue;
	std::set<WaitList*> fWaitLists;
	
	Point *FindPoint(uint64 value);

public:
	Syncobj(uint32 flags);
	virtual ~Syncobj();

	uint64 Value() {return fValue;}
	status_t Signal(uint64 value);
	status_t Reset();
	status_t Transfer(ExternalRef<Syncobj> src, uint64 dstPoint, uint64 srcPoint, uint32 flags);

	static status_t Wait(ExternalRef<Syncobj> *syncobjs, uint64 *points, uint32 count, bigtime_t timeout, uint32 flags, uint32 &signaled);
	
	void Dump();
};
