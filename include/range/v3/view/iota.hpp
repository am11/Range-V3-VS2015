/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2013-2014
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_VIEW_IOTA_HPP
#define RANGES_V3_VIEW_IOTA_HPP

#include <climits>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/view_facade.hpp>
#include <range/v3/utility/concepts.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/view/take_exactly.hpp>
#include <range/v3/view/delimit.hpp>

namespace ranges
{
    inline namespace v3
    {
        namespace concepts
        {
            struct BidirectionalIncrementable
              : refines<Incrementable>
            {
                template<typename T>
                auto requires_(T&& t) -> decltype(
                    concepts::valid_expr(
                        concepts::has_type<T &>(--t),
                        concepts::has_type<T>(t--)
                    ));
            };

            struct RandomAccessIncrementable
              : refines<BidirectionalIncrementable>
            {
                template<typename T>
                auto requires_(T&& t) -> decltype(
                    concepts::valid_expr(
                        concepts::model_of<Integral>(t - t),
                        concepts::has_type<T &>(t += (t - t)),
                        concepts::has_type<T &>(t -= (t - t)),
                        concepts::convertible_to<T>(t - (t - t)),
                        concepts::convertible_to<T>(t + (t - t)),
                        concepts::convertible_to<T>((t - t) + t)
                    ));
            };
        }

        template<typename T>
        using BidirectionalIncrementable = concepts::models<concepts::BidirectionalIncrementable, T>;

        template<typename T>
        using RandomAccessIncrementable = concepts::models<concepts::RandomAccessIncrementable, T>;

        template<typename T>
        using incrementable_concept =
            concepts::most_refined<
                meta::list<
                    concepts::RandomAccessIncrementable,
                    concepts::BidirectionalIncrementable,
                    concepts::Incrementable,
                    concepts::WeaklyIncrementable>, T>;

        template<typename T>
        using incrementable_concept_t = meta::_t<incrementable_concept<T>>;

        /// \cond
        namespace detail
        {
            template<typename Val, typename Iota = incrementable_concept_t<Val>,
                bool IsIntegral = std::is_integral<Val>::value>
            struct iota_difference_
              : ranges::difference_type<Val>
            {};

            template<typename Val>
            struct iota_difference_<Val, concepts::RandomAccessIncrementable, true>
            {
            private:
                using difference_t = decltype(std::declval<Val>() - std::declval<Val>());
                static constexpr std::size_t bits = sizeof(difference_t) * CHAR_BIT;
            public:
                using type =
                    meta::if_<
                        meta::not_<std::is_same<Val, difference_t>>,
                        meta::_t<std::make_signed<difference_t>>,
                        meta::if_c<
                            (bits < 8),
                            std::int_fast8_t,
                            meta::if_c<
                                (bits < 16),
                                std::int_fast16_t,
                                meta::if_c<
                                    (bits < 32),
                                    std::int_fast32_t,
                                    std::int_fast64_t> > > >;
            };

            template<typename Val>
            struct iota_difference
              : iota_difference_<Val>
            {};

            template<typename Val>
            using iota_difference_t = meta::_t<iota_difference<Val>>;

#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            template<typename Val, CONCEPT_REQUIRES_(!Integral<Val>::value)>
#else
            template<typename Val, CONCEPT_REQUIRES_(!Integral<Val>())>
#endif
            iota_difference_t<Val> iota_minus(Val const &v0, Val const &v1)
            {
                return v0 - v1;
            }

#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            template<typename Val, CONCEPT_REQUIRES_(SignedIntegral<Val>::value)>
#else
            template<typename Val, CONCEPT_REQUIRES_(SignedIntegral<Val>())>
#endif
            iota_difference_t<Val> iota_minus(Val const &v0, Val const &v1)
            {
                using D = iota_difference_t<Val>;
                return (D) v0 - (D) v1;
            }

#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            template<typename Val, CONCEPT_REQUIRES_(UnsignedIntegral<Val>::value)>
#else
            template<typename Val, CONCEPT_REQUIRES_(UnsignedIntegral<Val>())>
#endif
            iota_difference_t<Val> iota_minus(Val const &v0, Val const &v1)
            {
                using D = iota_difference_t<Val>;
                return (D) (v0 - v1);
            }
        }
        /// \endcond

        /// \addtogroup group-views
        /// @{

        /// An iota view in a closed range with non-random access iota value type
        template<typename From, typename To /* = From */>
        struct closed_iota_view
          : view_facade<closed_iota_view<From, To>, finite>
        {
        private:
            friend range_access;
            using difference_type_ = detail::iota_difference_t<From>;

            From from_;
            To to_;
            bool done_ = false;

            From current() const
            {
                return from_;
            }
            void next()
            {
                if(from_ == to_)
                    done_ = true;
                else
                    ++from_;
            }
            bool done() const
            {
                return done_;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(Incrementable<From>::value)
#else
            CONCEPT_REQUIRES(Incrementable<From>())
#endif
            bool equal(closed_iota_view const &that) const
            {
                return that.from_ == from_;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(BidirectionalIncrementable<From>::value)
#else
            CONCEPT_REQUIRES(BidirectionalIncrementable<From>())
#endif
            void prev()
            {
                --from_;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>::value)
#else
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>())
#endif
            void advance(difference_type_ n)
            {
                RANGES_ASSERT(detail::iota_minus(to_, from_) >= n);
                from_ += n;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>::value)
#else
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>())
#endif
            difference_type_ distance_to(closed_iota_view const &that) const
            {
                return detail::iota_minus(that.from_, from_);
            }
        public:
            closed_iota_view() = default;
            constexpr closed_iota_view(From from, To to)
              : from_(detail::move(from)), to_(detail::move(to))
            {}
        };

        template<typename From, typename To /* = void*/>
        struct iota_view
          : view_facade<iota_view<From, To>, finite>
        {
        private:
            friend range_access;
            using difference_type_ = detail::iota_difference_t<From>;

            From from_;
            To to_;

            From current() const
            {
                return from_;
            }
            void next()
            {
                ++from_;
            }
            bool done() const
            {
                return from_ == to_;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(Incrementable<From>::value)
#else
            CONCEPT_REQUIRES(Incrementable<From>())
#endif
            bool equal(iota_view const &that) const
            {
                return that.from_ == from_;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(BidirectionalIncrementable<From>::value)
#else
            CONCEPT_REQUIRES(BidirectionalIncrementable<From>())
#endif
            void prev()
            {
                --from_;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>::value)
#else
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>())
#endif
            void advance(difference_type_ n)
            {
                RANGES_ASSERT(detail::iota_minus(to_, from_) >= n);
                from_ += n;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>::value)
#else
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>())
#endif
            difference_type_ distance_to(iota_view const &that) const
            {
                return detail::iota_minus(that.from_, from_);
            }
        public:
            iota_view() = default;
            constexpr iota_view(From from, To to)
              : from_(detail::move(from)), to_(detail::move(to))
            {}
        };

        template<typename From>
        struct iota_view<From, void>
          : view_facade<iota_view<From, void>, infinite>
        {
        private:
            using incrementable_concept_t = ranges::incrementable_concept<From>;
            friend range_access;
            using difference_type_ = detail::iota_difference_t<From>;

            From value_;

            From current() const
            {
                return value_;
            }
            void next()
            {
                ++value_;
            }
            constexpr bool done() const
            {
                return false;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(Incrementable<From>::value)
#else
            CONCEPT_REQUIRES(Incrementable<From>())
#endif
            bool equal(iota_view const &that) const
            {
                return that.value_ == value_;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(BidirectionalIncrementable<From>::value)
#else
            CONCEPT_REQUIRES(BidirectionalIncrementable<From>())
#endif
            void prev()
            {
                --value_;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>::value)
#else
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>())
#endif
            void advance(difference_type_ n)
            {
                value_ += n;
            }
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>::value)
#else
            CONCEPT_REQUIRES(RandomAccessIncrementable<From>())
#endif
            difference_type_ distance_to(iota_view const &that) const
            {
                return detail::iota_minus(that.value_, value_);
            }
        public:
            iota_view() = default;
            constexpr explicit iota_view(From value)
              : value_(detail::move(value))
            {}
        };

        namespace view
        {
            struct iota_fn
            {
            private:
                template<typename From>
                static detail::take_exactly_view_<iota_view<From>, true>
                impl(From from, From to, concepts::RandomAccessIncrementable *)
                {
                    return {iota_view<From>{std::move(from)}, detail::iota_minus(to, from)};
                }
                template<typename From, typename To>
                static iota_view<From, To>
                impl(From from, To to, concepts::WeaklyIncrementable *)
                {
                    return {std::move(from), std::move(to)};
                }
            public:
                template<typename From,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(WeaklyIncrementable<From>::value)>
#else
                    CONCEPT_REQUIRES_(WeaklyIncrementable<From>())>
#endif
                iota_view<From> operator()(From value) const
                {
                    return iota_view<From>{std::move(value)};
                }
                template<typename From, typename To>
                meta::if_c<
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    WeaklyIncrementable<From>::value && EqualityComparable<From, To>::value,
#else
                    WeaklyIncrementable<From>() && EqualityComparable<From, To>(),
#endif
                    meta::if_<
                        meta::and_<RandomAccessIncrementable<From>, Same<From, To>>,
                        detail::take_exactly_view_<iota_view<From>, true>,
                        iota_view<From, To>>>
                operator()(From from, To to) const;

            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename From,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(!WeaklyIncrementable<From>::value)>
#else
                    CONCEPT_REQUIRES_(!WeaklyIncrementable<From>())>
#endif
                void operator()(From) const
                {
                    CONCEPT_ASSERT_MSG(WeaklyIncrementable<From>(),
                        "The object passed to view::iota must model the WeaklyIncrementable concept; "
                        "that is, it must have pre- and post-increment operators and it must have a "
                        " difference_type");
                }
                template<typename From, typename To,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(!(WeaklyIncrementable<From>::value && EqualityComparable<From, To>::value))>
#else
                    CONCEPT_REQUIRES_(!(WeaklyIncrementable<From>() && EqualityComparable<From, To>()))>
#endif
                void operator()(From, To) const
                {
                    CONCEPT_ASSERT_MSG(WeaklyIncrementable<From>(),
                        "The object passed to view::iota must model the WeaklyIncrementable concept; "
                        "that is, it must have pre- and post-increment operators and it must have a "
                        " difference_type");
                    CONCEPT_ASSERT_MSG(EqualityComparable<From, To>(),
                        "The two arguments passed to view::iota must be EqualityComparable with == and !=");
                }
            #endif
            };

            template<typename From, typename To>
            meta::if_c<
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                WeaklyIncrementable<From>::value && EqualityComparable<From, To>::value,
#else
                WeaklyIncrementable<From>() && EqualityComparable<From, To>(),
#endif
                meta::if_<
                    meta::and_<RandomAccessIncrementable<From>, Same<From, To>>,
                    detail::take_exactly_view_<iota_view<From>, true>,
                    iota_view<From, To>>>
            iota_fn::operator()(From from, To to) const
            {
                return iota_fn::impl(std::move(from), std::move(to), incrementable_concept<From>{});
            }

            struct closed_iota_fn
            {
            private:
                template<typename From>
                static detail::take_exactly_view_<iota_view<From>, true>
                impl(From from, From to, concepts::RandomAccessIncrementable *)
                {
                    return {iota_view<From>{std::move(from)}, detail::iota_minus(to, from) + 1};
                }
                template<typename From, typename To>
                static closed_iota_view<From, To>
                impl(From from, To to, concepts::WeaklyIncrementable *)
                {
                    return {std::move(from), std::move(to)};
                }
            public:
                template<typename From, typename To,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(WeaklyIncrementable<From>::value && EqualityComparable<From, To>::value)>
#else
                    CONCEPT_REQUIRES_(WeaklyIncrementable<From>() && EqualityComparable<From, To>())>
#endif
                meta::if_<
                    meta::and_<RandomAccessIncrementable<From>, Same<From, To>>,
                    detail::take_exactly_view_<iota_view<From>, true>,
                    closed_iota_view<From, To>>
                operator()(From from, To to) const
                {
                    return closed_iota_fn::impl(std::move(from), std::move(to), incrementable_concept<From>{});
                }
            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename From, typename To,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(!(WeaklyIncrementable<From>::value && EqualityComparable<From, To>::value))>
#else
                    CONCEPT_REQUIRES_(!(WeaklyIncrementable<From>() && EqualityComparable<From, To>()))>
#endif
                void operator()(From, To) const
                {
                    CONCEPT_ASSERT_MSG(WeaklyIncrementable<From>(),
                        "The object passed to view::closed_iota must model the WeaklyIncrementable concept; "
                        "that is, it must have pre- and post-increment operators and it must have a "
                        " difference_type");
                    CONCEPT_ASSERT_MSG(EqualityComparable<From, To>(),
                        "The two arguments passed to view::closed_iota must be EqualityComparable with == and !=");
                }
            #endif
            };

            /// \relates iota_fn
            /// \ingroup group-views
            namespace
            {
                constexpr auto&& iota = static_const<iota_fn>::value;
            }

            /// \relates closed_iota_fn
            /// \ingroup group-views
            namespace
            {
                constexpr auto&& closed_iota = static_const<closed_iota_fn>::value;
            }

            struct ints_fn
              : iota_view<int>
            {
                ints_fn() = default;

                template<typename Val,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(Integral<Val>::value)>
#else
                    CONCEPT_REQUIRES_(Integral<Val>())>
#endif
                iota_view<Val> operator()(Val value) const
                {
                    return iota_view<Val>{value};
                }
                template<typename Val>
                meta::if_c<
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    (bool)Integral<Val>::value,
#else
                    (bool)Integral<Val>(),
#endif
                    detail::take_exactly_view_<iota_view<Val>, true>>
                operator()(Val from, Val to) const;

            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename Val,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(!Integral<Val>::value)>
#else
                    CONCEPT_REQUIRES_(!Integral<Val>())>
#endif
                void operator()(Val) const
                {
                    CONCEPT_ASSERT_MSG(Integral<Val>(),
                        "The object passed to view::ints must be Integral");
                }
                template<typename Val,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(!Integral<Val>::value)>
#else
                    CONCEPT_REQUIRES_(!Integral<Val>())>
#endif
                void operator()(Val, Val) const
                {
                    CONCEPT_ASSERT_MSG(Integral<Val>(),
                        "The object passed to view::ints must be Integral");
                }
            #endif
            };

            template<typename Val>
            meta::if_c<
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                (bool)Integral<Val>::value,
#else
                (bool)Integral<Val>(),
#endif
                detail::take_exactly_view_<iota_view<Val>, true>>
            ints_fn::operator()(Val from, Val to) const
            {
                return {iota_view<Val>{from}, detail::iota_minus(to, from)};
            }

            struct closed_ints_fn
            {
                template<typename Val,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(Integral<Val>::value)>
#else
                    CONCEPT_REQUIRES_(Integral<Val>())>
#endif
                detail::take_exactly_view_<iota_view<Val>, true> operator()(Val from, Val to) const
                {
                    return {iota_view<Val>{from}, detail::iota_minus(to, from) + 1};
                }
            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename Val,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(!Integral<Val>::value)>
#else
                    CONCEPT_REQUIRES_(!Integral<Val>())>
#endif
                void operator()(Val, Val) const
                {
                    CONCEPT_ASSERT_MSG(Integral<Val>(),
                        "The object passed to view::closed_ints must be Integral");
                }
            #endif
            };

            /// \relates ints_fn
            /// \ingroup group-views
            namespace
            {
                constexpr auto&& ints = static_const<ints_fn>::value;
            }

            /// \relates closed_ints_fn
            /// \ingroup group-views
            namespace
            {
                constexpr auto&& closed_ints = static_const<closed_ints_fn>::value;
            }
        }
        /// @}
    }
}

#endif
