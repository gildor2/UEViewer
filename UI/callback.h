// Callback (delegate) implementation.
// Copyright (C) 2022 Konstantin Nosov.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/*

High performance easy to use C++11 callback.

Features:
- binding of C or static C++ functions
- binding of C++ class methods, execution is resolved at compile time (i.e. no T::*func is used at runtime)
- binding capturing and non-capturing lambdas
- binding of functor classes (classes with operator()) through BIND_LAMBDA macro
- universal class which can accept all kinds of C and C++ functions: Callback<bool(int)> can accept
  "bool Func(int)", "bool Class::Func(int)", "[capture](int) -> bool { body }"
- possibility to bind a function without parameters to a callback with parameters, i.e. Callback<bool(int)>
  will also accept "bool Func()", "bool Class::Func()", "[capture]() -> bool { body }"
- storing capturing lambda content inside a callback, so it still can be used when initial lambda object has
  been already destroyed
- using inline buffer for lambda storage to avoid memory allocations when lambda capture list is small
- easy to use interface:
  - declaration of a callback:
    Callback<Ret(params...)> callback
  - binding functions:
    callback = BIND_MEMBER(&Class::Method, obj)
    callback = BIND_STATIC(static_func)
    callback = BIND_LAMBDA([capture](params...) -> ret { body })
- very efficient callback assignment and call operations, using "move" semantic whenever possible
- no dependencies on compiler or external libraries, except for malloc/free/memcpy

The callback requires a compiler with full C++11 support. Supported compilers:
- Visual C++ 2013 and newer
- gcc 4.7 and newer (older version doesn't have C++11 support)
- clang 3.1 and newer

Gcc and clang requires option -std=c++11 (it is enabled by default since gcc 6 and clang 6).

Known compiler limitation: Visual C++ 2013 and clang 3.1 doesn't have "move" semantic for lambdas, so
BIND_LAMBDA will copy lambda instead of doing more efficient move. Newer compilers doesn't have this issue.

*/

#include <stdlib.h> // malloc/free
#include <memory.h> // memcpy

#ifndef FORCEINLINE
	#if _MSC_VER
		#define FORCEINLINE __forceinline
	#else
		#define FORCEINLINE inline __attribute__((always_inline))
	#endif
#endif // FORCEINLINE

namespace Detail
{
	// Some template classes, custom alternatives to STL implementation.
	// Code is copy-pasted from UE4 sources, however it doesn't contain anything
	// unique and proprietary, just re-implementation of STL functionality with
	// different coding style.

	// TEnableIf - std::enable_if alternative
	template <bool Predicate, typename Result = void>
	class TEnableIf;

	template <typename Result>
	class TEnableIf<true, Result>
	{
	public:
		typedef Result Type;
	};

	template <typename Result>
	class TEnableIf<false, Result>
	{};

	// TRemoveReference
	template <typename T> struct TRemoveReference      { typedef T Type; };
	template <typename T> struct TRemoveReference<T& > { typedef T Type; };
	template <typename T> struct TRemoveReference<T&&> { typedef T Type; };

	// TRemoveCV
	template <typename T> struct TRemoveCV                   { typedef T Type; };
	template <typename T> struct TRemoveCV<const T>          { typedef T Type; };
	template <typename T> struct TRemoveCV<volatile T>       { typedef T Type; };
	template <typename T> struct TRemoveCV<const volatile T> { typedef T Type; };

	// TDecay
	template <typename T>                 struct TDecayNonReference       { typedef typename TRemoveCV<T>::Type Type; };
	template <typename T>                 struct TDecayNonReference<T[]>  { typedef T* Type; };
	template <typename T, unsigned int N> struct TDecayNonReference<T[N]> { typedef T* Type; };

	template <typename T>
	struct TDecay
	{
		typedef typename TDecayNonReference<typename TRemoveReference<T>::Type>::Type Type;
	};

	template <typename RetType, typename... Params>
	struct TDecayNonReference<RetType(Params...)>
	{
		typedef RetType (*Type)(Params...);
	};

	// Forward - std::forward alternative
	template <typename T>
	FORCEINLINE T&& Forward(typename TRemoveReference<T>::Type& Obj)
	{
		return (T&&)Obj;
	}

	template <typename T>
	FORCEINLINE T&& Forward(typename TRemoveReference<T>::Type&& Obj)
	{
		return (T&&)Obj;
	}

	// MoveTemp - std::move alternative
	template<typename T> struct TIsLValueReferenceType     { enum { Value = false }; };
	template<typename T> struct TIsLValueReferenceType<T&> { enum { Value = true  }; };

	template<typename A,typename B>	struct TAreTypesEqual;
	template<typename,typename>		struct TAreTypesEqual      { enum { Value = false }; };
	template<typename A>			struct TAreTypesEqual<A,A> { enum { Value = true  }; };

	template<typename T>
	FORCEINLINE typename TRemoveReference<T>::Type&& MoveTemp(T&& Obj)
	{
		typedef typename TRemoveReference<T>::Type CastType;

		// Validate that we're not being passed an rvalue or a const object - the former is redundant, the latter is almost certainly a mistake
		static_assert(TIsLValueReferenceType<T>::Value, "MoveTemp called on an rvalue");
		static_assert(!TAreTypesEqual<CastType&, const CastType&>::Value, "MoveTemp called on a const object");

		return (CastType&&)Obj;
	}

} // namespace Detail



namespace Detail
{
	struct ObjectData;

	struct IOwnedObject
	{
		virtual void Destroy(void* object) const = 0;
		virtual void CopyTo(ObjectData& storage, const void* object) const = 0;
	};

	enum ENoInit { NoInit };

	struct ObjectData
	{
		enum { TotalSize = 16 + 4 * sizeof(size_t) }; // 32 bytes for 32-bit platform, 48 bytes for 64 bit
		enum { InlineObjectSize = TotalSize - sizeof(void*) - sizeof(void*) };
		enum { Magic_Value = 0x7b3ae001 };	// just some random value

		union
		{
			// Non-inline data storage
			struct
			{
				size_t Magic;				// use this value to detect non-inline data storage
				void* ObjectPtr;			// object passed to Caller, valid only if Magic == Magic_Value
			};

			// Inline data storage
			char buffer[InlineObjectSize];
		};

		// Fields which exists in all variants of callback
		const IOwnedObject* OwnedObject;	// interface class for management of stored object
		void* Caller;						// function which executes bound action

		FORCEINLINE ObjectData(ENoInit)
		{}

		FORCEINLINE ObjectData()
		: Magic(Magic_Value)
		, ObjectPtr(nullptr)
		, OwnedObject(nullptr)
		, Caller(nullptr)
		{}

		FORCEINLINE ObjectData(ObjectData&& other)
		{
			MoveFrom(MoveTemp(other));
		}

		FORCEINLINE ObjectData(const ObjectData& other)
		{
			CopyFrom(other);
		}

		FORCEINLINE ~ObjectData()
		{
			ReleaseObject();
		}

		FORCEINLINE void* GetObjectPtr() const
		{
			// When Magic != Magic_Value, object is stored inline, so its pointer is 'this'
			return const_cast<void*>(Magic == Magic_Value ? ObjectPtr : this);
		}

		FORCEINLINE ObjectData& operator=(ObjectData&& other)
		{
			ReleaseObject();
			MoveFrom(MoveTemp(other));
			return *this;
		}

		FORCEINLINE ObjectData& operator=(const ObjectData& other)
		{
			ReleaseObject();
			CopyFrom(other);
			return *this;
		}

		void CopyFrom(const ObjectData& other)
		{
			memcpy((void*)this, &other, sizeof(ObjectData));
			if (other.OwnedObject)
			{
				other.OwnedObject->CopyTo(*this, other.GetObjectPtr());
			}
		}

		void MoveFrom(ObjectData&& other)
		{
			memcpy((void*)this, &other, sizeof(ObjectData));
			other.OwnedObject = nullptr;
		}

		void ReleaseObject()
		{
			if (OwnedObject)
			{
				if (Magic == Magic_Value)
				{
					// Not inline object
					OwnedObject->Destroy(ObjectPtr);
					free(ObjectPtr);
				}
				else
				{
					// Inline object
					OwnedObject->Destroy(this);
				}
				OwnedObject = nullptr;
			}
		}
	};

	static_assert(sizeof(ObjectData) == ObjectData::TotalSize, "ObjectData::buffer has wrong size");

} // close namespace to let compiler see our operator new

// Use custom operator new for ObjectData to allocate object either inline or with memory allocation
FORCEINLINE void* operator new(size_t size, Detail::ObjectData& storage)
{
	if (size <= Detail::ObjectData::InlineObjectSize)
	{
		// Use inline storage
		storage.Magic = 0;
		return &storage;
	}
	else
	{
		// Separately allocated block
		storage.Magic = Detail::ObjectData::Magic_Value;
		void* result = malloc(size);
		storage.ObjectPtr = result;
		return result;
	}
}

FORCEINLINE void operator delete(void*, Detail::ObjectData&)
{
	// do nothing, this function here is just to shut up compiler warning
}

namespace Detail
{
	template<typename T>
	struct TOwnedObject : public IOwnedObject
	{
		virtual void Destroy(void* object) const override
		{
			T* typedObject = (T*)object;
			typedObject->~T();
		}
		virtual void CopyTo(ObjectData& storage, const void* object) const override
		{
			new (storage) T(*(const T*)object);
			storage.OwnedObject = this;
		}
		static TOwnedObject Instance;
	};

	// Using a static const instance makes code more effective
	template<typename T>
	TOwnedObject<T> TOwnedObject<T>::Instance;

	typedef decltype(nullptr) nullptr_t;

	// Forward declarations of helper classes
	template<typename T> struct WrapFunction;
	struct WrapLambda;
}

template <typename FuncType>
struct Callback;

template <typename Ret, typename... ParamTypes>
struct Callback<Ret (ParamTypes...)>
{
protected:
	// Make Callback<A> to be friend to Callback<B>. This is required for being able
	// to compile assignment of Callback<Ret()> to Callback<Ret(Params...)>.
	template<typename T> friend struct Callback;
	// Allow calling protected methods from helper classes.
	template<typename T> friend struct Detail::WrapFunction;
	friend struct Detail::WrapLambda;

	// Declaration of function which will be used for execution of callback. Pointer to object
	// goes first, parameters - next. Note that C++ will generate a function which will get
	// the object pointer and call its method by passing all other parameters to the function.
	// In other words, this wrapper function will strip first argument from parameter list
	// and call user function. This could be simplified if we'd pass object pointer last - in
	// this case, compiler could simply replace the function with a single jump instruction,
	// however this won't let us to assign no-parameter callback to a callback with parameters.
	typedef Ret CallbackType(void*, ParamTypes...);

public:
	// Default constructor. Will call default construction of ObjectData. Could be called
	// as "Callback cb" or "Callback cb(nullptr)".
	FORCEINLINE Callback(Detail::nullptr_t = 0)
	{}

	// Standard copy constructor and assignment operator

	FORCEINLINE Callback(Callback&& other)
	: Data(Detail::MoveTemp(other.Data))
	{}

	FORCEINLINE Callback(const Callback& other)
	: Data(other.Data)
	{}

	FORCEINLINE Callback& operator=(Callback&& other)
	{
		Data = Detail::MoveTemp(other.Data);
		return *this;
	}

	FORCEINLINE Callback& operator=(const Callback& other)
	{
		Data = other.Data;
		return *this;
	}

	// Working with null callbacks

	FORCEINLINE Callback& operator=(Detail::nullptr_t)
	{
		Data.ReleaseObject();
		Data.Caller = nullptr;
		return *this;
	}

	FORCEINLINE explicit operator bool() const
	{
		return Data.Caller != nullptr;
	}

	// Allow assignment of another callback which wraps no-argument function. Do not compile these
	// operators if this Callback type already has empty ParamTypes list. Note that we're using
	// "Dummy" type in template parameter list to bypass compiler restrictions (VC compiles without
	// it well, however gcc and clang fails without additional argument).

	template<typename Dummy = void, typename = typename Detail::TEnableIf<(sizeof...(ParamTypes) > 0), Dummy>::Type>
	FORCEINLINE Callback& operator=(Callback<Ret()>&& other)
	{
		Data = Detail::MoveTemp(other.Data);
		return *this;
	}

	template<typename Dummy = void, typename = typename Detail::TEnableIf<(sizeof...(ParamTypes) > 0), Dummy>::Type>
	FORCEINLINE Callback& operator=(const Callback<Ret()>& other)
	{
		Data = other.Data;
		return *this;
	}

	// operator() for callback execution.
	FORCEINLINE Ret operator()(ParamTypes... Params) const
	{
		const auto Caller = reinterpret_cast<CallbackType*>(this->Data.Caller);
		return Caller(Data.GetObjectPtr(), Params...);
	}

protected:
	// Internal constructors used by helper classes

	Callback(CallbackType* Caller, void* Object)
	: Data(Detail::NoInit)
	{
		Set(Caller, Object);
	}

	template<typename FunctorType, typename = typename Detail::TEnableIf<__is_class(FunctorType)>::Type>
	FORCEINLINE Callback(CallbackType* Caller, FunctorType&& Func)
	: Data(Detail::NoInit)
	{
		SetFunctor(Caller, Func);
	}

	template<typename FunctorType, typename = typename Detail::TEnableIf<__is_class(FunctorType)>::Type>
	FORCEINLINE Callback(CallbackType* Caller, const FunctorType& Func)
	: Data(Detail::NoInit)
	{
		SetFunctor(Caller, Func);
	}

private:
	// Internal "Set" functions, should be called only on newly created Callback object.

	FORCEINLINE void Set(CallbackType* Caller, void* Object)
	{
		Data.Magic = Detail::ObjectData::Magic_Value; // really doesn't matter because OwnedObject is null here
		Data.Caller = reinterpret_cast<void*>(Caller);
		Data.OwnedObject = nullptr;
		Data.ObjectPtr = Object;
	}

	template<typename FunctorType>
	void SetFunctor(CallbackType* Caller, FunctorType&& Func)
	{
		using namespace Detail;
		typedef typename TDecay<FunctorType>::Type DecayedFunctorType;

		new (Data) DecayedFunctorType(Detail::MoveTemp(Func));
		Data.OwnedObject = &TOwnedObject<DecayedFunctorType>::Instance;
		Data.Caller = reinterpret_cast<void*>(Caller);
	}

	template<typename FunctorType>
	void SetFunctor(CallbackType* Caller, const FunctorType& Func)
	{
		using namespace Detail;
		typedef typename TDecay<FunctorType>::Type DecayedFunctorType;

		TOwnedObject<DecayedFunctorType>::Instance.CopyTo(Data, &Func);
		Data.Caller = reinterpret_cast<void*>(Caller);
	}

	// The only data object used by callback. It is the same for all callback types.
	Detail::ObjectData Data;
};

// Comparison of Callback with nullptr
template<typename FuncType>
FORCEINLINE bool operator==(Detail::nullptr_t, const Callback<FuncType>& Func)
{
	return !Func;
}

template<typename FuncType>
FORCEINLINE bool operator==(const Callback<FuncType>& Func, Detail::nullptr_t)
{
	return !Func;
}

template<typename FuncType>
FORCEINLINE bool operator!=(Detail::nullptr_t, const Callback<FuncType>& Func)
{
	return (bool)Func;
}

template<typename FuncType>
FORCEINLINE bool operator!=(const Callback<FuncType>& Func, Detail::nullptr_t)
{
	return (bool)Func;
}

namespace Detail
{
	// Helper classes for Callback construction with bound function and object

	// Forward declaration of template
	template<typename T> struct WrapFunction;

	// Specialization for static function
	template<typename Ret, typename... ParamTypes>
	struct WrapFunction<Ret (*)(ParamTypes...)>
	{
		typedef Ret CallbackType(ParamTypes...);
		typedef Ret CallerType(void*, ParamTypes...);
		typedef Ret VoidCallerType(void*, ParamTypes...);

		template<Ret (*Function)(ParamTypes...)>
		FORCEINLINE static CallerType* Caller()
		{
			return [](void*, ParamTypes... Params) -> Ret
				{
					return Function(Params...);
				};
		}

		template<Ret (*Function)(ParamTypes...)>
		FORCEINLINE static Callback<CallbackType> wrap()
		{
			return Callback<CallbackType>(reinterpret_cast<VoidCallerType*>(Caller<Function>()), nullptr);
		}
	};

	// Specialization for non-const class method
	template<typename Class, typename Ret, typename... ParamTypes>
	struct WrapFunction<Ret (Class::*)(ParamTypes...)>
	{
		typedef Ret CallbackType(ParamTypes...);
		typedef Ret CallerType(Class*, ParamTypes...);
		typedef Ret VoidCallerType(void*, ParamTypes...);

		template<Ret (Class::*MethodPtr)(ParamTypes...)>
		FORCEINLINE static CallerType* Caller()
		{
			return [](Class* Object, ParamTypes... Params) -> Ret
				{
					return (*Object.*MethodPtr)(Params...);
				};
		}

		template<Ret (Class::*MethodPtr)(ParamTypes...)>
		FORCEINLINE static Callback<CallbackType> wrap(Class* obj)
		{
			return Callback<CallbackType>(reinterpret_cast<VoidCallerType*>(Caller<MethodPtr>()), obj);
		}
	};

	// Specialization for const class method
	template<typename Class, typename Ret, typename... ParamTypes>
	struct WrapFunction<Ret (Class::*)(ParamTypes...) const>
	{
		typedef Ret CallbackType(ParamTypes...);
		typedef Ret CallerType(const Class*, ParamTypes...);
		typedef Ret VoidCallerType(void*, ParamTypes...);

		template<Ret (Class::*MethodPtr)(ParamTypes...) const>
		FORCEINLINE static CallerType* Caller()
		{
			return [](const Class* Object, ParamTypes... Params) -> Ret
				{
					return (*Object.*MethodPtr)(Params...);
				};
		}

		template<Ret (Class::*MethodPtr)(ParamTypes...) const>
		FORCEINLINE static Callback<CallbackType> wrap(const Class* obj)
		{
			return Callback<CallbackType>(reinterpret_cast<VoidCallerType*>(Caller<MethodPtr>()), const_cast<Class*>(obj));
		}
	};

	// Wrapper for functor (object with operator()).
	// This won't work when passing lambda as a class - lambdas with captures are created at runtime.
	// The only use of this class for lambda is:
	//   auto lambda = []() { lambda body };
	//   auto func = WrapFunction<decltype(lambda)>::Caller();
	template<typename FunctorType>
	struct WrapFunctor
	{
		typedef typename Detail::TDecay<FunctorType>::Type DecayedFunctorType;
		typedef WrapFunction<decltype(&DecayedFunctorType::operator())> SuperType;
		typedef typename SuperType::CallerType CallerType;
		typedef typename SuperType::VoidCallerType VoidCallerType;
		typedef typename SuperType::CallbackType CallbackType;

		FORCEINLINE static CallerType* Caller()
		{
			return SuperType::template Caller<&DecayedFunctorType::operator()>();
		}
	};


	// This lambda wrapper works with lambda object passed at runtime (i.e. can call wrap([]() { lambda body } ))
	struct WrapLambda
	{
		// Declaring 2 different wrap functions - for empty and non-empty lambdas. For empty lambda, we can simply
		// use its caller, without object pointer. For non-empty lambda, we should copy lambda into the Callback.
		// We're using TEnableIf to make 2 versions of the function. Some useful article about this feature:
		// http://loungecpp.net/cpp/enable-if/

		template<typename FunctorType>
		using Wrapper = WrapFunctor<typename Detail::TDecay<FunctorType>::Type>;

		template<typename FunctorType>
		using CallbackType = Callback<typename Wrapper<FunctorType>::CallbackType>;

		// Wrap empty lambda. We don't need to copy lambda into Callback here, only caller function is important.
		template<typename FunctorType, typename = typename Detail::TEnableIf<__is_empty(FunctorType) && __is_class(FunctorType)>::Type>
		FORCEINLINE static auto wrap(const FunctorType&)
			-> CallbackType<FunctorType>
		{
			typedef Wrapper<FunctorType> SuperType;
			return CallbackType<FunctorType>(reinterpret_cast<typename SuperType::VoidCallerType*>(SuperType::Caller()), nullptr);
		}

		// Wrap non-empty lambda. Added "typename = void" to avoid compiled error saying that template already defined (see
		// the article mentioned above).

		// Move semantic wrapper, used only for non-empty functors, i.e. when content should be copied
		template<typename FunctorType, typename = typename Detail::TEnableIf<!__is_empty(FunctorType) && __is_class(FunctorType)>::Type, typename = void>
		static auto wrap(FunctorType&& func)
			-> CallbackType<FunctorType>
		{
			typedef Wrapper<FunctorType> SuperType;
			return CallbackType<FunctorType>(reinterpret_cast<typename SuperType::VoidCallerType*>(SuperType::Caller()), Detail::Forward<FunctorType>(func));
		}

		// Copy semantic wrapper
		template<typename FunctorType, typename = typename Detail::TEnableIf<!__is_empty(FunctorType) && __is_class(FunctorType)>::Type, typename = void>
		static auto wrap(const FunctorType& func)
			-> CallbackType<FunctorType>
		{
			typedef Wrapper<FunctorType> SuperType;
			return CallbackType<FunctorType>(reinterpret_cast<typename SuperType::VoidCallerType*>(SuperType::Caller()), func);
		}
	};

} // namespace Detail

// Easy to use helper macros for binding functions, class methods and lambdas.
#define BIND_STATIC(func)		Detail::WrapFunction<decltype(func)>::wrap<func>()
#define BIND_MEMBER(func, obj)	Detail::WrapFunction<decltype(func)>::wrap<func>(obj)
#define BIND_LAMBDA /*(obj)*/	Detail::WrapLambda::wrap /*(obj)*/
