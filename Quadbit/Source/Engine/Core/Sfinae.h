#pragma once
#include <type_traits>

namespace Quadbit {
	// SFINAE Detector
	namespace SFINAE {
		namespace detail {
			template <template <typename> typename Op, typename T, typename = void>
			struct is_detected : std::false_type {};

			template <template <typename> typename Op, typename T>
			struct is_detected<Op, T, std::void_t<Op<T>>> : std::true_type {};
		}
		template <template <typename> typename Op, typename T>
		static constexpr bool is_detected_v = detail::is_detected<Op, T>::value;
	}
}