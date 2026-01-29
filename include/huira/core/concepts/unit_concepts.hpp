#include <concepts>
#include <ratio>
#include <type_traits>

namespace huira::units {
    template<typename T>
    struct is_std_ratio_impl : std::false_type {};

    template<std::intmax_t Num, std::intmax_t Den>
    struct is_std_ratio_impl<std::ratio<Num, Den>> : std::true_type {};


    template<typename T>
    concept IsRatio = is_std_ratio_impl<T>::value;

    template<typename T>
    struct is_unit_tag : std::false_type {};


    template<typename T>
    concept IsUnitTag = is_unit_tag<T>::value;

    template<typename T>
    concept IsRatioOrTag = IsRatio<T> || IsUnitTag<T>;

}
