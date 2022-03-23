/*
 * named.h
 *
 *  Created on: 12 Apr 2021
 *      Author: roland
 */

#ifndef UTILITY_NAMED_H_
#define UTILITY_NAMED_H_

#include <algorithm>

template<int N> struct named {

    constexpr named(char const (&s)[N]) {
        std::copy_n(s, N, this->m_elems);
        m_used = true;
    }

    constexpr named(std::nullptr_t n) {
        m_used = false;
    }

    constexpr auto operator<=>(named const&) const = default;

    constexpr std::string_view name() const {
        return &m_elems[0];
    }

    constexpr  bool used() const {
        return m_used;
    }


    /**
     * Contained Data
     */
    char m_elems[N];
    bool m_used;

};
template<int N> named(char const(&)[N])->named<N>;
named(std::nullptr_t n) ->named<0>;


#endif /* UTILITY_NAMED_H_ */
