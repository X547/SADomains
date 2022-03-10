#include "Domains.h"

#include <stdio.h>
#include "Locks/AutoLock.h"

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

Domain *CurrentRootDomain()
{
	Domain *curDomain = CurrentDomain();
	if (curDomain == NULL) {
		return curDomain;
	}
	return curDomain->GetRoot();
}

static inline void SetDomain(Domain *d)
{
	gCurDomain = d;
	Trace("SetDomain(%p), thread: %" B_PRId32 "\n", d, find_thread(NULL));
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
{
	Assert(!SetIn(pendingRequest, state) && !SetIn(runningRequest, state));
}

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

AsyncRequest::~AsyncRequest()
{}


SyncRequest::SyncRequest(Domain *root, Domain *dst):
	fRoot(root),
	fDst(dst),
	fRefCnt(1)
{
}

SyncRequest::~SyncRequest()
{}


Domain *GetRequestDomain(Request *_req)
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
	} else {
		char *name = CppDemangle(typeid(*_req).name());
		fprintf(stderr, "[!] unknown request type: %s\n", name);
		Assert(false);
	}
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
	uint32 oldState = req->state;
	if (SetIn(doneRequest, oldState) && !SetIn(pendingRequest, oldState)) {
		req->Resolved(); req = NULL;
	}
	if (next == NULL) {
		if (SetIn(pendingRequest, oldState)) {
			SetExcl(req->state, pendingRequest);
			Schedule(req); // calls Run
		} else {
			Trace("%p: last request\n", this);
			fEmptyQueueCV.Release(true);
		}
	} else {
		if (SetIn(pendingRequest, oldState)) {
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
		SyncRequest *runReq = dynamic_cast<SyncRequest*>(fQueue);
		Domain *rootDom = CurrentRootDomain();
		if (runReq != NULL && runReq->fRoot == rootDom) {
			runReq->fRefCnt++;
			return;
		}
		if (rootDom == NULL) {
			req = new SyncRequest(this, this);
		} else {
			req = new SyncRequest(rootDom, this);
			Request *rootReq = rootDom->RootRequest();
			req->nextSub = rootReq->nextSub; rootReq->nextSub = req;
		}
		//Trace("BeginSync: "); WriteSubrequests(req->fRoot);
		Schedule(req);
	}
	req->fSem.Acquire();
}

void Domain::EndSync()
{
	AutoLock lock(fLocker);
	Request *curReq = fQueue;
	SyncRequest *req = dynamic_cast<SyncRequest*>(curReq);
	AsyncRequest *asyncReq = dynamic_cast<AsyncRequest*>(curReq);
	Assert(req != NULL);
	Assert(asyncReq == NULL);
	Domain *rootDom = req->fRoot;
	Request *rootReq = RootRequest();
	Assert(req->fRefCnt > 0);
	req->fRefCnt--;
	if (req->fRefCnt == 0) {
		if (req->fDst == rootDom)
			Assert(rootReq->nextSub == NULL); // can't finish root request if subrequests are still running
		else {
			// remove subrequest
			Request *prev = NULL, *cur = rootReq->nextSub;
			while (cur != NULL && !(cur == req)) {prev = cur; cur = cur->nextSub;}
			Assert(cur != NULL);
			if (prev == NULL) {rootReq->nextSub = req->nextSub;} else {prev->nextSub = req->nextSub;}
			req->nextSub = NULL;
		}
		//Trace("EndSync: "); WriteSubrequests(rootReq);
		Done(req, doneRequest);
	}
}

void Domain::BeginSubrequests(Request *rootReq)
{
	for (SyncRequest *cur = rootReq->nextSub; cur != NULL; cur = cur->nextSub) {
		cur->fDst->Schedule(cur);
	}
	SyncRequest *reversed = NULL;
	while (rootReq->nextSub != NULL) {
		SyncRequest *cur = rootReq->nextSub;
		cur->fSem.Acquire();
		rootReq->nextSub = cur->nextSub;
		cur->nextSub = reversed; reversed = cur;
	}
	rootReq->nextSub = reversed;
	//Trace("Wait(3): "); WriteSubrequests(rootReq);
}

void Domain::EndSubrequests(Request *rootReq)
{
	//Trace("Wait(1): "); WriteSubrequests(rootReq);
	SyncRequest *reversed = NULL;
	while (rootReq->nextSub != NULL) {
		SyncRequest *cur = rootReq->nextSub;
		cur->fDst->Done(cur, waitingRequest);
		rootReq->nextSub = cur->nextSub;
		cur->nextSub = reversed; reversed = cur;
	}
	rootReq->nextSub = reversed;
	//Trace("Wait(2): "); WriteSubrequests(rootReq);
}

void Domain::Wait()
{
	Domain *rootDom = CurrentRootDomain();
	Assert(rootDom != NULL);
	Request *rootReq = rootDom->CurrentRequest();
	rootDom->EndSubrequests(rootReq);
	rootDom->Done(rootReq, waitingRequest);
	rootReq->fSem.Acquire();
	rootDom->BeginSubrequests(rootReq);
}

void Domain::Yield()
{
	Domain *rootDom = CurrentRootDomain();
	Assert(rootDom != NULL);
	Trace("%p.Yield()\n", rootDom);
	Request *rootReq = rootDom->CurrentRequest();
	rootDom->Schedule(rootReq);
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
