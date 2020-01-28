#ifndef HASH_H
#define HASH_H


#include <unordered_map>
#include <unordered_set>


template <typename K, typename V>
	using HashMap = std::unordered_map<K, V>;


template <typename T>
	using HashSet = std::unordered_set<T>;



#endif  // HASH_H
