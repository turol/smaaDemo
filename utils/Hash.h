#ifndef HASH_H
#define HASH_H


#include <cstddef>
#include <cstdint>

#include <unordered_map>
#include <unordered_set>


template <typename K, typename V>
	using HashMap = std::unordered_map<K, V>;


template <typename T>
	using HashSet = std::unordered_set<T>;


template <typename T>
void hashCombine(size_t &h, const T &value) {
	if constexpr (sizeof(size_t) == sizeof(uint64_t)) {
		uint64_t k = std::hash<T>()(value);
		// These come from boost
		// Copyright 2005-2014 Daniel James.
		// Distributed under the Boost Software License, Version 1.0. (See accompanying
		// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
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
	} else {
		assert(sizeof(size_t) == sizeof(uint32_t));
		uint32_t k = std::hash<T>()(value);

		h ^= k + 0x9e3779b9 + (h << 6) + (h >> 2);
	}
}


#endif  // HASH_H
