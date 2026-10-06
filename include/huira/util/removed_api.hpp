#pragma once

namespace huira::detail {

/**
 * @brief Always false, but only once a template using it is instantiated.
 *
 * A method removed from the API stays declared as a template whose body is
 *
 *     static_assert(detail::removed_api<Args...>, "f() was removed in vX. Use g() instead.");
 *
 * so that a call fails to compile with that message, while code that does not call it is
 * unaffected. (A plain static_assert(false) would fail as soon as the class is compiled.) In
 * Python, removed methods are left out and named by the class's __getattr__ instead: see
 * camera_handle_py.ipp.
 */
template <typename... T>
inline constexpr bool removed_api = false;

} // namespace huira::detail
