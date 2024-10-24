module;
#include <boost/variant.hpp>
#include <string>
#include <type_traits>
#include <variant>
export module ivm.value:variant;
import :date;
import :dictionary;
import :primitive;
import :tag;
import :visit;

namespace ivm::value {

// Specialize in order to disable `std::variant` visitor
export template <class... Types>
struct is_variant : std::bool_constant<true> {};

template <class... Types>
constexpr bool is_variant_v = is_variant<Types...>::value;

// Helper which holds a recursive `boost::variant` followed by its alternatives
template <class Variant, class... Types>
struct variant_of;

// Helper which extracts recursive variant alternative types
template <class First, class... Rest>
using recursive_variant = boost::variant<boost::detail::variant::recursive_flag<First>, Rest...>;

// Helper which substitutes recursive variant alternative types
template <class Variant, class Type>
using substitute_recursive = typename boost::detail::variant::substitute<Type, Variant, boost::recursive_variant_>::type;

// Covariant `accept` helper for variant-like types
template <class Type, class Result>
struct covariant;

// Boxes the result of the underlying acceptor into the variant
template <class Meta, class Type, class Result>
struct accept<Meta, covariant<Type, Result>> : accept<Meta, Type> {
		using accept<Meta, Type>::accept;

		constexpr auto operator()(auto_tag auto tag, auto&& value, auto&&... rest) const -> Result
			requires std::is_invocable_v<accept<Meta, Type>, decltype(tag), decltype(value), decltype(rest)...> {
			const accept<Meta, Type>& parent = *this;
			return parent(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...);
		}
};

// Tagged primitives only accept their covariant type
template <class Meta, class Type, class Result>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct accept<Meta, covariant<Type, Result>> : accept<Meta, Type> {
		using accept<Meta, Type>::accept;

		constexpr auto operator()(tag_for_t<Type> tag, auto&& value) const -> Result {
			const accept<Meta, Type>& parent = *this;
			return parent(tag, std::forward<decltype(value)>(value));
		}
};

// Accepting a `std::variant` will delegate to each underlying type's acceptor via the `covariant`
// helper `accept` specialization.
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct accept<Meta, std::variant<Types...>>
		: accept<Meta, covariant<Types, std::variant<Types...>>>... {
		constexpr accept(const auto_visit auto& visit) :
				accept<Meta, covariant<Types, std::variant<Types...>>>{visit}... {}
		constexpr accept(int dummy, const auto_visit auto& visit, const auto_accept auto& acceptor) :
				accept<Meta, covariant<Types, std::variant<Types...>>>{dummy, visit, acceptor}... {}
		using accept<Meta, covariant<Types, std::variant<Types...>>>::operator()...;
};

// Recursive `boost::variant` acceptor
template <class Meta, class Variant, class... Types>
struct accept<Meta, variant_of<Variant, Types...>>
		: accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>... {
		constexpr accept(const auto_visit auto& visit) :
				accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>{visit}... {}
		constexpr accept(int dummy, const auto_visit auto& visit, const auto_accept auto& acceptor) :
				accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>{dummy, visit, acceptor}... {}
		using accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>::operator()...;
};

// `accept` entry for `boost::make_recursive_variant`
template <class Meta, class First, class... Rest>
struct accept<Meta, recursive_variant<First, Rest...>>
		: accept<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>> {
		using accept<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>>::accept;
};

// `std::variant` visitor. This used to delegate to the `boost::variant` visitor above, but
// `boost::apply_visitor` isn't constexpr, so we can't use it to test statically. `boost::variant`
// can't delegate to this one either because it handles recursive variants.
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct visit<Meta, std::variant<Types...>> {
	public:
		constexpr visit() :
				visitors{visit<Meta, Types>{0, *this}...} {}
		constexpr visit(int /*dummy*/, const auto_visit auto& /*visit*/) :
				visit{} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return util::visit_by_index(
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					const auto& visit = std::get<Index>(visitors);
					return visit(std::get<Index>(std::forward<decltype(value)>(value)), accept);
				},
				value
			);
		}

	private:
		std::tuple<visit<Meta, Types>...> visitors;
};

// Visiting a `boost::variant` visits the underlying members
template <class Meta, class Variant, class... Types>
struct visit<Meta, variant_of<Variant, Types...>> : visit<Meta, substitute_recursive<Variant, Types>>... {
		constexpr visit(int dummy, const auto_visit auto& visit_) :
				visit<Meta, substitute_recursive<Variant, Types>>{dummy, visit_}... {}

		auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return boost::apply_visitor(
				[ & ]<class Value>(Value&& value) {
					const visit<Meta, std::decay_t<Value>>& visit = *this;
					return visit(std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// `visit` entry for `boost::make_recursive_variant`
template <class Meta, class First, class... Rest>
struct visit<Meta, recursive_variant<First, Rest...>>
		: visit<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>> {
		using visit<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>>::visit;
		constexpr visit() :
				visit<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>>{0, *this} {}
};

} // namespace ivm::value
