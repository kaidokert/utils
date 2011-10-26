//-----------------------------------------
// Detail implementation of utils::variant
//-----------------------------------------
//
//          Copyright kennytm (auraHT Ltd.) 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file doc/LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef VARIANT_IMPL_HPP_90M3SQU09PV
#define VARIANT_IMPL_HPP_90M3SQU09PV 1

#include <cassert>
#include <type_traits>
#include <climits>
#if !defined(BOOST_NO_TYPEID)
#include <typeinfo>
#endif

namespace utils { namespace xx_impl {

//{{{ Utilities

template <typename From, typename To>
struct copy_reference
{
    typedef typename std::remove_reference<To>::type RawTo;
    typedef typename std::conditional<std::is_lvalue_reference<From>::value,
                                      RawTo&, RawTo&&>::type Type;
};

template <typename Like, typename U>
typename copy_reference<Like, U>::Type forward_like(U&& t) noexcept
{
    return static_cast<typename copy_reference<Like, U>::Type>(t);
}

static inline constexpr size_t max2(size_t a, size_t b) noexcept
{
    return a > b ? a : b;
}

//}}}

//{{{ Variadic unrestricted union (collapsed tuple)

template <typename...>
union union_impl;

template <typename T, typename... Rest>
union union_impl<T, Rest...>
{
    T head;
    union_impl<Rest...> rest;

    typedef T First;

    union_impl() noexcept {}
    ~union_impl() noexcept {}
};

template <typename T>
struct is_empty
{
    enum {
        value = std::is_same<
            union_impl<>,
            typename std::remove_cv<typename std::remove_reference<T>::type>::type
        >::value
    };
};

template <typename SV, typename T>
typename std::enable_if<!is_empty<T>::value, typename SV::result_type>::type
    apply(T&& un, size_t index, SV& visitor)
{
    if (index == 0)
        return visitor(forward_like<T>(un.head));
    else
        return apply(forward_like<T>(un.rest), index-1, visitor);
}

template <typename SV, typename T>
typename std::enable_if<is_empty<T>::value, typename SV::result_type>::type
    apply(T&&, size_t, SV&) { assert(false); }

template <typename SV, typename T1, typename T2>
typename std::enable_if<!is_empty<T1>::value && !is_empty<T2>::value,
                        typename SV::result_type>::type
    apply2(T1&& un1, size_t index1, T2&& un2, size_t index2, SV& visitor)
{
    if (index1 != 0)
        return apply2(forward_like<T1>(un1.rest), index1-1, std::forward<T2>(un2), index2, visitor);
    else if (index2 != 0)
        return apply2(std::forward<T1>(un1), index1, forward_like<T2>(un2.rest), index2-1, visitor);
    else
        return visitor(forward_like<T1>(un1.head), forward_like<T2>(un2.head));
}

template <typename SV, typename T1, typename T2>
typename std::enable_if<is_empty<T1>::value || is_empty<T2>::value,
                        typename SV::result_type>::type
    apply2(T1&&, size_t, T2&&, size_t, SV&) { assert(false); }

template <typename SV, typename T1, typename T2>
typename std::enable_if<!is_empty<T1>::value && !is_empty<T2>::value,
                        typename SV::result_type>::type
    apply(T1&& un1, T2&& un2, size_t index, SV& visitor)
{
    if (index == 0)
        return visitor(forward_like<T1>(un1.head), forward_like<T2>(un2.head));
    else
        return apply(forward_like<T1>(un1.rest), forward_like<T2>(un2.rest), index-1, visitor);
}

template <typename SV, typename T1, typename T2>
typename std::enable_if<is_empty<T1>::value || is_empty<T2>::value,
                        typename SV::result_type>::type
    apply(T1&&, T2&&, size_t, SV&) { assert(false); }

template <size_t index>
struct static_applier
{
    template <typename V, typename union_type>
    typename V::result_type operator()(union_type& un, V& visitor)
    {
        return static_applier<index-1>()(un.rest, visitor);
    }
};
template <>
struct static_applier<0>
{
    template <typename V, typename union_type>
    typename V::result_type operator()(union_type& un, V& visitor)
    {
        return visitor(un.head);
    }
};

template <>
union union_impl<> {};

//}}}

//{{{ type_traits missing in std

template <typename T, typename U, typename = bool>
struct is_equatable
{
    enum { value = false };
};

template <typename T, typename U>
struct is_equatable<T, U, decltype(std::declval<T>() == std::declval<U>())>
{
    enum { value = true };
};

template <typename T, typename U, typename = bool>
struct is_less_than_comparable
{
    enum { value = false };
};

template <typename T, typename U>
struct is_less_than_comparable<T, U, decltype(std::declval<T>() < std::declval<U>())>
{
    enum { value = true };
};

template <typename T, typename U, typename = bool>
struct is_greater_than_comparable
{
    enum { value = false };
};

template <typename T, typename U>
struct is_greater_than_comparable<T, U, decltype(std::declval<U>() < std::declval<T>())>
{
    enum { value = true };
};

template <typename T, typename U, typename = bool>
struct is_assignable
{
    enum { value = false };
};

template <typename T, typename U>
struct is_assignable<T, U, decltype(std::declval<T&>() = std::declval<U>(), true)>
{
    enum { value = true };
};

template <typename T, typename U, typename = bool>
struct is_constructible
{
    enum { value = std::is_constructible<T, U>::value };
};

template <typename T, typename U, typename = bool>
struct is_same
{
    enum { value = std::is_same<T, U>::value };
};


template <typename T, typename U, typename = bool>
struct is_nothrow_assignable_helper
{
    enum { value = false };
};
template <typename T, typename U>
struct is_nothrow_assignable_helper<T, U, typename std::enable_if<is_assignable<T, U>::value, bool>::type>
{
    enum { value = noexcept(std::declval<T&>() = std::declval<U>()) };
};

template <typename T, typename U>
static constexpr bool is_nothrow_assignable() noexcept
{
    return is_nothrow_assignable_helper<T, U>::value;
}
template <typename T>
static constexpr bool is_nothrow_move_constructible() noexcept
{
    return std::is_nothrow_constructible<typename std::remove_reference<T>::type,
                                         typename std::remove_reference<T>::type&&>::value;
}

template <typename T>
static constexpr bool is_nothrow_copy_constructible() noexcept
{
    return std::is_nothrow_constructible<typename std::remove_reference<T>::type,
                                         const T&>::value;
}

//}}}

//{{{ Get index of type matching condition

static inline constexpr bool check_unambiguous(bool me_exact, bool else_exact, bool rest) noexcept
{
    return me_exact && else_exact ? false : me_exact != else_exact ? true : rest;
}

template <typename T, typename = void>
struct signed_unqual
{
    typedef typename std::remove_cv<T>::type type;
};

template <typename T>
struct signed_unqual<T, typename std::enable_if<std::is_integral<T>::value>::type>
{
    typedef typename std::make_signed<typename std::remove_cv<T>::type>::type type;
};

template <typename T, typename U>
static inline constexpr bool is_same_upto_cv() noexcept
{
    return std::is_same<typename std::remove_cv<T>::type,
                        typename std::remove_cv<U>::type>::value;
}

template <typename T, typename U>
static inline constexpr bool relaxed_same() noexcept
{
    return (std::is_integral<T>::value && std::is_integral<U>::value)
        || (std::is_floating_point<T>::value && std::is_floating_point<U>::value);
}

template <typename, typename>
struct get_index_of_variant;

template <typename, template <typename, typename, typename=bool> class, typename...>
struct get_index;

template <typename From, template <typename, typename, typename=bool> class Checker, typename T, typename... Rest>
struct get_index<From, Checker, T, Rest...>
{
    typedef get_index<From, Checker, Rest...> Tail;

    static const bool is_exact_match = is_same_upto_cv<T, From>();
    static const bool is_relaxed_match = relaxed_same<T, From>();
    static const bool is_variant = is_variant<T>::value;
    static const bool is_ud_match = Checker<T, From>::value;
    static const bool is_strict_ud_match = is_ud_match && !is_variant;

    typedef typename get_index_of_variant<From, T>::type variant_indices;
    static const int var_quality = is_variant ? variant_indices::quality - 2 : 0;

    // Match quality:
    //   exact > su > rel > strict_ud > ud
    static const int local_quality_0 = is_exact_match ? INT_MAX
                                     : is_relaxed_match ? INT_MAX-1
                                     : is_strict_ud_match ? 2
                                     : is_ud_match ? 1
                                     : 0;
    static const int local_quality = local_quality_0 < var_quality ? var_quality : local_quality_0;
    static const bool is_match_here = local_quality > Tail::quality;
    static const int quality = is_match_here ? local_quality : Tail::quality;
    static const size_t index = is_match_here ? 0 : 1 + Tail::index;
    static const bool ambiguous = local_quality == Tail::quality;
    static const bool is_exact = quality == INT_MAX;
    static const bool found = quality > 0;

    /*
    static void debug() noexcept
    {
        printf("%lu (%s vs %s) :: (lqual) %d (vqual) %d (tqual) %d (qual) %d (index) %lu (ambig) %d\n",
            sizeof...(Rest)+1, typeid(From).name(), typeid(T).name(),
            local_quality, var_quality, Tail::quality, quality, index, ambiguous);

        if (is_variant)
        {
            printf("<< var <<\n");
            variant_indices::debug();
            printf(">> var >>\n");
        }

        Tail::debug();
    }
    */
};

template <typename From, template <typename, typename, typename=bool> class Checker>
struct get_index<From, Checker>
{
    static const int quality = 0;
    static const size_t index = 0;

    //static void debug() noexcept {}
};

template <typename From, typename>
struct get_index_of_variant
{
    typedef get_index<From, is_same> type;
};

template <typename From, typename... T>
struct get_index_of_variant<From, variant<T...>>
{
    typedef get_index<From, is_same, T...> type;
};

//}}}

//{{{ Visitors

template <typename T>
class getter_visitor : public static_visitor<T*>
{
public:
    T* operator()(T& obj) const noexcept { return &obj; }
};

class destroy_visitor : public static_visitor<void>
{
public:
    template <typename T>
    void operator()(T& obj) const noexcept
    {
        obj.~T();
    }
};

template <typename U>
class init_visitor_1 : public static_visitor<void>
{
public:
    init_visitor_1(U&& field) noexcept : _field(std::forward<U>(field)) {}

    template <typename T>
    void operator()(T& obj) const noexcept(std::is_nothrow_constructible<T, U>::value)
    {
        new(&obj) T(std::forward<U>(_field));
    }

private:
    U&& _field;
};

class init_visitor_2 : public static_visitor<void>
{
public:
    template <typename T, typename U>
    void operator()(T& dest, U&& src) const noexcept(std::is_nothrow_constructible<T, U>::value)
    {
        new(&dest) T(std::forward<U>(src));
    }
};

template <typename U>
class assign_visitor_1 : public static_visitor<void>
{
public:
    assign_visitor_1(U&& field) : _field(std::forward<U>(field)) {}

    template <typename T, typename = typename std::enable_if<is_assignable<T, U>::value>::type>
    void operator()(T& obj) const noexcept(is_nothrow_assignable<T, U>())
    {
        obj = std::forward<U>(_field);
    }

    template <typename T, typename = typename std::enable_if<!is_assignable<T, U>::value>::type>
    void operator()(T&&) const
    {
        assert(false);
    }

private:
    U&& _field;
};

template <typename V>
class assign_to_visitor : public static_visitor<void>
{
public:
    assign_to_visitor(V& var) : _var(var) {}

    template <typename T>
    void operator()(T&& obj) const noexcept(is_nothrow_assignable<V, T>())
    {
        _var = std::forward<T>(obj);
    }

private:
    V& _var;
};

class is_nothrow_movable_checker : public static_visitor<bool>
{
public:
    template <typename T>
    bool operator()(const T&) const noexcept
    {
        return is_nothrow_move_constructible<T>();
    }
};

class is_nothrow_copyable_checker : public static_visitor<bool>
{
public:
    template <typename T>
    bool operator()(const T&) const noexcept
    {
        return is_nothrow_copy_constructible<T>();
    }
};

class is_nothrow_movable_checker_2 : public static_visitor<bool>
{
public:
    template <typename T, typename U>
    bool operator()(const T&, const U&) const noexcept
    {
        return std::is_nothrow_constructible<T, typename std::remove_reference<U>::type&&>::value;
    }
};

class is_nothrow_copyable_checker_2 : public static_visitor<bool>
{
public:
    template <typename T, typename U>
    bool operator()(const T&, const U&) const noexcept
    {
        return std::is_nothrow_constructible<T, const U&>::value;
    }
};

class assign_visitor_2 : public static_visitor<void>
{
public:
    template <typename T, typename U, typename = typename std::enable_if<is_assignable<T, U>::value>::type>
    void operator()(T& dest, U&& src) const noexcept(is_nothrow_assignable<T, U>())
    {
        dest = std::forward<U>(src);
    }

    template <typename T, typename U, typename = typename std::enable_if<!is_assignable<T, U>::value>::type>
    void operator()(T&&, U&&) const
    {
        assert(false);
    }
};

class swap_visitor_2 : public static_visitor<void>
{
public:
    template <typename T, typename U>
    void operator()(T& first, U& second) const
    {
        std::swap(first, second);
    }
};

template <typename U>
class equals_visitor_1 : public static_visitor<bool>
{
public:
    equals_visitor_1(const U& field) : _field(field) {}

    template <typename T>
    bool operator()(const T& other) const
    {
        return other == _field;
    }

private:
    const U& _field;
};

template <typename U>
class less_than_visitor_1 : public static_visitor<bool>
{
public:
    less_than_visitor_1(const U& field) : _field(field) {}

    template <typename T>
    bool operator()(const T& other) const
    {
        return other < _field;
    }

private:
    const U& _field;
};

template <typename U>
class greater_than_visitor_1 : public static_visitor<bool>
{
public:
    greater_than_visitor_1(const U& field) : _field(field) {}

    template <typename T>
    bool operator()(const T& other) const
    {
        return _field < other;
    }

private:
    const U& _field;
};

class equals_visitor_2 : public static_visitor<bool>
{
public:
    template <typename T, typename U>
    bool operator()(const T& first, const U& second) const
    {
        return first == second;
    }
};

class less_than_visitor_2 : public static_visitor<bool>
{
public:
    template <typename T, typename U>
    bool operator()(const T& first, const U& second) const
    {
        return first < second;
    }
};

class ostream_visitor : public static_visitor<std::ostream&>
{
public:
    ostream_visitor(std::ostream& stream) : _stream(stream) {}

    template <typename T>
    std::ostream& operator()(const T& value)
    {
        return _stream << value;
    }

private:
    std::ostream& _stream;
};

class is_assignable_visitor : public static_visitor<bool>
{
public:
    template <typename T, typename U>
    bool operator()(const T&, const U&) const noexcept
    {
        return is_assignable<T, U>::value;
    }
};

class is_same_visitor : public static_visitor<std::pair<bool, bool>>
{
public:
    template <typename T, typename U>
    std::pair<bool, bool> operator()(const T&, const U&) const noexcept
    {
        return std::make_pair(std::is_same<T, U>::value, is_assignable<T, U>::value);
    }
};

template <typename... T>
class is_one_of_visitor : public static_visitor<bool>
{
public:
    template <typename U>
    bool operator()(const U&) const noexcept
    {
        return get_index<U, is_same, T...>::is_exact;
    }
};

class typeid_visitor : public static_visitor<const std::type_info&>
{
public:
    template <typename U>
    const std::type_info& operator()(const U& u) const noexcept
    {
        return typeid(u);
    }
};

//}}}

template <typename SV>
class delayed_visitor
{
public:
    explicit delayed_visitor(SV& visitor) : _visitor(visitor) {}

    typedef typename SV::result_type result_type;

    template <typename... T>
    result_type operator()(T&&... val) const
    {
        return apply_visitor(_visitor, std::forward<T>(val)...);
    }

private:
    SV& _visitor;
};

}}

#endif

