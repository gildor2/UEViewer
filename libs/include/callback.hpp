/*
 * Delegate implementation
 * Article:
 *   http://www.codeproject.com/Articles/136799/Lightweight-Generic-C-Callbacks-or-Yet-Another-Del
 *   Author: Elbert Mai
 * C++11 optimized version taken from
 *   https://github.com/BaroboRobotics/cxx-util/tree/master/include/util
 *   Author: Harris Hancock (https://github.com/harrishancock)
 */

// Copyright (c) 2014-2016 Barobo, Inc.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef UTIL_CALLBACK_HPP
#define UTIL_CALLBACK_HPP

#define UTIL_GET_CALLBACK_FACTORY_BIND_FREE(freeFuncPtr) \
    (util::getCallbackFactory(freeFuncPtr).bind<freeFuncPtr>())
#define BIND_FREE_CB UTIL_GET_CALLBACK_FACTORY_BIND_FREE

#define UTIL_GET_CALLBACK_FACTORY_BIND_MEMBER(memFuncPtr, instancePtr) \
    (util::getCallbackFactory(memFuncPtr).bind<memFuncPtr>(instancePtr))
#define BIND_MEM_CB UTIL_GET_CALLBACK_FACTORY_BIND_MEMBER

namespace util {

template<typename FuncSignature>
class Callback;

struct NullCallback {};

template<typename R, typename... Ps>
class Callback<R (Ps...)>
{
public:
    Callback()                    : func(0), obj(0) {}
    Callback(NullCallback)        : func(0), obj(0) {}
    Callback(const Callback& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~Callback() {}

    Callback& operator=(NullCallback)
        { obj = 0; func = 0; return *this; }
    Callback& operator=(const Callback& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()(Ps... as) const
    {
        return (*func)(obj, as...);
    }

private:
    typedef const void* Callback::*SafeBoolType;
public:
    inline operator SafeBoolType() const
        { return func != 0 ? &Callback::obj : 0; }
    inline bool operator!() const
        { return func == 0; }

private:
    typedef R (*FuncType)(const void*, Ps...);
    Callback(FuncType f, const void* o) : func(f), obj(o) {}

private:
    FuncType func;
    const void* obj;

    template<typename FR, typename... FPs>
    friend class FreeCallbackFactory;
    template<typename FR, class FT, typename... FPs>
    friend class MemberCallbackFactory;
    template<typename FR, class FT, typename... FPs>
    friend class ConstMemberCallbackFactory;
};

template<typename R, typename... Ps>
void operator==(const Callback<R (Ps...)>&,
                const Callback<R (Ps...)>&);
template<typename R, typename... Ps>
void operator!=(const Callback<R (Ps...)>&,
                const Callback<R (Ps...)>&);

template<typename R, typename... Ps>
class FreeCallbackFactory
{
private:
    template<R (*Func)(Ps...)>
    static R wrapper(const void*, Ps... as)
    {
        return (*Func)(as...);
    }

public:
    template<R (*Func)(Ps...)>
    inline static Callback<R (Ps...)> bind()
    {
        return Callback<R (Ps...)>
            (&FreeCallbackFactory::wrapper<Func>, 0);
    }
};

template<typename R, typename... Ps>
inline FreeCallbackFactory<R, Ps...>
getCallbackFactory(R (*)(Ps...))
{
    return FreeCallbackFactory<R, Ps...>();
}

template<typename R, class T, typename... Ps>
class MemberCallbackFactory
{
private:
    template<R (T::*Func)(Ps...)>
    static R wrapper(const void* o, Ps... as)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)(as...);
    }

public:
    template<R (T::*Func)(Ps...)>
    inline static Callback<R (Ps...)> bind(T* o)
    {
        return Callback<R (Ps...)>
            (&MemberCallbackFactory::wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename... Ps>
inline MemberCallbackFactory<R, T, Ps...>
getCallbackFactory(R (T::*)(Ps...))
{
    return MemberCallbackFactory<R, T, Ps...>();
}

template<typename R, class T, typename... Ps>
class ConstMemberCallbackFactory
{
private:
    template<R (T::*Func)(Ps...) const>
    static R wrapper(const void* o, Ps... as)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)(as...);
    }

public:
    template<R (T::*Func)(Ps...) const>
    inline static Callback<R (Ps...)> bind(const T* o)
    {
        return Callback<R (Ps...)>
            (&ConstMemberCallbackFactory::wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T, typename... Ps>
inline ConstMemberCallbackFactory<R, T, Ps...>
getCallbackFactory(R (T::*)(Ps...) const)
{
    return ConstMemberCallbackFactory<R, T, Ps...>();
}

template <class FuncSignature>
class Signal;

template <class... Ps>
class Signal<void(Ps...)> {
public:
    void operator() (Ps... as) const {
        if (mCallback) {
            mCallback(as...);
        }
    }

    void connect (Callback<void(Ps...)> callback) {
        mCallback = callback;
    }

private:
    Callback<void(Ps...)> mCallback;
};

} // namespace util

#endif
