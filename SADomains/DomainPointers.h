#pragma once


class RefObject: public Object, public BReferenceable
{
public:
	virtual ~RefObject() {}

	void LastReferenceReleased() override
	{
		ExternalPtr<RefObject>(this).Switch().Delete();
	}

};


template<typename Type = RefObject>
class ExternalRef: public ExternalPtrBase<Type> {
public:
	ExternalRef(): ExternalPtrBase<Type>(NULL) {}
	ExternalRef(Type* ptr, bool alreadyHasReference = false): ExternalPtrBase<Type>(NULL) {SetTo(ptr, alreadyHasReference);}
	ExternalRef(const ExternalPtr<Type>& other, bool alreadyHasReference = false): ExternalPtrBase<Type>(NULL) {SetTo(other.fPtr, alreadyHasReference);}
	template<typename OtherType>
	ExternalRef(const ExternalPtr<OtherType>& other, bool alreadyHasReference = false): ExternalPtrBase<Type>(NULL) {SetTo(other.fPtr, alreadyHasReference);}
	ExternalRef(const ExternalRef<Type>& other): ExternalPtrBase<Type>(NULL) {SetTo(other.fPtr);}
	template<typename OtherType>
	ExternalRef(const ExternalRef<OtherType>& other): ExternalPtrBase<Type>(NULL) {SetTo(other.fPtr);}

	~ExternalRef() {Unset();}

	operator ExternalPtr<Type>() {return ExternalPtr<Type>(this->fPtr);}

	void SetTo(Type* ptr, bool alreadyHasReference = false)
	{
		if (ptr != NULL && !alreadyHasReference)
			ptr->AcquireReference();
		Unset();
		this->fPtr = ptr;
	}

	void Unset()
	{
		if (!IsSet()) return;
		this->fPtr->ReleaseReference();
		this->fPtr = NULL;
	}

	bool IsSet() const {return this->fPtr != NULL;}

	ExternalRef& operator=(const ExternalRef<Type>& other) {SetTo(other.fPtr); return *this;}
	ExternalRef& operator=(Type* other) {SetTo(other); return *this;}
	template<typename OtherType>
	ExternalRef& operator=(const ExternalRef<OtherType>& other) {SetTo(other.fPtr); return *this;}

	bool operator==(const ExternalRef<Type>& other) const {return this->fPtr == other.fPtr;}
	bool operator==(const Type* other) const {return this->fPtr == other;}
	bool operator!=(const ExternalRef<Type>& other) const {return this->fPtr != other.fPtr;}
	bool operator!=(const Type* other) const {return this->fPtr != other;}
};


template <typename Base = Object>
class ExternalUniquePtr: public ExternalPtrBase<Base>
{
public:
	ExternalUniquePtr(): ExternalPtrBase<Base>(NULL) {}
	ExternalUniquePtr(Base *other): ExternalPtrBase<Base>(other) {}
	ExternalUniquePtr(const ExternalPtrBase<Base>& other): ExternalPtrBase<Base>(other.fPtr) {}
	template<class OtherBase>
	ExternalUniquePtr(OtherBase *other): ExternalPtrBase<Base>(other) {}
	template<class OtherBase>
	ExternalUniquePtr(const ExternalPtrBase<OtherBase>& other): ExternalPtrBase<Base>(other.fPtr) {}
	~ExternalUniquePtr() {Unset();}

	bool operator==(Base *other) const {return this->fPtr == other;}
	bool operator!=(Base *other) const {return this->fPtr != other;}
	bool operator==(ExternalUniquePtr<Base> other) const {return this->fPtr == other.fPtr;}
	bool operator!=(ExternalUniquePtr<Base> other) const {return this->fPtr != other.fPtr;}

	void operator=(ExternalPtr<Base> other) {this->fPtr = other.fPtr;}
	void operator=(LockedPtr<Base> other){this->fPtr = other.fPtr;}

	ExternalPtr<Base> Detach()
	{
		Base *ptr = this->fPtr;
		this->fPtr = NULL;
		return ptr;
	}

	void SetTo(Base *other)
	{
		if (this->fPtr == other) return;
		Unset();
		this->fPtr = other;
	}

	template<class OtherBase>
	void SetTo(const ExternalPtrBase<OtherBase>& other)
	{
		if (this->fPtr == other.fPtr) return;
		Unset();
		this->fPtr = other.fPtr;
	}

	bool IsSet()
	{
		return this->fPtr != NULL;
	}

	void Unset()
	{
		if (!IsSet()) return;
		this->Switch().Delete();
		this->fPtr = NULL;
	}

};
