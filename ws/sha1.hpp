#ifndef WS_SHA1_HPP
#define WS_SHA1_HPP

#include <array>
#include <string>
#include <boost/uuid/sha1.hpp>

namespace ws {

inline void sha1hash(const std::string &str, std::array<char, 20> &hash) {
    boost::uuids::detail::sha1 sha1;
    unsigned int uint_hash[5];
    sha1.process_bytes(str.data(), str.size());
    sha1.get_digest(uint_hash);
    /* Endian independent char-hash generation */
    for (std::size_t i = 0; i < hash.size(); ++i)
        hash[i] = uint_hash[i / 4] >> (((-i + 3) % 4) * 8);
}

} /* namespace ws */

#endif /* WS_SHA1_HPP */
