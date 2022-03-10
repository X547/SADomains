#ifndef _DOMAINS_H_
#define _DOMAINS_H_

#include <utility>
#include <functional>
#include <OS.h>
#include <Referenceable.h>

#include "Locks/Sem.h"
#include "Locks/Mutex.h"
#include "Locks/RecursiveLock.h"
#include "Locks/ConditionVariable.h"


extern RecursiveLock gPrintLock;

void Assert(bool cond);

static inline uint32 SetBit(uint32 bit) {return (1 << bit);}
static inline bool SetIn(uint32 bit, uint32 set) {return SetBit(bit) & set;}
static inline void SetIncl(uint32 &set, uint32 bit) {set |= SetBit(bit);}
static inline void SetExcl(uint32 &set, uint32 bit) {set &= ~SetBit(bit);}

class Domain;

template <typename Base> class ExternalPtrBase;
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

class Request;
class SyncRequest;
class ThreadPoolItem;

Domain *CurrentDomain();
Domain *CurrentRootDomain();

class Domain: public BReferenceable
{
private:
	friend Domain *CurrentRootDomain();

	RecursiveLock fLocker;
	Request *fQueue, *fQueueEnd;
	bool fExiting;
	ConditionVariable fEmptyQueueCV;

	static Request *GetRootRequest(Request *req);
	Domain *GetRoot();
	void Run(Request *_req);
	void Done(Request *req, RequestFlag flag);
	void BeginSubrequests(Request *rootReq);
	void EndSubrequests(Request *rootReq);

public:
	Domain();
	~Domain();

	Request *CurrentRequest();
	Request *RootRequest();
	void WaitForEmptyQueue();
	void Schedule(Request *req);
	void Cancel(Request *req, RequestFlag flag = cancelledRequest);

	void AsyncEntry(ThreadPoolItem *pi);

	void BeginSync();
	void EndSync();
	static void Wait();
	static void Yield();
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

template <typename Base = Object>
class LockedPtr
{
private:
	friend ExternalPtrBase<Base>;
	Base *fPtr;
	DomainSection sect;

	LockedPtr(Base *other, Domain *dom): fPtr(other), sect(dom) {}

public:
	~LockedPtr()
	{
		if (fPtr == NULL) return;
		sect.Unset();
		fPtr->GetDomain()->EndSync();
	}

	void Unset()
	{
		if (fPtr == NULL) return;
		sect.Unset();
		fPtr->GetDomain()->EndSync();
		fPtr = NULL;
	}

	void Delete()
	{
		if (fPtr == NULL) return;
		BReference<Domain> dom(fPtr->GetDomain(), false);
		delete fPtr; fPtr = NULL;
		sect.Unset();
		dom->EndSync();
	}

	Base& operator*() const {return *fPtr;}
	Base* operator->() const {return fPtr;}
	operator Base*() const {return fPtr;}
};

template <typename Base = Object>
class ExternalPtrBase
{
protected:
	friend Domain;
	template<class OtherBase> friend class ExternalPtr;
	template<class OtherBase> friend class ExternalRef;
	template<class OtherBase> friend class ExternalUniquePtr;
	Base *fPtr;

public:
	ExternalPtrBase(Base *other): fPtr(other) {}

	Domain *GetDomain() const
	{
		return fPtr == NULL ? NULL : fPtr->GetDomain();
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
};

template <typename Base = Object>
class ExternalPtr: public ExternalPtrBase<Base>
{
public:
	ExternalPtr(): ExternalPtrBase<Base>(NULL) {}
	ExternalPtr(Base *other): ExternalPtrBase<Base>(other) {}
	ExternalPtr(const ExternalPtrBase<Base>& other): ExternalPtrBase<Base>(other.fPtr) {}
    ExternalPtr(const ExternalPtr<Base>& other): ExternalPtrBase<Base>(other.fPtr) {}
    template<class OtherBase>
	ExternalPtr(OtherBase *other): ExternalPtrBase<Base>(other) {}
	template<class OtherBase>
	ExternalPtr(const ExternalPtrBase<OtherBase>& other): ExternalPtrBase<Base>(other.fPtr) {}

	bool operator==(Base *other) const {return this->fPtr == other;}
	bool operator!=(Base *other) const {return this->fPtr != other;}
	bool operator==(ExternalPtr<Base> other) const {return this->fPtr == other.fPtr;}
	bool operator!=(ExternalPtr<Base> other) const {return this->fPtr != other.fPtr;}
	bool operator==(LockedPtr<Base> other) const {return this->fPtr == other.fPtr;}
	bool operator!=(LockedPtr<Base> other) const {return this->fPtr != other.fPtr;}

	void operator=(ExternalPtr<Base> other) {this->fPtr = other.fPtr;}
	void operator=(LockedPtr<Base> other){this->fPtr = other.fPtr;}

	Base *Get() const {return this->fPtr;}

	void Unset()
	{
		this->fPtr = NULL;
	}

	void Delete()
	{
		if (this->fPtr == NULL) return;
		this->Switch().Delete();
		this->fPtr = NULL;
	}
};

template<typename T, typename... Args>
ExternalPtr<T> MakeExternal(Args&&... args)
{
	BReference<Domain> domain(new Domain(), true);
	domain->BeginSync();
	DomainSection sect(domain);
	ExternalPtr<T> ptr(new T(std::forward<Args>(args)...));
	sect.Unset();
	domain->EndSync();
	return ptr;
}


//#pragma mark Requests

class Request
{
protected:
	friend class Domain;
	uint32 state;
	Request *next;
	SyncRequest *nextSub;
	Sem fSem;

	void WriteRequest();
	void WriteSubrequests();

	virtual void Run(Domain *dom) = 0;

public:
	Request();
	virtual ~Request();
	virtual Domain *TargetDomain() = 0;
	virtual void Resolved();
};

class SyncRequest: public Request
{
private:
	friend class Domain;
	Domain *fRoot;
	Domain *fDst;
	int32 fRefCnt;

	SyncRequest(Domain *root, Domain *dst);
	void Run(Domain *dom) override;

public:
	virtual ~SyncRequest();

	Domain *TargetDomain() final {return fDst;}
};

class AsyncRequest: public Request
{
private:
	friend class Domain;
	ExternalPtr<Object> fPtr;

	void Run(Domain *dom) override;

public:
	AsyncRequest(ExternalPtr<Object> ptr);
	virtual ~AsyncRequest();

	Domain *TargetDomain() final {return fPtr.GetDomain();}
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
		AsyncRequest(ptr),
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


#include "DomainPointers.h"

#endif	// _DOMAINS_H_
