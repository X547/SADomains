#include "Domains.h"

#include <stdio.h>
#include "../Locks/AutoLock.h"

#include <algorithm>
#include <map>
#include <cxxabi.h>

void Assert(bool cond)
{
	if (!cond) {
		fprintf(stderr, "assert failed\n");
		debugger("assert failed");
		exit(1);
	}
}

static char *CppDemangle(const char *abiName)
{
  int status;
  char *ret = abi::__cxa_demangle(abiName, 0, 0, &status);
  return ret;
}


enum {
	doTrace = false,
};

RecursiveLock gPrintLock;

template<typename... Args>
void Trace(Args&&... args)
{
	if (doTrace)
		printf(std::forward<Args>(args)...);
}

static void WriteRequest(Request *req)
{
	if (!doTrace)
		return;

	static int32 newId = 1;
	static std::map<Request*, int32> dict;

	AutoLock lock(gPrintLock);

	int32 id;
	auto it = dict.find(req);
	if (it == dict.end()) {
		id = newId++;
		dict[req] = id;
	} else {
		id = it->second;
	}
	char *name = CppDemangle(typeid(*req).name());
	Trace("%s[%d](state: {", name, id);
	free(name); name = NULL;
	bool first = true;
	for (int i = 0; i < 32; i++) {
		if (SetIn(i, req->state)) {
			if (first) first = false; else Trace(", ");
			switch (i) {
			case pendingRequest: Trace("pending"); break;
			case runningRequest: Trace("running"); break;
			case doneRequest: Trace("done"); break;
			case cancelledRequest: Trace("cancelled"); break;
			case waitingRequest: Trace("waiting"); break;
			default: Trace("? (%d)", i);
			}
		}
	}
	Trace("})");
}

static void WriteSubrequests(Request *req)
{
	AutoLock lock(gPrintLock);
	WriteRequest(req); Trace(" {\n");
	for (Request *cur = req->nextSub; cur != NULL; cur = cur->nextSub) {
		Trace("\t"); WriteRequest(cur); Trace("\n");
	}
	Trace("}\n");
}


thread_local Domain *gCurDomain = NULL;
ThreadPool gThreadPool;

Domain *CurrentDomain()
{
	return gCurDomain;
}

static inline void SetDomain(Domain *d)
{
	gCurDomain = d;
}


Object::Object():
	fDom(CurrentDomain())
{
	Assert(fDom != NULL);
}

Object::~Object()
{}


//#pragma mark Requests

Request::Request():
	state(0),
	next(NULL),
	nextSub(NULL),
	fSem(0)
{}

Request::~Request()
{SetIncl(state, 31);}

void Request::Resolved()
{
	WriteRequest(this); Trace(".Resolved\n");
	delete this;
}


AsyncRequest::AsyncRequest(ExternalPtr<Object> ptr):
	fPtr(ptr)
{
	Assert(fPtr != NULL);
}


SyncRequest::SyncRequest(Domain *root, Domain *dst):
	fRoot(root),
	fDst(dst),
	fRefCnt(1)
{
}

SyncRequest::~SyncRequest()
{
}


static Domain *GetRequestDomain(Request *_req)
{
	if (auto req = dynamic_cast<AsyncRequest*>(_req))
		return req->TargetDomain();
	if (auto req = dynamic_cast<SyncRequest*>(_req))
		return req->fDst;
	Assert(false);
	return NULL;
}


//#pragma mark Domain

Domain::Domain():
	fQueue(NULL),
	fQueueEnd(NULL),
	fSubrequests(NULL),
	fExiting(false)
{
	Trace("+Domain(%p)\n", this);
}

Domain::~Domain()
{
	Trace("-Domain(%p)\n", this);
	AutoLock lock(fLocker);
	Assert(!fExiting);
	fExiting = true;
	if (fQueue != NULL) {
		auto syncReq = dynamic_cast<SyncRequest*>(fQueue);
		auto asyncReq = dynamic_cast<AsyncRequest*>(fQueue);
		if (syncReq != NULL || (asyncReq != NULL && asyncReq->fPtr.GetDomain() == this))
			Done(fQueue, doneRequest);
		WaitForEmptyQueue();
	}
	Trace("-Domain(%p): done\n", this);
	Assert(fQueue == NULL);
}

void Domain::Run(Request *_req)
{
	AutoLock lock(fLocker);
	Trace("%p.Run(", this); WriteRequest(_req); Trace(")\n");
	SetExcl(_req->state, pendingRequest);
	SetIncl(_req->state, runningRequest);
	if (auto req = dynamic_cast<SyncRequest*>(_req)) {
		SetExcl(req->state, waitingRequest);
		req->fSem.Release();
	} else if (auto req = dynamic_cast<AsyncRequest*>(_req)) {
		if (SetIn(waitingRequest, req->state)) {
			SetExcl(req->state, waitingRequest);
			Trace("resuming %p\n", req);
			req->fSem.Release();
		} else {
			ThreadPoolItem *pi = gThreadPool.Take(this);
			pi->fSem.Release();
		}
	} else
		Assert(false); // unknown request type
}

void Domain::Done(Request *req, RequestFlag flag)
{
	AutoLock lock(fLocker);
	Trace("%p.Done(", this); WriteRequest(req); Trace(", %d)\n", flag != doneRequest);
	Assert(req == fQueue && SetIn(runningRequest, req->state));
	Assert((fQueue != NULL && fQueueEnd != NULL) || (fQueue == NULL && fQueueEnd == NULL));
	Request *next = req->next;
	req->next = NULL;
	fQueue = next; if (next == NULL) fQueueEnd = NULL;
	SetExcl(req->state, runningRequest);
	SetIncl(req->state, flag);
	if (SetIn(doneRequest, req->state) && !SetIn(pendingRequest, req->state))
		req->Resolved();
	if (next == NULL) {
		if (SetIn(pendingRequest, req->state)) {
			SetExcl(req->state, pendingRequest);
			Schedule(req); // calls Run
		} else {
			Trace("%p: last request\n", this);
			fEmptyQueueCV.Release(true);
		}
	} else {
		if (SetIn(pendingRequest, req->state)) {
			SetExcl(req->state, pendingRequest);
			Schedule(req);
		}
		Run(next);
	}
	Assert((fQueue != NULL && fQueueEnd != NULL) || (fQueue == NULL && fQueueEnd == NULL));
}


Request *Domain::CurrentRequest()
{
	AutoLock lock(fLocker);
	return fQueue;
}

Request *Domain::RootRequest()
{
	AutoLock lock(fLocker);
	return GetRootRequest(fQueue);
}

void Domain::WaitForEmptyQueue()
{
	AutoLock lock(fLocker);
	while (fQueue != NULL) fEmptyQueueCV.Acquire(fLocker);
}

void Domain::Schedule(Request *req)
{
	AutoLock lock(fLocker);
	Trace("%p.Schedule(", this); WriteRequest(req); Trace(")\n");
	Assert(!(fExiting && fQueue == NULL));
	Assert((fQueue != NULL && fQueueEnd != NULL) || (fQueue == NULL && fQueueEnd == NULL));
	if (SetIn(pendingRequest, req->state) || (SetIn(runningRequest, req->state) && SetIn(pendingRequest, req->state)))
		return; // already scheduled
	if (SetIn(runningRequest, req->state)) {
		Assert(req == fQueue);
		SetIncl(req->state, pendingRequest);
		return;
	}
	Assert(!SetIn(pendingRequest, req->state) && !SetIn(runningRequest, req->state));
	SetExcl(req->state, cancelledRequest); SetExcl(req->state, doneRequest);
	SetIncl(req->state, pendingRequest);
	if (fQueue == NULL)
		fQueue = req;
	else
		fQueueEnd->next = req;
	fQueueEnd = req;
	Assert((fQueue != NULL && fQueueEnd != NULL) || (fQueue == NULL && fQueueEnd == NULL));
	if (req == fQueue)
		Run(req);
}

void Domain::Cancel(Request *req, RequestFlag flag)
{
	AutoLock lock(fLocker);
	Trace("%p.Cancel(%p)\n", this, req);
	Assert(SetIn(pendingRequest, req->state));
	if (SetIn(runningRequest, req->state)) {
		SetExcl(req->state, pendingRequest);
		return;
	}
	Assert(req != fQueue);

	Request *prev = fQueue, *cur = fQueue->next;
	while (cur != NULL && cur != req) {prev = cur; cur = cur->next;}
	Assert(cur != NULL);
	prev->next = req->next;
	if (fQueueEnd == req) fQueueEnd = prev;
	req->next = NULL;

	SetExcl(req->state, pendingRequest);
	SetIncl(req->state, flag);
	if (SetIn(cancelledRequest, req->state))
		req->Resolved();
}


void Domain::AsyncEntry(ThreadPoolItem *pi)
{
	SetDomain(this);
	AsyncRequest *req = dynamic_cast<AsyncRequest*>(fQueue);
	Assert(req->fPtr.GetDomain() == CurrentDomain());
	req->Do(req->fPtr.fPtr);
	SetDomain(NULL);
	gThreadPool.Put(pi);
	Done(req, doneRequest);
}


Request *Domain::GetRootRequest(Request *req)
{
	if (SyncRequest *syncReq = dynamic_cast<SyncRequest*>(req))
		return syncReq->fRoot->fQueue;
	return req;
}

Domain *Domain::GetRoot()
{
	Request *req = fQueue;
	if (auto syncReq = dynamic_cast<SyncRequest*>(req))
		return syncReq->fRoot;
	return this;
}

void Domain::BeginSync()
{
	SyncRequest *req;
	{
		AutoLock lock(fLocker);

		req = dynamic_cast<SyncRequest*>(fQueue);
		Domain *rootDom = NULL;
		bool isRootDom = false;
		if (CurrentDomain() != NULL) {
			rootDom = CurrentDomain()->GetRoot();
		} else {
			rootDom = this;
			isRootDom = true;
		}
		if (req != NULL && !isRootDom && req->fRoot == rootDom) {
			req->fRefCnt++;
			return;
		}
		req = new SyncRequest(rootDom, this);
		if (!isRootDom) {
			req->nextSub = rootDom->fSubrequests; rootDom->fSubrequests = req;
		}
		// Trace("BeginSync: "); WriteSubrequests(rootReq);
		Schedule(req);
	}
	req->fSem.Acquire();
}

void Domain::EndSync()
{
	SyncRequest *req = dynamic_cast<SyncRequest*>(fQueue);
	Assert(req != NULL);
	Domain *rootDom = req->fRoot;
	Assert(req->fRefCnt > 0);
	req->fRefCnt--;
	if (req->fRefCnt == 0) {
		if (req->fDst == rootDom)
			Assert(rootDom->fSubrequests == NULL); // can't finish root request if subrequests are still running
		else {
			// remove subrequest
			Request *prev = NULL, *cur = rootDom->fSubrequests;
			while (cur != NULL && !(cur == req)) {prev = cur; cur = cur->nextSub;}
			Assert(cur != NULL);
			if (prev == NULL) {rootDom->fSubrequests = req->nextSub;} else {prev->nextSub = req->nextSub;}
			req->nextSub = NULL;
		}
		// Trace("EndSync: "); WriteSubrequests(rootReq);
		Done(req, doneRequest);
	}
}

void Domain::BeginSubrequests()
{
	for (SyncRequest *cur = fSubrequests; cur != NULL; cur = cur->nextSub) {
		cur->fDst->Schedule(cur);
	}
	SyncRequest *reversed = NULL;
	while (fSubrequests != NULL) {
		SyncRequest *cur = fSubrequests;
		cur->fSem.Acquire();
		fSubrequests = cur->nextSub;
		cur->nextSub = reversed; reversed = cur;
	}
	fSubrequests = reversed;
	// Trace("Wait(3): "); WriteSubrequests(rootReq);
}

void Domain::EndSubrequests()
{
	// Trace("Wait(1): "); WriteSubrequests(rootReq);
	SyncRequest *reversed = NULL;
	while (fSubrequests != NULL) {
		SyncRequest *cur = fSubrequests;
		cur->fDst->Done(cur, waitingRequest);
		fSubrequests = cur->nextSub;
		cur->nextSub = reversed; reversed = cur;
	}
	fSubrequests = reversed;
	// Trace("Wait(2): "); WriteSubrequests(rootReq);
}

void Domain::Wait()
{
	Request *req = CurrentRequest();
	Request *rootReq = GetRootRequest(req);
	Assert(rootReq != NULL);
	Domain *rootDom = GetRequestDomain(rootReq);
	rootDom->EndSubrequests();
	rootDom->Done(rootReq, waitingRequest);
	rootReq->fSem.Acquire();
	rootDom->BeginSubrequests();
}

void Domain::Yield()
{
	Trace("%p.Yield()\n", this);
	Request *rootReq = GetRootRequest(fQueue);
	GetRequestDomain(rootReq)->Schedule(rootReq);
	Wait();
}


DomainSection::DomainSection(Domain *dom)
{
	fOldDomain = CurrentDomain();
	fDomain.SetTo(dom, false);
	SetDomain(fDomain);
}

DomainSection::~DomainSection()
{
	SetDomain(fOldDomain);
}

void DomainSection::SetTo(Domain *dom)
{
	fDomain.SetTo(dom, false);
	SetDomain(fDomain);
}

void DomainSection::Unset()
{
	fDomain.Unset();
	SetDomain(fOldDomain);
}


//#pragma mark ThreadPool

ThreadPoolItem::ThreadPoolItem():
	fNext(NULL),
	fSem(0),
	fExiting(false),
	fDom(NULL)
{
	fThread = spawn_thread(ThreadEntry, "domain thread", B_NORMAL_PRIORITY, this);
	resume_thread(fThread);
}

ThreadPoolItem::~ThreadPoolItem()
{
	status_t res;
	suspend_thread(fThread);
	fExiting = true;
	fSem.Release();
	wait_for_thread(fThread, &res); fThread = -1;
}

status_t ThreadPoolItem::ThreadEntry(void *arg)
{
	ThreadPoolItem &pi = *(ThreadPoolItem*)arg;
	Trace("+%p.ThreadEntry\n", &pi);
	for (;;) {
		pi.fSem.Acquire();
		if (pi.fExiting) break;
		pi.fDom->AsyncEntry(&pi);
	}
	Trace("-%p.ThreadEntry\n", &pi);
	return B_OK;
}


ThreadPool::ThreadPool():
	fItems(NULL)
{
	Trace("+ThreadPool\n");
	//for (int i = 0; i < 2; i++) Put(new ThreadPoolItem());
}

ThreadPool::~ThreadPool()
{
	Trace("-ThreadPool\n");
	AutoLock lock(fLocker);
	while (fItems != NULL) {
		ThreadPoolItem *next = fItems->fNext;
		delete fItems;
		fItems = next;
	}
}

ThreadPoolItem* ThreadPool::Take(Domain *d)
{
	AutoLock lock(fLocker);
	ThreadPoolItem *pi;
	if (fItems != NULL) {
		pi = fItems; fItems = fItems->fNext;
		pi->fNext = NULL;
	} else {
		pi = new ThreadPoolItem();
	}
	pi->fDom = d;
	return pi;
}

void ThreadPool::Put(ThreadPoolItem* pi)
{
	AutoLock lock(fLocker);
	pi->fDom = NULL;
	pi->fNext = fItems;
	fItems = pi;
}
