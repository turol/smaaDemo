#ifndef HASH_H
#define HASH_H


#include <cstdint>

#include <unordered_map>
#include <unordered_set>


template <typename K, typename V>
	using HashMap = std::unordered_map<K, V>;


template <typename T>
	using HashSet = std::unordered_set<T>;


// These come from boost
// Copyright 2005-2014 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
static inline uint32_t combineHashes(uint32_t seed, uint32_t value) {
	seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);

	return seed;
}


static inline uint64_t combineHashes(uint64_t h, uint64_t k) {
	const uint64_t m = 0xc6a4a7935bd1e995UL;
	const int r = 47;

	k *= m;
	k ^= k >> r;
	k *= m;

	h ^= k;
	h *= m;

	// Completely arbitrary number, to prevent 0's
	// from hashing to 0.
	h += 0xe6546b64;

	return h;
}


template <typename It>
size_t hashRange(It b, It e) {
	size_t h = 0;

	for (It it = b; it != e; it++) {
		h = combineHashes(h, std::hash<typename It::value_type>()(*it));
	}

	return h;
}


#endif  // HASH_H
