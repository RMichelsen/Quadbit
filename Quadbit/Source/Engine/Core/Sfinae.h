#pragma once
#include <EASTL/type_traits.h>

namespace Quadbit {
	// SFINAE Detector
	namespace SFINAE {
		namespace detail {
			template <template <typename> typename Op, typename T, typename = void>
			struct is_detected : eastl::false_type {};

			template <template <typename> typename Op, typename T>
			struct is_detected<Op, T, eastl::void_t<Op<T>>> : eastl::true_type {};
		}
		template <template <typename> typename Op, typename T>
		static constexpr bool is_detected_v = detail::is_detected<Op, T>::value;
	}
}