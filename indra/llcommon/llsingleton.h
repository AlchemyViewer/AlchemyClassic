/** 
 * @file llsingleton.h
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
#ifndef LLSINGLETON_H
#define LLSINGLETON_H

#include <boost/unordered_set.hpp>
#include <list>
#include <vector>
#include <typeinfo>
#include <mutex>

class LLSingletonBase
{
public:
    class MasterList;

    LLSingletonBase(const LLSingletonBase&) = delete;
    LLSingletonBase& operator=(const LLSingletonBase&) = delete;

private:
    // All existing LLSingleton instances are tracked in this master list.
    typedef std::list<LLSingletonBase*> list_t;
    static list_t& get_master();
    // This, on the other hand, is a stack whose top indicates the LLSingleton
    // currently being initialized.
    static list_t& get_initializing();
    // Produce a vector<LLSingletonBase*> of master list, in dependency order.
    typedef std::vector<LLSingletonBase*> vec_t;
    static vec_t dep_sort();

    bool mCleaned;                  // cleanupSingleton() has been called
    // we directly depend on these other LLSingletons
    typedef boost::unordered_set<LLSingletonBase*> set_t;
    set_t mDepends;

protected:
    typedef enum e_init_state
    {
        UNINITIALIZED = 0,          // must be default-initialized state
        CONSTRUCTING,               // within DERIVED_TYPE constructor
        CONSTRUCTED,                // finished DERIVED_TYPE constructor
        INITIALIZING,               // within DERIVED_TYPE::initSingleton()
        INITIALIZED,                // normal case
        DELETED                     // deleteSingleton() or deleteAll() called
    } EInitState;

    // Define tag<T> to pass to our template constructor. You can't explicitly
    // invoke a template constructor with ordinary template syntax:
    // http://stackoverflow.com/a/3960925/5533635
    template <typename T>
    struct tag
    {
        typedef T type;
    };

    // Base-class constructor should only be invoked by the DERIVED_TYPE
    // constructor, which passes tag<DERIVED_TYPE> for various purposes.
    template <typename DERIVED_TYPE>
    LLSingletonBase(tag<DERIVED_TYPE>);
    virtual ~LLSingletonBase() = default;

    // Every new LLSingleton should be added to/removed from the master list
    void add_master();
    void remove_master();
    // with a little help from our friends.
    template <class T> friend struct LLSingleton_manage_master;

    // Maintain a stack of the LLSingleton subclass instance currently being
    // initialized. We use this to notice direct dependencies: we want to know
    // if A requires B. We deduce a dependency if while initializing A,
    // control reaches B::getInstance().
    // We want &A to be at the top of that stack during both A::A() and
    // A::initSingleton(), since a call to B::getInstance() might occur during
    // either.
    // Unfortunately the desired timespan does not correspond neatly with a
    // single C++ scope, else we'd use RAII to track it. But we do know that
    // LLSingletonBase's constructor definitely runs just before
    // LLSingleton's, which runs just before the specific subclass's.
    void push_initializing(const char*);
    // LLSingleton is, and must remain, the only caller to initSingleton().
    // That being the case, we control exactly when it happens -- and we can
    // pop the stack immediately thereafter.
    void pop_initializing();
    // Remove 'this' from the init stack in case of exception in the
    // LLSingleton subclass constructor.
    static void reset_initializing(list_t::size_type size);
private:
    // logging
    static void log_initializing(const char* verb, const char* name);
protected:
    // If a given call to B::getInstance() happens during either A::A() or
    // A::initSingleton(), record that A directly depends on B.
    void capture_dependency(list_t& initializing, EInitState);

    // delegate LL_ERRS() logging to llsingleton.cpp
    static void logerrs(const char* p1, const char* p2="",
                        const char* p3="", const char* p4="");
    // delegate LL_WARNS() logging to llsingleton.cpp
    static void logwarns(const char* p1, const char* p2="",
                         const char* p3="", const char* p4="");
    static std::string demangle(const char* mangled);
    template <typename T>
    static std::string classname()       { return demangle(typeid(T).name()); }
    template <typename T>
    static std::string classname(T* ptr) { return demangle(typeid(*ptr).name()); }

    // Default methods in case subclass doesn't declare them.
    virtual void initSingleton() {}
    virtual void cleanupSingleton() {}

    // deleteSingleton() isn't -- and shouldn't be -- a virtual method. It's a
    // class static. However, given only Foo*, deleteAll() does need to be
    // able to reach Foo::deleteSingleton(). Make LLSingleton (which declares
    // deleteSingleton()) store a pointer here. Since we know it's a static
    // class method, a classic-C function pointer will do.
    void (*mDeleteSingleton)();

public:
    /**
     * Call this to call the cleanupSingleton() method for every LLSingleton
     * constructed since the start of the last cleanupAll() call. (Any
     * LLSingleton constructed DURING a cleanupAll() call won't be cleaned up
     * until the next cleanupAll() call.) cleanupSingleton() neither deletes
     * nor destroys its LLSingleton; therefore it's safe to include logic that
     * might take significant realtime or even throw an exception.
     *
     * The most important property of cleanupAll() is that cleanupSingleton()
     * methods are called in dependency order, leaf classes last. Thus, given
     * two LLSingleton subclasses A and B, if A's dependency on B is properly
     * expressed as a B::getInstance() or B::instance() call during either
     * A::A() or A::initSingleton(), B will be cleaned up after A.
     *
     * If a cleanupSingleton() method throws an exception, the exception is
     * logged, but cleanupAll() attempts to continue calling the rest of the
     * cleanupSingleton() methods.
     */
    static void cleanupAll();
    /**
     * Call this to call the deleteSingleton() method for every LLSingleton
     * constructed since the start of the last deleteAll() call. (Any
     * LLSingleton constructed DURING a deleteAll() call won't be cleaned up
     * until the next deleteAll() call.) deleteSingleton() deletes and
     * destroys its LLSingleton. Any cleanup logic that might take significant
     * realtime -- or throw an exception -- must not be placed in your
     * LLSingleton's destructor, but rather in its cleanupSingleton() method.
     *
     * The most important property of deleteAll() is that deleteSingleton()
     * methods are called in dependency order, leaf classes last. Thus, given
     * two LLSingleton subclasses A and B, if A's dependency on B is properly
     * expressed as a B::getInstance() or B::instance() call during either
     * A::A() or A::initSingleton(), B will be cleaned up after A.
     *
     * If a deleteSingleton() method throws an exception, the exception is
     * logged, but deleteAll() attempts to continue calling the rest of the
     * deleteSingleton() methods.
     */
    static void deleteAll();
};

// Most of the time, we want LLSingleton_manage_master() to forward its
// methods to real LLSingletonBase methods.
template <class T>
struct LLSingleton_manage_master
{
    void add(LLSingletonBase* sb) { sb->add_master(); }
    void remove(LLSingletonBase* sb) { sb->remove_master(); }
    void push_initializing(LLSingletonBase* sb) { sb->push_initializing(typeid(T).name()); }
    void pop_initializing (LLSingletonBase* sb) { sb->pop_initializing(); }
    // used for init stack cleanup in case an LLSingleton subclass constructor
    // throws an exception
    void reset_initializing(LLSingletonBase::list_t::size_type size)
    {
        LLSingletonBase::reset_initializing(size);
    }
    // For any LLSingleton subclass except the MasterList, obtain the init
    // stack from the MasterList singleton instance.
    LLSingletonBase::list_t& get_initializing() { return LLSingletonBase::get_initializing(); }
};

// But for the specific case of LLSingletonBase::MasterList, don't.
template <>
struct LLSingleton_manage_master<LLSingletonBase::MasterList>
{
    void add(LLSingletonBase*) {}
    void remove(LLSingletonBase*) {}
    void push_initializing(LLSingletonBase*) {}
    void pop_initializing (LLSingletonBase*) {}
    // since we never pushed, no need to clean up
    void reset_initializing(LLSingletonBase::list_t::size_type size) {}
    LLSingletonBase::list_t& get_initializing()
    {
        // The MasterList shouldn't depend on any other LLSingletons. We'd
        // get into trouble if we tried to recursively engage that machinery.
        static LLSingletonBase::list_t sDummyList;
        return sDummyList;
    }
};

// Now we can implement LLSingletonBase's template constructor.
template <typename DERIVED_TYPE>
LLSingletonBase::LLSingletonBase(tag<DERIVED_TYPE>):
    mCleaned(false),
    mDeleteSingleton(nullptr)
{
    // This is the earliest possible point at which we can push this new
    // instance onto the init stack. LLSingleton::constructSingleton() can't
    // do it before calling the constructor, because it doesn't have an
    // instance pointer until the constructor returns. Fortunately this
    // constructor is guaranteed to be called before any subclass constructor.
    // Make this new instance the currently-initializing LLSingleton.
    LLSingleton_manage_master<DERIVED_TYPE>().push_initializing(this);
}

// forward declare for friend directive within LLSingleton
template <typename DERIVED_TYPE>
class LLParamSingleton;

/**
 * LLSingleton implements the getInstance() method part of the Singleton
 * pattern. It can't make the derived class constructors protected, though, so
 * you have to do that yourself.
 *
 * Derive your class from LLSingleton, passing your subclass name as
 * LLSingleton's template parameter, like so:
 *
 *   class Foo: public LLSingleton<Foo>
 *   {
 *       // use this macro at start of every LLSingleton subclass
 *       LLSINGLETON(Foo);
 *   public:
 *       // ...
 *   };
 *
 *   Foo& instance = Foo::instance();
 *
 * LLSingleton recognizes a couple special methods in your derived class.
 *
 * If you override LLSingleton<T>::initSingleton(), your method will be called
 * immediately after the instance is constructed. This is useful for breaking
 * circular dependencies: if you find that your LLSingleton subclass
 * constructor references other LLSingleton subclass instances in a chain
 * leading back to yours, move the instance reference from your constructor to
 * your initSingleton() method.
 *
 * If you override LLSingleton<T>::cleanupSingleton(), your method will be
 * called if someone calls LLSingletonBase::cleanupAll(). The significant part
 * of this promise is that cleanupAll() will call individual
 * cleanupSingleton() methods in reverse dependency order.
 *
 * That is, consider LLSingleton subclasses C, B and A. A depends on B, which
 * in turn depends on C. These dependencies are expressed as calls to
 * B::instance() or B::getInstance(), and C::instance() or C::getInstance().
 * It shouldn't matter whether these calls appear in A::A() or
 * A::initSingleton(), likewise B::B() or B::initSingleton().
 *
 * We promise that if you later call LLSingletonBase::cleanupAll():
 * 1. A::cleanupSingleton() will be called before
 * 2. B::cleanupSingleton(), which will be called before
 * 3. C::cleanupSingleton().
 * Put differently, if your LLSingleton subclass constructor or
 * initSingleton() method explicitly depends on some other LLSingleton
 * subclass, you may continue to rely on that other subclass in your
 * cleanupSingleton() method.
 *
 * We introduce a special cleanupSingleton() method because cleanupSingleton()
 * operations can involve nontrivial realtime, or might throw an exception. A
 * destructor should do neither!
 *
 * If your cleanupSingleton() method throws an exception, we log that
 * exception but proceed with the remaining cleanupSingleton() calls.
 *
 * Similarly, if at some point you call LLSingletonBase::deleteAll(), all
 * remaining LLSingleton instances will be destroyed in dependency order. (Or
 * call MySubclass::deleteSingleton() to specifically destroy the canonical
 * MySubclass instance.)
 *
 * As currently written, LLSingleton is not thread-safe.
 */
template <typename DERIVED_TYPE>
class LLSingleton : public LLSingletonBase
{
private:
    // Allow LLParamSingleton subclass -- but NOT DERIVED_TYPE itself -- to
    // access our private members.
    friend class LLParamSingleton<DERIVED_TYPE>;

    // LLSingleton only supports a nullary constructor. However, the specific
    // purpose for its subclass LLParamSingleton is to support Singletons
    // requiring constructor arguments. constructSingleton() supports both use
    // cases.
    template <typename... Args>
    static void constructSingleton(Args&&... args)
    {
        auto prev_size = LLSingleton_manage_master<DERIVED_TYPE>().get_initializing().size();
        // getInstance() calls are from within constructor
        sData.mInitState = CONSTRUCTING;
        try
        {
            sData.mInstance = new DERIVED_TYPE(std::forward<Args>(args)...);
            // we have called constructor, have not yet called initSingleton()
            sData.mInitState = CONSTRUCTED;
        }
        catch (const std::exception& err)
        {
            // LLSingletonBase might -- or might not -- have pushed the new
            // instance onto the init stack before the exception. Reset the
            // init stack to its previous size BEFORE logging so log-machinery
            // LLSingletons don't record a dependency on DERIVED_TYPE!
            LLSingleton_manage_master<DERIVED_TYPE>().reset_initializing(prev_size);
            logwarns("Error constructing ", classname<DERIVED_TYPE>().c_str(),
                     ": ", err.what());
            // There isn't a separate EInitState value meaning "we attempted
            // to construct this LLSingleton subclass but could not," so use
            // DELETED. That seems slightly more appropriate than UNINITIALIZED.
            sData.mInitState = DELETED;
            // propagate the exception
            throw;
        }
    }

    static void finishInitializing()
    {
        // getInstance() calls are from within initSingleton()
        sData.mInitState = INITIALIZING;
        try
        {
            // initialize singleton after constructing it so that it can
            // reference other singletons which in turn depend on it, thus
            // breaking cyclic dependencies
            sData.mInstance->initSingleton();
            sData.mInitState = INITIALIZED;

            // pop this off stack of initializing singletons
            pop_initializing();
        }
        catch (const std::exception& err)
        {
            // pop this off stack of initializing singletons here, too --
            // BEFORE logging, so log-machinery LLSingletons don't record a
            // dependency on DERIVED_TYPE!
            pop_initializing();
            logwarns("Error in ", classname<DERIVED_TYPE>().c_str(),
                     "::initSingleton(): ", err.what());
            // and get rid of the instance entirely
            deleteSingleton();
            // propagate the exception
            throw;
        }
    }

    static void pop_initializing()
    {
        // route through LLSingleton_manage_master so we Do The Right Thing
        // (namely, nothing) for MasterList
        LLSingleton_manage_master<DERIVED_TYPE>().pop_initializing(sData.mInstance);
    }

    // Without this 'using' declaration, the static method we're declaring
    // here would hide the base-class method we want it to call.
    using LLSingletonBase::capture_dependency;
    static void capture_dependency()
    {
        // By this point, if DERIVED_TYPE was pushed onto the initializing
        // stack, it has been popped off. So the top of that stack, if any, is
        // an LLSingleton that directly depends on DERIVED_TYPE. If
        // getInstance() was called by another LLSingleton, rather than from
        // vanilla application code, record the dependency.
        sData.mInstance->capture_dependency(
            LLSingleton_manage_master<DERIVED_TYPE>().get_initializing(),
            sData.mInitState);
    }

    // We know of no way to instruct the compiler that every subclass
    // constructor MUST be private. However, we can make the LLSINGLETON()
    // macro both declare a private constructor and provide the required
    // friend declaration. How can we ensure that every subclass uses
    // LLSINGLETON()? By making that macro provide a definition for this pure
    // virtual method. If you get "can't instantiate class due to missing pure
    // virtual method" for this method, then add LLSINGLETON(yourclass) in the
    // subclass body.
    virtual void you_must_use_LLSINGLETON_macro() = 0;

    // The purpose of this struct is to engage the C++11 guarantee that static
    // variables declared in function scope are initialized exactly once, even
    // if multiple threads concurrently reach the same declaration.
    // https://en.cppreference.com/w/cpp/language/storage_duration#Static_local_variables
    // Since getInstance() declares a static instance of SingletonInitializer,
    // only the first call to getInstance() calls constructSingleton().
    struct SingletonInitializer
    {
        SingletonInitializer()
        {
            constructSingleton();
        }
    };

protected:
    // Pass DERIVED_TYPE explicitly to LLSingletonBase's constructor because,
    // until our subclass constructor completes, *this isn't yet a
    // full-fledged DERIVED_TYPE.
    LLSingleton(): LLSingletonBase(LLSingletonBase::tag<DERIVED_TYPE>())
    {
        // populate base-class function pointer with the static
        // deleteSingleton() function for this particular specialization
        mDeleteSingleton = &deleteSingleton;

        // add this new instance to the master list
        LLSingleton_manage_master<DERIVED_TYPE>().add(this);
    }

public:
    virtual ~LLSingleton()
    {
        // remove this instance from the master list
        LLSingleton_manage_master<DERIVED_TYPE>().remove(this);
        sData.mInstance = nullptr;
        sData.mInitState = DELETED;
    }

    /**
     * @brief Immediately delete the singleton.
     *
     * A subsequent call to LLProxy::getInstance() will construct a new
     * instance of the class.
     *
     * Without an explicit call to LLSingletonBase::deleteAll(), LLSingletons
     * are implicitly destroyed after main() has exited and the C++ runtime is
     * cleaning up statically-constructed objects. Some classes derived from
     * LLSingleton have objects that are part of a runtime system that is
     * terminated before main() exits. Calling the destructor of those objects
     * after the termination of their respective systems can cause crashes and
     * other problems during termination of the project. Using this method to
     * destroy the singleton early can prevent these crashes.
     *
     * An example where this is needed is for a LLSingleton that has an APR
     * object as a member that makes APR calls on destruction. The APR system is
     * shut down explicitly before main() exits. This causes a crash on exit.
     * Using this method before the call to apr_terminate() and NOT calling
     * getInstance() again will prevent the crash.
     */
    static void deleteSingleton()
    {
        delete sData.mInstance;
        // SingletonData state handled by destructor, above
    }

    static DERIVED_TYPE* getInstance()
    {
        // call constructSingleton() only the first time we get here
        static SingletonInitializer sInitializer;

        switch (sData.mInitState)
        {
        case UNINITIALIZED:
            // should never be uninitialized at this point
            logerrs("Uninitialized singleton ",
                    classname<DERIVED_TYPE>().c_str());
            return nullptr;

        case CONSTRUCTING:
            // here if DERIVED_TYPE's constructor (directly or indirectly)
            // calls DERIVED_TYPE::getInstance()
            logerrs("Tried to access singleton ",
                    classname<DERIVED_TYPE>().c_str(),
                    " from singleton constructor!");
            return nullptr;

        case CONSTRUCTED:
            // first time through: set to CONSTRUCTED by
            // constructSingleton(), called by sInitializer's constructor;
            // still have to call initSingleton()
            finishInitializing();
            break;

        case INITIALIZING:
            // here if DERIVED_TYPE::initSingleton() (directly or indirectly)
            // calls DERIVED_TYPE::getInstance(): go ahead and allow it
        case INITIALIZED:
            // normal subsequent calls
            break;

        case DELETED:
            // called after deleteSingleton()
            logwarns("Trying to access deleted singleton ",
                     classname<DERIVED_TYPE>().c_str(),
                     " -- creating new instance");
            // This recovery sequence is NOT thread-safe! We would need a
            // recursive_mutex a la LLParamSingleton.
            constructSingleton();
            finishInitializing();
            break;
        }

        // record the dependency, if any: check if we got here from another
        // LLSingleton's constructor or initSingleton() method
        capture_dependency();
        return sData.mInstance;
    }

    // Reference version of getInstance()
    // Preferred over getInstance() as it disallows checking for NULL
    static DERIVED_TYPE& instance()
    {
        return *getInstance();
    }

    // Has this singleton been created yet?
    // Use this to avoid accessing singletons before they can safely be constructed.
    static bool instanceExists()
    {
        return sData.mInitState == INITIALIZED;
    }

    // Has this singleton been deleted? This can be useful during shutdown
    // processing to avoid "resurrecting" a singleton we thought we'd already
    // cleaned up.
    static bool wasDeleted()
    {
        return sData.mInitState == DELETED;
    }

private:
    struct SingletonData
    {
        // explicitly has a default constructor so that member variables are zero initialized in BSS
        // and only changed by singleton logic, not constructor running during startup
        EInitState      mInitState;
        DERIVED_TYPE*   mInstance;
    };
    static SingletonData sData;
};

template<typename T>
typename LLSingleton<T>::SingletonData LLSingleton<T>::sData;


/**
 * LLParamSingleton<T> is like LLSingleton<T>, except in the following ways:
 *
 * * It is NOT instantiated on demand (instance() or getInstance()). You must
 *   first call initParamSingleton(constructor args...).
 * * Before initParamSingleton(), calling instance() or getInstance() dies with
 *   LL_ERRS.
 * * initParamSingleton() may be called only once. A second call dies with
 *   LL_ERRS.
 * * However, distinct initParamSingleton() calls can be used to engage
 *   different constructors, as long as only one such call is executed at
 *   runtime.
 * * Unlike LLSingleton, an LLParamSingleton cannot be "revived" by an
 *   instance() or getInstance() call after deleteSingleton().
 *
 * Importantly, though, each LLParamSingleton subclass does participate in the
 * dependency-ordered LLSingletonBase::deleteAll() processing.
 */
template <typename DERIVED_TYPE>
class LLParamSingleton : public LLSingleton<DERIVED_TYPE>
{
private:
    typedef LLSingleton<DERIVED_TYPE> super;
    // Use a recursive_mutex in case of constructor circularity. With a
    // non-recursive mutex, that would result in deadlock rather than the
    // logerrs() call in getInstance().
    typedef std::recursive_mutex mutex_t;

public:
    using super::deleteSingleton;
    using super::instanceExists;
    using super::wasDeleted;

    // Passes arguments to DERIVED_TYPE's constructor and sets appropriate states
    template <typename... Args>
    static void initParamSingleton(Args&&... args)
    {
        // In case racing threads both call initParamSingleton() at the same
        // time, serialize them. One should initialize; the other should see
        // mInitState already set.
        std::unique_lock<mutex_t> lk(getMutex());
        // For organizational purposes this function shouldn't be called twice
        if (super::sData.mInitState != super::UNINITIALIZED)
        {
            super::logerrs("Tried to initialize singleton ",
                           super::template classname<DERIVED_TYPE>().c_str(),
                           " twice!");
        }
        else
        {
            super::constructSingleton(std::forward<Args>(args)...);
            super::finishInitializing();
        }
    }

    static DERIVED_TYPE* getInstance()
    {
        // In case racing threads call getInstance() at the same moment as
        // initParamSingleton(), serialize the calls.
        std::unique_lock<mutex_t> lk(getMutex());

        switch (super::sData.mInitState)
        {
        case super::UNINITIALIZED:
            super::logerrs("Uninitialized param singleton ",
                           super::template classname<DERIVED_TYPE>().c_str());
            break;

        case super::CONSTRUCTING:
            super::logerrs("Tried to access param singleton ",
                           super::template classname<DERIVED_TYPE>().c_str(),
                           " from singleton constructor!");
            break;

        case super::CONSTRUCTED:
            // Should never happen!? The CONSTRUCTED state is specifically to
            // navigate through LLSingleton::SingletonInitializer getting
            // constructed (once) before LLSingleton::getInstance()'s switch
            // on mInitState. But our initParamSingleton() method calls
            // constructSingleton() and then calls finishInitializing(), which
            // immediately sets INITIALIZING. Why are we here?
            super::logerrs("Param singleton ",
                           super::template classname<DERIVED_TYPE>().c_str(),
                           "::initSingleton() not yet called");
            break;

        case super::INITIALIZING:
            // As with LLSingleton, explicitly permit circular calls from
            // within initSingleton()
        case super::INITIALIZED:
            // for any valid call, capture dependencies
            super::capture_dependency();
            return super::sData.mInstance;

        case super::DELETED:
            super::logerrs("Trying to access deleted param singleton ",
                           super::template classname<DERIVED_TYPE>().c_str());
            break;
        }

        // should never actually get here; this is to pacify the compiler,
        // which assumes control might return from logerrs()
        return nullptr;
    }

    // instance() is replicated here so it calls
    // LLParamSingleton::getInstance() rather than LLSingleton::getInstance()
    // -- avoid making getInstance() virtual
    static DERIVED_TYPE& instance()
    {
        return *getInstance();
    }

private:
    // sMutex must be a function-local static rather than a static member. One
    // of the essential features of LLSingleton and friends is that they must
    // support getInstance() even when the containing module's static
    // variables have not yet been runtime-initialized. A mutex requires
    // construction. A static class member might not yet have been
    // constructed.
    //
    // We could store a dumb mutex_t*, notice when it's NULL and allocate a
    // heap mutex -- but that's vulnerable to race conditions. And we can't
    // defend the dumb pointer with another mutex.
    //
    // We could store a std::atomic<mutex_t*> -- but a default-constructed
    // std::atomic<T> does not contain a valid T, even a default-constructed
    // T! Which means std::atomic, too, requires runtime initialization.
    //
    // But a function-local static is guaranteed to be initialized exactly
    // once, the first time control reaches that declaration.
    static mutex_t& getMutex()
    {
        static mutex_t sMutex;
        return sMutex;
    }
};

/**
 * Initialization locked singleton, only derived class can decide when to initialize.
 * Starts locked.
 * For cases when singleton has a dependency onto something or.
 *
 * LLLockedSingleton is like an LLParamSingleton with a nullary constructor.
 * It cannot be instantiated on demand (instance() or getInstance() call) --
 * it must be instantiated by calling construct(). However, it does
 * participate in dependency-ordered LLSingletonBase::deleteAll() processing.
 */
template <typename DT>
class LLLockedSingleton : public LLParamSingleton<DT>
{
    typedef LLParamSingleton<DT> super;

public:
    using super::deleteSingleton;
    using super::getInstance;
    using super::instance;
    using super::instanceExists;
    using super::wasDeleted;

    static void construct()
    {
        super::initParamSingleton();
    }
};

/**
 * Use LLSINGLETON(Foo); at the start of an LLSingleton<Foo> subclass body
 * when you want to declare an out-of-line constructor:
 *
 * @code
 *   class Foo: public LLSingleton<Foo>
 *   {
 *       // use this macro at start of every LLSingleton subclass
 *       LLSINGLETON(Foo);
 *   public:
 *       // ...
 *   };
 *   // ...
 *   [inline]
 *   Foo::Foo() { ... }
 * @endcode
 *
 * Unfortunately, this mechanism does not permit you to define even a simple
 * (but nontrivial) constructor within the class body. If it's literally
 * trivial, use LLSINGLETON_EMPTY_CTOR(); if not, use LLSINGLETON() and define
 * the constructor outside the class body. If you must define it in a header
 * file, use 'inline' (unless it's a template class) to avoid duplicate-symbol
 * errors at link time.
 */
#define LLSINGLETON(DERIVED_CLASS, ...)                                 \
private:                                                                \
    /* implement LLSingleton pure virtual method whose sole purpose */  \
    /* is to remind people to use this macro */                         \
    virtual void you_must_use_LLSINGLETON_macro() override {}           \
    friend class LLSingleton<DERIVED_CLASS>;                            \
    DERIVED_CLASS(__VA_ARGS__)

/**
 * Use LLSINGLETON_EMPTY_CTOR(Foo); at the start of an LLSingleton<Foo>
 * subclass body when the constructor is trivial:
 *
 * @code
 *   class Foo: public LLSingleton<Foo>
 *   {
 *       // use this macro at start of every LLSingleton subclass
 *       LLSINGLETON_EMPTY_CTOR(Foo);
 *   public:
 *       // ...
 *   };
 * @endcode
 */
#define LLSINGLETON_EMPTY_CTOR(DERIVED_CLASS)                           \
    /* LLSINGLETON() is carefully implemented to permit exactly this */ \
    LLSINGLETON(DERIVED_CLASS) = default

#endif
