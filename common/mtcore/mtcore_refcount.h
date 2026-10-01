// mtcore_refcount.h                                                  -*-C++-*-
#ifndef INCLUDED_MTCORE_REFCOUNT
#define INCLUDED_MTCORE_REFCOUNT

//@PURPOSE: Provide intrusive reference counting and a 'Handle' smart pointer.
//
//@CLASSES:
//  mtcore::RefCounted:  base class with an interlocked reference count
//  mtcore::Handle<T>:   intrusive owning pointer to a 'RefCounted' type
//
//@DESCRIPTION: Objects derived from 'RefCounted' start with a count of zero
// and are deleted when the last 'Handle' releases them.  Threads that need to
// keep an object alive while running call 'acquire' / 'release' directly.

#include "mtcore_platform.h"

namespace mtcore {

class RefCounted {
    mutable volatile LONG d_refs;

  private:
    // NOT IMPLEMENTED
    RefCounted(const RefCounted&);
    RefCounted& operator=(const RefCounted&);

  protected:
    RefCounted() : d_refs(0) {}
    virtual ~RefCounted() {}

  public:
    void acquire() const
    {
        ::InterlockedIncrement(&d_refs);
    }

    void release() const
    {
        if (::InterlockedDecrement(&d_refs) == 0) {
            delete this;
        }
    }

    int refCount() const
    {
        return (int)d_refs;
    }
};

template <class TYPE>
class Handle {
    TYPE *d_ptr_p;

  public:
    Handle() : d_ptr_p(NULL) {}

    Handle(TYPE *ptr) : d_ptr_p(ptr)
    {
        if (d_ptr_p) {
            d_ptr_p->acquire();
        }
    }

    Handle(const Handle& other) : d_ptr_p(other.d_ptr_p)
    {
        if (d_ptr_p) {
            d_ptr_p->acquire();
        }
    }

    ~Handle()
    {
        if (d_ptr_p) {
            d_ptr_p->release();
        }
    }

    Handle& operator=(const Handle& rhs)
    {
        reset(rhs.d_ptr_p);
        return *this;
    }

    void reset(TYPE *ptr = NULL)
    {
        if (ptr) {
            ptr->acquire();
        }
        TYPE *old = d_ptr_p;
        d_ptr_p = ptr;
        if (old) {
            old->release();
        }
    }

    TYPE *get() const { return d_ptr_p; }
    TYPE *operator->() const { return d_ptr_p; }
    TYPE& operator*() const { return *d_ptr_p; }
    bool isNull() const { return d_ptr_p == NULL; }

    bool operator==(const Handle& rhs) const { return d_ptr_p == rhs.d_ptr_p; }
    bool operator!=(const Handle& rhs) const { return d_ptr_p != rhs.d_ptr_p; }
    bool operator<(const Handle& rhs) const { return d_ptr_p < rhs.d_ptr_p; }
};

}  // close namespace mtcore

#endif
