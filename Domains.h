#ifndef _DOMAINS_H_
#define _DOMAINS_H_

#include <utility>
#include <functional>
#include <OS.h>
#include <Referenceable.h>

#include "../Locks/Sem.h"
#include "../Locks/Mutex.h"
#include "../Locks/RecursiveLock.h"
#include "../Locks/ConditionVariable.h"


extern RecursiveLock gPrintLock;

void Assert(bool cond);

inline uint32 SetBit(uint32 bit) {return (1 << bit);}
inline bool SetIn(uint32 bit, uint32 set) {return SetBit(bit) & set;}
inline void SetIncl(uint32 &set, uint32 bit) {set |= SetBit(bit);}
inline void SetExcl(uint32 &set, uint32 bit) {set &= ~SetBit(bit);}

class Domain;

template <typename Base> class ExternalPtr;
template <typename Base> class LockedPtr;


enum RequestFlag {
	pendingRequest,
	runningRequest,
	doneRequest,
	cancelledRequest,
	waitingRequest,
};


class Object
{
private:
	BReference<Domain> fDom;

public:
	Object();
	virtual ~Object();
	Domain *GetDomain() {return fDom;}
};


//#pragma mark Domain

Domain *CurrentDomain();

class Request;
class SyncRequest;
class ThreadPoolItem;
class Domain: public BReferenceable
{
private:
	RecursiveLock fLocker;
	Request *fQueue, *fQueueEnd;
	bool fExiting;
	ConditionVariable fEmptyQueueCV;

	static Request *GetRootRequest(Request *req);
	Domain *GetRoot();
	void BeginSubrequests(Request *rootReq);
	void EndSubrequests(Request *rootReq);

public:
	Domain();
	~Domain();

	void Run(Request *_req);
	void Done(Request *req, RequestFlag flag);

	Request *CurrentRequest();
	Request *RootRequest();
	void WaitForEmptyQueue();
	void Schedule(Request *req);
	void Cancel(Request *req, RequestFlag flag = cancelledRequest);

	void AsyncEntry(ThreadPoolItem *pi);

	void BeginSync();
	void EndSync();
	void Wait();
	void Yield();
};

class DomainSection
{
private:
	Domain *fOldDomain;
	BReference<Domain> fDomain;

public:
	DomainSection(Domain *dom);
	~DomainSection();
	void SetTo(Domain *dom);
	void Unset();
};


//#pragma mark Pointers

template <typename Base>
class LockedPtr
{
public:
	~LockedPtr()
	{
		if (fPtr != NULL) {
			sect.Unset();
			fPtr->GetDomain()->EndSync();
		}
	}

	void Unset()
	{
		if (fPtr != NULL) {
			sect.Unset();
			fPtr->GetDomain()->EndSync();
			fPtr = NULL;
		}
	}

	void Delete()
	{
		delete fPtr; fPtr = NULL;
	}

	Base& operator*() const {return *fPtr;}
	Base* operator->() const {return fPtr;}
	operator Base*() const {return fPtr;}

private:
	friend ExternalPtr<Base>;
	Base *fPtr;
	DomainSection sect;

	LockedPtr(Base *other, Domain *dom): fPtr(other), sect(dom) {}
};

template <typename Base>
class ExternalPtr
{
public:
	ExternalPtr(): fPtr(NULL) {}
	template <typename OtherBase>
	ExternalPtr(OtherBase *other): fPtr(other) {}
	template <typename OtherBase>
	ExternalPtr(ExternalPtr<OtherBase> other): fPtr(other.fPtr) {}

	bool operator==(Base *other) const {return fPtr == other;}
	bool operator!=(Base *other) const {return fPtr != other;}
	bool operator==(ExternalPtr<Base> other) const {return fPtr == other.fPtr;}
	bool operator!=(ExternalPtr<Base> other) const {return fPtr != other.fPtr;}
	bool operator==(LockedPtr<Base> other) const {return fPtr == other.fPtr;}
	bool operator!=(LockedPtr<Base> other) const {return fPtr != other.fPtr;}
	void operator=(void *other){fPtr = fPtr;}

	template <typename OtherBase>
	void operator=(ExternalPtr<OtherBase> other) {fPtr = other.fPtr;}
	template <typename OtherBase>
	void operator=(LockedPtr<OtherBase> other){fPtr = other.fPtr;}

	Base *Get() const {return fPtr;}

	Domain *GetDomain() const
	{
		Base *ptr = fPtr;
		if (ptr == NULL)
			return NULL;
		return ptr->GetDomain();
	}

	LockedPtr<Base> Lock() const
	{
		LockedPtr<Base> locked(fPtr, CurrentDomain());
		if (locked.fPtr != NULL)
			locked.fPtr->GetDomain()->BeginSync();
		return locked;
	}

	LockedPtr<Base> Switch() const
	{
		LockedPtr<Base> locked(fPtr, CurrentDomain());
		if (locked.fPtr != NULL) {
			locked.fPtr->GetDomain()->BeginSync();
			locked.sect.SetTo(fPtr->GetDomain());
		}
		return locked;
	}

	void Unset()
	{
		fPtr = NULL;
	}

	void Delete()
	{
		if (fPtr != NULL) {
			Switch().Delete();
			fPtr = NULL;
		}
	}

private:
	template<class OtherBase>
	friend class ExternalPtr;
	friend Domain;
	Base *fPtr;
};

template<typename T, typename... Args>
ExternalPtr<T> MakeExternal(Args&&... args)
{
	BReference<Domain> domain(new Domain(), true);
	DomainSection sect(domain);
	ExternalPtr<T> ptr(new T(std::forward<Args>(args)...));
	return ptr;
}


//#pragma mark Requests

class Request
{
public:
	uint32 state;
	Request *next;
	SyncRequest *nextSub;
	Sem fSem;

	Request();
	virtual ~Request();
	virtual void Resolved();
};

class SyncRequest: public Request
{
public:
	Domain *fRoot;
	Domain *fDst;
	int32 fRefCnt;

	SyncRequest(Domain *root, Domain *dst);
	~SyncRequest();
};

class AsyncRequest: public Request
{
private:
	friend Domain;
	ExternalPtr<Object> fPtr;

public:
	AsyncRequest(ExternalPtr<Object> ptr);

	Domain *TargetDomain() {return fPtr.GetDomain();}
	void Schedule() {TargetDomain()->Schedule(this);}
	void Cancel() {TargetDomain()->Cancel(this);}

	virtual void Do(Object *ptr) = 0;
};


template <typename Type, typename FnType>
class AsyncRequestTmpl: public AsyncRequest
{
private:
	FnType fDoFn;

public:
	AsyncRequestTmpl(ExternalPtr<Type> ptr, FnType doFn):
		AsyncRequest(ExternalPtr<Object>(ptr)),
		fDoFn(doFn)
	{}
	
	void Do(Object *ptr){
		fDoFn((Type*)ptr);
	}
	
};

template <typename Type, typename FnType>
AsyncRequest *MakeAsyncRequest(ExternalPtr<Type> ptr, FnType Do)
{
	return new AsyncRequestTmpl<Type, FnType>(ptr, Do);
}

template <typename Type>
class AsyncRequestMthTmpl: public AsyncRequest
{
private:
	void (Type::*fMethod)();

public:
	AsyncRequestMthTmpl(ExternalPtr<Type> ptr, void (Type::*method)()):
		AsyncRequest(ExternalPtr<Object>(ptr)),
		fMethod(method)
	{}

	void Do(Object *ptr){
		(((Type*)ptr)->*fMethod)();
	}

};

template <typename Type>
AsyncRequest *MakeAsyncRequestMth(ExternalPtr<Type> ptr, void (Type::*Method)())
{
	return new AsyncRequestMthTmpl<Type>(ptr, Method);
}

template<typename Result, typename Type, typename... Args>
Result SyncCallMth(ExternalPtr<Type> ptr, void (Type::*Method)(), Args&&... args)
{
	return (ptr.Switch()->*Method)(std::forward<Args>(args)...);
}

class ThreadPoolItem
{
public:
	ThreadPoolItem *fNext;
	thread_id fThread;
	Sem fSem;
	bool fExiting;
	Domain *fDom;

	ThreadPoolItem();
	~ThreadPoolItem();

	static status_t ThreadEntry(void *arg);
};

class ThreadPool
{
private:
	RecursiveLock fLocker;
	ThreadPoolItem *fItems;

public:
	ThreadPool();
	~ThreadPool();

	ThreadPoolItem* Take(Domain *d);
	void Put(ThreadPoolItem* pi);
};


#endif	// _DOMAINS_H_
