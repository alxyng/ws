#ifndef WS_BASE64_HPP
#define WS_BASE64_HPP

#include <string>
#include <sstream>
#include <iostream>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

namespace ws {

/* http://stackoverflow.com/questions/7053538/how-do-i-encode-a-string-to-base64-using-only-boost */
inline std::string base64encode(const char *data, std::size_t length) {
    using namespace boost::archive::iterators;

    std::stringstream ss;
    const std::string right_padding[] = {"", "==", "="};

    /* 1) Convert binary values to base64 characters
     * 2) Retrieve 6-bit integers from a sequence of 8-bit bytes */
    typedef
        base64_from_binary< /* 1 */
            transform_width<const char *, 6, 8> /* 2 */
        > base64_text;

    std::copy(
        base64_text(data),
        base64_text(data + length),
        ostream_iterator<char>(ss)
    );

    return ss.str() + right_padding[length % 3];
}

} /* namespace ws */

#endif /* WS_BASE64_HPP */
