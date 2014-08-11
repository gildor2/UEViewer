// Delegate implementation
// Source:
// http://www.codeproject.com/Articles/136799/Lightweight-Generic-C-Callbacks-or-Yet-Another-Del

#ifndef UTIL_CALLBACK_HPP
#define UTIL_CALLBACK_HPP

#define UTIL_GET_CALLBACK_FACTORY_BIND_FREE(freeFuncPtr) \
    (util::GetCallbackFactory(freeFuncPtr).Bind<freeFuncPtr>())
#define BIND_FREE_CB UTIL_GET_CALLBACK_FACTORY_BIND_FREE

#define UTIL_GET_CALLBACK_FACTORY_BIND_MEMBER(memFuncPtr, instancePtr) \
    (util::GetCallbackFactory(memFuncPtr).Bind<memFuncPtr>(instancePtr))
#define BIND_MEM_CB UTIL_GET_CALLBACK_FACTORY_BIND_MEMBER

namespace util {

template<typename FuncSignature>
class Callback;

struct NullCallback {};

// 0 parameter version

template<typename R>
class Callback<R ()>
{
public:
    static const int Arity = 0;
    typedef R ReturnType;

    Callback()                    : func(0), obj(0) {}
    Callback(NullCallback)        : func(0), obj(0) {}
    Callback(const Callback& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~Callback() {}

    Callback& operator=(NullCallback)
        { obj = 0; func = 0; return *this; }
    Callback& operator=(const Callback& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()() const
    {
        return (*func)(obj);
    }

private:
    typedef const void* Callback::*SafeBoolType;
public:
    inline operator SafeBoolType() const
        { return func != 0 ? &Callback::obj : 0; }
    inline bool operator!() const
        { return func == 0; }

private:
    typedef R (*FuncType)(const void*);
    Callback(FuncType f, const void* o) : func(f), obj(o) {}

private:
    FuncType func;
    const void* obj;

    template<typename FR>
    friend class FreeCallbackFactory0;
    template<typename FR, class FT>
    friend class MemberCallbackFactory0;
    template<typename FR, class FT>
    friend class ConstMemberCallbackFactory0;
};

template<typename R>
void operator==(const Callback<R ()>&,
                const Callback<R ()>&);
template<typename R>
void operator!=(const Callback<R ()>&,
                const Callback<R ()>&);

template<typename R>
class FreeCallbackFactory0
{
private:
    template<R (*Func)()>
    static R Wrapper(const void*)
    {
        return (*Func)();
    }

public:
    template<R (*Func)()>
    inline static Callback<R ()> Bind()
    {
        return Callback<R ()>
            (&FreeCallbackFactory0::Wrapper<Func>, 0);
    }
};

template<typename R>
inline FreeCallbackFactory0<R>
GetCallbackFactory(R (*)())
{
    return FreeCallbackFactory0<R>();
}

template<typename R, class T>
class MemberCallbackFactory0
{
private:
    template<R (T::*Func)()>
    static R Wrapper(const void* o)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)();
    }

public:
    template<R (T::*Func)()>
    inline static Callback<R ()> Bind(T* o)
    {
        return Callback<R ()>
            (&MemberCallbackFactory0::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T>
inline MemberCallbackFactory0<R, T>
GetCallbackFactory(R (T::*)())
{
    return MemberCallbackFactory0<R, T>();
}

template<typename R, class T>
class ConstMemberCallbackFactory0
{
private:
    template<R (T::*Func)() const>
    static R Wrapper(const void* o)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)();
    }

public:
    template<R (T::*Func)() const>
    inline static Callback<R ()> Bind(const T* o)
    {
        return Callback<R ()>
            (&ConstMemberCallbackFactory0::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T>
inline ConstMemberCallbackFactory0<R, T>
GetCallbackFactory(R (T::*)() const)
{
    return ConstMemberCallbackFactory0<R, T>();
}

// 1 parameter version

template<typename R, typename P1>
class Callback<R (P1)>
{
public:
    static const int Arity = 1;
    typedef R ReturnType;
    typedef P1 Param1Type;

    Callback()                    : func(0), obj(0) {}
    Callback(NullCallback)        : func(0), obj(0) {}
    Callback(const Callback& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~Callback() {}

    Callback& operator=(NullCallback)
        { obj = 0; func = 0; return *this; }
    Callback& operator=(const Callback& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()(P1 a1) const
    {
        return (*func)(obj, a1);
    }

private:
    typedef const void* Callback::*SafeBoolType;
public:
    inline operator SafeBoolType() const
        { return func != 0 ? &Callback::obj : 0; }
    inline bool operator!() const
        { return func == 0; }

private:
    typedef R (*FuncType)(const void*, P1);
    Callback(FuncType f, const void* o) : func(f), obj(o) {}

private:
    FuncType func;
    const void* obj;

    template<typename FR, typename FP1>
    friend class FreeCallbackFactory1;
    template<typename FR, class FT, typename FP1>
    friend class MemberCallbackFactory1;
    template<typename FR, class FT, typename FP1>
    friend class ConstMemberCallbackFactory1;
};

template<typename R, typename P1>
void operator==(const Callback<R (P1)>&,
                const Callback<R (P1)>&);
template<typename R, typename P1>
void operator!=(const Callback<R (P1)>&,
                const Callback<R (P1)>&);

template<typename R, typename P1>
class FreeCallbackFactory1
{
private:
    template<R (*Func)(P1)>
    static R Wrapper(const void*, P1 a1)
    {
        return (*Func)(a1);
    }

public:
    template<R (*Func)(P1)>
    inline static Callback<R (P1)> Bind()
    {
        return Callback<R (P1)>
            (&FreeCallbackFactory1::Wrapper<Func>, 0);
    }
};

template<typename R, typename P1>
inline FreeCallbackFactory1<R, P1>
GetCallbackFactory(R (*)(P1))
{
    return FreeCallbackFactory1<R, P1>();
}

template<typename R, class T, typename P1>
class MemberCallbackFactory1
{
private:
    template<R (T::*Func)(P1)>
    static R Wrapper(const void* o, P1 a1)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)(a1);
    }

public:
    template<R (T::*Func)(P1)>
    inline static Callback<R (P1)> Bind(T* o)
    {
        return Callback<R (P1)>
            (&MemberCallbackFactory1::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1>
inline MemberCallbackFactory1<R, T, P1>
GetCallbackFactory(R (T::*)(P1))
{
    return MemberCallbackFactory1<R, T, P1>();
}

template<typename R, class T, typename P1>
class ConstMemberCallbackFactory1
{
private:
    template<R (T::*Func)(P1) const>
    static R Wrapper(const void* o, P1 a1)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)(a1);
    }

public:
    template<R (T::*Func)(P1) const>
    inline static Callback<R (P1)> Bind(const T* o)
    {
        return Callback<R (P1)>
            (&ConstMemberCallbackFactory1::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1>
inline ConstMemberCallbackFactory1<R, T, P1>
GetCallbackFactory(R (T::*)(P1) const)
{
    return ConstMemberCallbackFactory1<R, T, P1>();
}

// 2 parameter version

template<typename R, typename P1, typename P2>
class Callback<R (P1, P2)>
{
public:
    static const int Arity = 2;
    typedef R ReturnType;
    typedef P1 Param1Type;
    typedef P2 Param2Type;

    Callback()                    : func(0), obj(0) {}
    Callback(NullCallback)        : func(0), obj(0) {}
    Callback(const Callback& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~Callback() {}

    Callback& operator=(NullCallback)
        { obj = 0; func = 0; return *this; }
    Callback& operator=(const Callback& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()(P1 a1, P2 a2) const
    {
        return (*func)(obj, a1, a2);
    }

private:
    typedef const void* Callback::*SafeBoolType;
public:
    inline operator SafeBoolType() const
        { return func != 0 ? &Callback::obj : 0; }
    inline bool operator!() const
        { return func == 0; }

private:
    typedef R (*FuncType)(const void*, P1, P2);
    Callback(FuncType f, const void* o) : func(f), obj(o) {}

private:
    FuncType func;
    const void* obj;

    template<typename FR, typename FP1, typename FP2>
    friend class FreeCallbackFactory2;
    template<typename FR, class FT, typename FP1, typename FP2>
    friend class MemberCallbackFactory2;
    template<typename FR, class FT, typename FP1, typename FP2>
    friend class ConstMemberCallbackFactory2;
};

template<typename R, typename P1, typename P2>
void operator==(const Callback<R (P1, P2)>&,
                const Callback<R (P1, P2)>&);
template<typename R, typename P1, typename P2>
void operator!=(const Callback<R (P1, P2)>&,
                const Callback<R (P1, P2)>&);

template<typename R, typename P1, typename P2>
class FreeCallbackFactory2
{
private:
    template<R (*Func)(P1, P2)>
    static R Wrapper(const void*, P1 a1, P2 a2)
    {
        return (*Func)(a1, a2);
    }

public:
    template<R (*Func)(P1, P2)>
    inline static Callback<R (P1, P2)> Bind()
    {
        return Callback<R (P1, P2)>
            (&FreeCallbackFactory2::Wrapper<Func>, 0);
    }
};

template<typename R, typename P1, typename P2>
inline FreeCallbackFactory2<R, P1, P2>
GetCallbackFactory(R (*)(P1, P2))
{
    return FreeCallbackFactory2<R, P1, P2>();
}

template<typename R, class T, typename P1, typename P2>
class MemberCallbackFactory2
{
private:
    template<R (T::*Func)(P1, P2)>
    static R Wrapper(const void* o, P1 a1, P2 a2)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)(a1, a2);
    }

public:
    template<R (T::*Func)(P1, P2)>
    inline static Callback<R (P1, P2)> Bind(T* o)
    {
        return Callback<R (P1, P2)>
            (&MemberCallbackFactory2::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2>
inline MemberCallbackFactory2<R, T, P1, P2>
GetCallbackFactory(R (T::*)(P1, P2))
{
    return MemberCallbackFactory2<R, T, P1, P2>();
}

template<typename R, class T, typename P1, typename P2>
class ConstMemberCallbackFactory2
{
private:
    template<R (T::*Func)(P1, P2) const>
    static R Wrapper(const void* o, P1 a1, P2 a2)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)(a1, a2);
    }

public:
    template<R (T::*Func)(P1, P2) const>
    inline static Callback<R (P1, P2)> Bind(const T* o)
    {
        return Callback<R (P1, P2)>
            (&ConstMemberCallbackFactory2::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2>
inline ConstMemberCallbackFactory2<R, T, P1, P2>
GetCallbackFactory(R (T::*)(P1, P2) const)
{
    return ConstMemberCallbackFactory2<R, T, P1, P2>();
}

// 3 parameter version

template<typename R, typename P1, typename P2, typename P3>
class Callback<R (P1, P2, P3)>
{
public:
    static const int Arity = 3;
    typedef R ReturnType;
    typedef P1 Param1Type;
    typedef P2 Param2Type;
    typedef P3 Param3Type;

    Callback()                    : func(0), obj(0) {}
    Callback(NullCallback)        : func(0), obj(0) {}
    Callback(const Callback& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~Callback() {}

    Callback& operator=(NullCallback)
        { obj = 0; func = 0; return *this; }
    Callback& operator=(const Callback& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()(P1 a1, P2 a2, P3 a3) const
    {
        return (*func)(obj, a1, a2, a3);
    }

private:
    typedef const void* Callback::*SafeBoolType;
public:
    inline operator SafeBoolType() const
        { return func != 0 ? &Callback::obj : 0; }
    inline bool operator!() const
        { return func == 0; }

private:
    typedef R (*FuncType)(const void*, P1, P2, P3);
    Callback(FuncType f, const void* o) : func(f), obj(o) {}

private:
    FuncType func;
    const void* obj;

    template<typename FR, typename FP1, typename FP2, typename FP3>
    friend class FreeCallbackFactory3;
    template<typename FR, class FT, typename FP1, typename FP2, typename FP3>
    friend class MemberCallbackFactory3;
    template<typename FR, class FT, typename FP1, typename FP2, typename FP3>
    friend class ConstMemberCallbackFactory3;
};

template<typename R, typename P1, typename P2, typename P3>
void operator==(const Callback<R (P1, P2, P3)>&,
                const Callback<R (P1, P2, P3)>&);
template<typename R, typename P1, typename P2, typename P3>
void operator!=(const Callback<R (P1, P2, P3)>&,
                const Callback<R (P1, P2, P3)>&);

template<typename R, typename P1, typename P2, typename P3>
class FreeCallbackFactory3
{
private:
    template<R (*Func)(P1, P2, P3)>
    static R Wrapper(const void*, P1 a1, P2 a2, P3 a3)
    {
        return (*Func)(a1, a2, a3);
    }

public:
    template<R (*Func)(P1, P2, P3)>
    inline static Callback<R (P1, P2, P3)> Bind()
    {
        return Callback<R (P1, P2, P3)>
            (&FreeCallbackFactory3::Wrapper<Func>, 0);
    }
};

template<typename R, typename P1, typename P2, typename P3>
inline FreeCallbackFactory3<R, P1, P2, P3>
GetCallbackFactory(R (*)(P1, P2, P3))
{
    return FreeCallbackFactory3<R, P1, P2, P3>();
}

template<typename R, class T, typename P1, typename P2, typename P3>
class MemberCallbackFactory3
{
private:
    template<R (T::*Func)(P1, P2, P3)>
    static R Wrapper(const void* o, P1 a1, P2 a2, P3 a3)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)(a1, a2, a3);
    }

public:
    template<R (T::*Func)(P1, P2, P3)>
    inline static Callback<R (P1, P2, P3)> Bind(T* o)
    {
        return Callback<R (P1, P2, P3)>
            (&MemberCallbackFactory3::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2, typename P3>
inline MemberCallbackFactory3<R, T, P1, P2, P3>
GetCallbackFactory(R (T::*)(P1, P2, P3))
{
    return MemberCallbackFactory3<R, T, P1, P2, P3>();
}

template<typename R, class T, typename P1, typename P2, typename P3>
class ConstMemberCallbackFactory3
{
private:
    template<R (T::*Func)(P1, P2, P3) const>
    static R Wrapper(const void* o, P1 a1, P2 a2, P3 a3)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)(a1, a2, a3);
    }

public:
    template<R (T::*Func)(P1, P2, P3) const>
    inline static Callback<R (P1, P2, P3)> Bind(const T* o)
    {
        return Callback<R (P1, P2, P3)>
            (&ConstMemberCallbackFactory3::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2, typename P3>
inline ConstMemberCallbackFactory3<R, T, P1, P2, P3>
GetCallbackFactory(R (T::*)(P1, P2, P3) const)
{
    return ConstMemberCallbackFactory3<R, T, P1, P2, P3>();
}

// 4 parameter version

template<typename R, typename P1, typename P2, typename P3,
                     typename P4>
class Callback<R (P1, P2, P3, P4)>
{
public:
    static const int Arity = 4;
    typedef R ReturnType;
    typedef P1 Param1Type;
    typedef P2 Param2Type;
    typedef P3 Param3Type;
    typedef P4 Param4Type;

    Callback()                    : func(0), obj(0) {}
    Callback(NullCallback)        : func(0), obj(0) {}
    Callback(const Callback& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~Callback() {}

    Callback& operator=(NullCallback)
        { obj = 0; func = 0; return *this; }
    Callback& operator=(const Callback& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()(P1 a1, P2 a2, P3 a3, P4 a4) const
    {
        return (*func)(obj, a1, a2, a3, a4);
    }

private:
    typedef const void* Callback::*SafeBoolType;
public:
    inline operator SafeBoolType() const
        { return func != 0 ? &Callback::obj : 0; }
    inline bool operator!() const
        { return func == 0; }

private:
    typedef R (*FuncType)(const void*, P1, P2, P3, P4);
    Callback(FuncType f, const void* o) : func(f), obj(o) {}

private:
    FuncType func;
    const void* obj;

    template<typename FR, typename FP1, typename FP2, typename FP3,
                          typename FP4>
    friend class FreeCallbackFactory4;
    template<typename FR, class FT, typename FP1, typename FP2, typename FP3,
                                    typename FP4>
    friend class MemberCallbackFactory4;
    template<typename FR, class FT, typename FP1, typename FP2, typename FP3,
                                    typename FP4>
    friend class ConstMemberCallbackFactory4;
};

template<typename R, typename P1, typename P2, typename P3,
                     typename P4>
void operator==(const Callback<R (P1, P2, P3, P4)>&,
                const Callback<R (P1, P2, P3, P4)>&);
template<typename R, typename P1, typename P2, typename P3,
                     typename P4>
void operator!=(const Callback<R (P1, P2, P3, P4)>&,
                const Callback<R (P1, P2, P3, P4)>&);

template<typename R, typename P1, typename P2, typename P3,
                     typename P4>
class FreeCallbackFactory4
{
private:
    template<R (*Func)(P1, P2, P3, P4)>
    static R Wrapper(const void*, P1 a1, P2 a2, P3 a3, P4 a4)
    {
        return (*Func)(a1, a2, a3, a4);
    }

public:
    template<R (*Func)(P1, P2, P3, P4)>
    inline static Callback<R (P1, P2, P3, P4)> Bind()
    {
        return Callback<R (P1, P2, P3, P4)>
            (&FreeCallbackFactory4::Wrapper<Func>, 0);
    }
};

template<typename R, typename P1, typename P2, typename P3,
                     typename P4>
inline FreeCallbackFactory4<R, P1, P2, P3, P4>
GetCallbackFactory(R (*)(P1, P2, P3, P4))
{
    return FreeCallbackFactory4<R, P1, P2, P3, P4>();
}

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4>
class MemberCallbackFactory4
{
private:
    template<R (T::*Func)(P1, P2, P3, P4)>
    static R Wrapper(const void* o, P1 a1, P2 a2, P3 a3, P4 a4)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)(a1, a2, a3, a4);
    }

public:
    template<R (T::*Func)(P1, P2, P3, P4)>
    inline static Callback<R (P1, P2, P3, P4)> Bind(T* o)
    {
        return Callback<R (P1, P2, P3, P4)>
            (&MemberCallbackFactory4::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4>
inline MemberCallbackFactory4<R, T, P1, P2, P3, P4>
GetCallbackFactory(R (T::*)(P1, P2, P3, P4))
{
    return MemberCallbackFactory4<R, T, P1, P2, P3, P4>();
}

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4>
class ConstMemberCallbackFactory4
{
private:
    template<R (T::*Func)(P1, P2, P3, P4) const>
    static R Wrapper(const void* o, P1 a1, P2 a2, P3 a3, P4 a4)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)(a1, a2, a3, a4);
    }

public:
    template<R (T::*Func)(P1, P2, P3, P4) const>
    inline static Callback<R (P1, P2, P3, P4)> Bind(const T* o)
    {
        return Callback<R (P1, P2, P3, P4)>
            (&ConstMemberCallbackFactory4::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4>
inline ConstMemberCallbackFactory4<R, T, P1, P2, P3, P4>
GetCallbackFactory(R (T::*)(P1, P2, P3, P4) const)
{
    return ConstMemberCallbackFactory4<R, T, P1, P2, P3, P4>();
}

// 5 parameter version

template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5>
class Callback<R (P1, P2, P3, P4, P5)>
{
public:
    static const int Arity = 5;
    typedef R ReturnType;
    typedef P1 Param1Type;
    typedef P2 Param2Type;
    typedef P3 Param3Type;
    typedef P4 Param4Type;
    typedef P5 Param5Type;

    Callback()                    : func(0), obj(0) {}
    Callback(NullCallback)        : func(0), obj(0) {}
    Callback(const Callback& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~Callback() {}

    Callback& operator=(NullCallback)
        { obj = 0; func = 0; return *this; }
    Callback& operator=(const Callback& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()(P1 a1, P2 a2, P3 a3, P4 a4, P5 a5) const
    {
        return (*func)(obj, a1, a2, a3, a4, a5);
    }

private:
    typedef const void* Callback::*SafeBoolType;
public:
    inline operator SafeBoolType() const
        { return func != 0 ? &Callback::obj : 0; }
    inline bool operator!() const
        { return func == 0; }

private:
    typedef R (*FuncType)(const void*, P1, P2, P3, P4, P5);
    Callback(FuncType f, const void* o) : func(f), obj(o) {}

private:
    FuncType func;
    const void* obj;

    template<typename FR, typename FP1, typename FP2, typename FP3,
                          typename FP4, typename FP5>
    friend class FreeCallbackFactory5;
    template<typename FR, class FT, typename FP1, typename FP2, typename FP3,
                                    typename FP4, typename FP5>
    friend class MemberCallbackFactory5;
    template<typename FR, class FT, typename FP1, typename FP2, typename FP3,
                                    typename FP4, typename FP5>
    friend class ConstMemberCallbackFactory5;
};

template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5>
void operator==(const Callback<R (P1, P2, P3, P4, P5)>&,
                const Callback<R (P1, P2, P3, P4, P5)>&);
template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5>
void operator!=(const Callback<R (P1, P2, P3, P4, P5)>&,
                const Callback<R (P1, P2, P3, P4, P5)>&);

template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5>
class FreeCallbackFactory5
{
private:
    template<R (*Func)(P1, P2, P3, P4, P5)>
    static R Wrapper(const void*, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5)
    {
        return (*Func)(a1, a2, a3, a4, a5);
    }

public:
    template<R (*Func)(P1, P2, P3, P4, P5)>
    inline static Callback<R (P1, P2, P3, P4, P5)> Bind()
    {
        return Callback<R (P1, P2, P3, P4, P5)>
            (&FreeCallbackFactory5::Wrapper<Func>, 0);
    }
};

template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5>
inline FreeCallbackFactory5<R, P1, P2, P3, P4, P5>
GetCallbackFactory(R (*)(P1, P2, P3, P4, P5))
{
    return FreeCallbackFactory5<R, P1, P2, P3, P4, P5>();
}

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4, typename P5>
class MemberCallbackFactory5
{
private:
    template<R (T::*Func)(P1, P2, P3, P4, P5)>
    static R Wrapper(const void* o, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)(a1, a2, a3, a4, a5);
    }

public:
    template<R (T::*Func)(P1, P2, P3, P4, P5)>
    inline static Callback<R (P1, P2, P3, P4, P5)> Bind(T* o)
    {
        return Callback<R (P1, P2, P3, P4, P5)>
            (&MemberCallbackFactory5::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4, typename P5>
inline MemberCallbackFactory5<R, T, P1, P2, P3, P4, P5>
GetCallbackFactory(R (T::*)(P1, P2, P3, P4, P5))
{
    return MemberCallbackFactory5<R, T, P1, P2, P3, P4, P5>();
}

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4, typename P5>
class ConstMemberCallbackFactory5
{
private:
    template<R (T::*Func)(P1, P2, P3, P4, P5) const>
    static R Wrapper(const void* o, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)(a1, a2, a3, a4, a5);
    }

public:
    template<R (T::*Func)(P1, P2, P3, P4, P5) const>
    inline static Callback<R (P1, P2, P3, P4, P5)> Bind(const T* o)
    {
        return Callback<R (P1, P2, P3, P4, P5)>
            (&ConstMemberCallbackFactory5::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4, typename P5>
inline ConstMemberCallbackFactory5<R, T, P1, P2, P3, P4, P5>
GetCallbackFactory(R (T::*)(P1, P2, P3, P4, P5) const)
{
    return ConstMemberCallbackFactory5<R, T, P1, P2, P3, P4, P5>();
}

// 6 parameter version

template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5, typename P6>
class Callback<R (P1, P2, P3, P4, P5, P6)>
{
public:
    static const int Arity = 6;
    typedef R ReturnType;
    typedef P1 Param1Type;
    typedef P2 Param2Type;
    typedef P3 Param3Type;
    typedef P4 Param4Type;
    typedef P5 Param5Type;
    typedef P6 Param6Type;

    Callback()                    : func(0), obj(0) {}
    Callback(NullCallback)        : func(0), obj(0) {}
    Callback(const Callback& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~Callback() {}

    Callback& operator=(NullCallback)
        { obj = 0; func = 0; return *this; }
    Callback& operator=(const Callback& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()(P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6) const
    {
        return (*func)(obj, a1, a2, a3, a4, a5, a6);
    }

private:
    typedef const void* Callback::*SafeBoolType;
public:
    inline operator SafeBoolType() const
        { return func != 0 ? &Callback::obj : 0; }
    inline bool operator!() const
        { return func == 0; }

private:
    typedef R (*FuncType)(const void*, P1, P2, P3, P4, P5, P6);
    Callback(FuncType f, const void* o) : func(f), obj(o) {}

private:
    FuncType func;
    const void* obj;

    template<typename FR, typename FP1, typename FP2, typename FP3,
                          typename FP4, typename FP5, typename FP6>
    friend class FreeCallbackFactory6;
    template<typename FR, class FT, typename FP1, typename FP2, typename FP3,
                                    typename FP4, typename FP5, typename FP6>
    friend class MemberCallbackFactory6;
    template<typename FR, class FT, typename FP1, typename FP2, typename FP3,
                                    typename FP4, typename FP5, typename FP6>
    friend class ConstMemberCallbackFactory6;
};

template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5, typename P6>
void operator==(const Callback<R (P1, P2, P3, P4, P5, P6)>&,
                const Callback<R (P1, P2, P3, P4, P5, P6)>&);
template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5, typename P6>
void operator!=(const Callback<R (P1, P2, P3, P4, P5, P6)>&,
                const Callback<R (P1, P2, P3, P4, P5, P6)>&);

template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5, typename P6>
class FreeCallbackFactory6
{
private:
    template<R (*Func)(P1, P2, P3, P4, P5, P6)>
    static R Wrapper(const void*, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6)
    {
        return (*Func)(a1, a2, a3, a4, a5, a6);
    }

public:
    template<R (*Func)(P1, P2, P3, P4, P5, P6)>
    inline static Callback<R (P1, P2, P3, P4, P5, P6)> Bind()
    {
        return Callback<R (P1, P2, P3, P4, P5, P6)>
            (&FreeCallbackFactory6::Wrapper<Func>, 0);
    }
};

template<typename R, typename P1, typename P2, typename P3,
                     typename P4, typename P5, typename P6>
inline FreeCallbackFactory6<R, P1, P2, P3, P4, P5, P6>
GetCallbackFactory(R (*)(P1, P2, P3, P4, P5, P6))
{
    return FreeCallbackFactory6<R, P1, P2, P3, P4, P5, P6>();
}

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4, typename P5, typename P6>
class MemberCallbackFactory6
{
private:
    template<R (T::*Func)(P1, P2, P3, P4, P5, P6)>
    static R Wrapper(const void* o, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)(a1, a2, a3, a4, a5, a6);
    }

public:
    template<R (T::*Func)(P1, P2, P3, P4, P5, P6)>
    inline static Callback<R (P1, P2, P3, P4, P5, P6)> Bind(T* o)
    {
        return Callback<R (P1, P2, P3, P4, P5, P6)>
            (&MemberCallbackFactory6::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4, typename P5, typename P6>
inline MemberCallbackFactory6<R, T, P1, P2, P3, P4, P5, P6>
GetCallbackFactory(R (T::*)(P1, P2, P3, P4, P5, P6))
{
    return MemberCallbackFactory6<R, T, P1, P2, P3, P4, P5, P6>();
}

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4, typename P5, typename P6>
class ConstMemberCallbackFactory6
{
private:
    template<R (T::*Func)(P1, P2, P3, P4, P5, P6) const>
    static R Wrapper(const void* o, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)(a1, a2, a3, a4, a5, a6);
    }

public:
    template<R (T::*Func)(P1, P2, P3, P4, P5, P6) const>
    inline static Callback<R (P1, P2, P3, P4, P5, P6)> Bind(const T* o)
    {
        return Callback<R (P1, P2, P3, P4, P5, P6)>
            (&ConstMemberCallbackFactory6::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename P1, typename P2, typename P3,
                              typename P4, typename P5, typename P6>
inline ConstMemberCallbackFactory6<R, T, P1, P2, P3, P4, P5, P6>
GetCallbackFactory(R (T::*)(P1, P2, P3, P4, P5, P6) const)
{
    return ConstMemberCallbackFactory6<R, T, P1, P2, P3, P4, P5, P6>();
}

}

#endif
