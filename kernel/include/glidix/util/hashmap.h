/*
	Glidix kernel

	Copyright (c) 2021, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __glidix_util_hashmap_h
#define	__glidix_util_hashmap_h

#include <glidix/util/common.h>

/**
 * Number of buckets in a HashMap.
 */
#define	HM_NUM_BUCKETS					64

/**
 * Represents a node in the hash map.
 */
typedef struct HashMapEntry_ HashMapEntry;
struct HashMapEntry_
{
	/**
	 * Links.
	 */
	HashMapEntry *prev;
	HashMapEntry *next;

	/**
	 * The key.
	 */
	char *key;

	/**
	 * The value.
	 */
	void *value;
};

/**
 * A hash map. All hash map operations are NOT thread-safe! Any shared hash maps
 * must be explicitly protected with locks.
 */
typedef struct
{
	/**
	 * The buckets.
	 */
	HashMapEntry *buckets[HM_NUM_BUCKETS];
} HashMap;

/**
 * An iterator for a hash map. Allocate this locally; no memory allocations are
 * performed during hash map iteration.
 */
typedef struct
{
	/**
	 * The current key (public).
	 */
	const char *key;

	/**
	 * The value (public).
	 */
	void *value;

	/**
	 * The hash map we are iterating.
	 */
	HashMap *hm;

	/**
	 * The current bucket.
	 */
	int bucket;

	/**
	 * The current entry.
	 */
	HashMapEntry *ent;
} HashMapIterator;

/**
 * Create a hash map. Returns NULL if allocation failed.
 */
HashMap* hmNew();

/**
 * Destroy the hash map. This deletes all entries but does nothing to the values
 * themselves. Make sure you iterate the hash map and destroy the values if this
 * is required, before calling this!
 */
void hmDestroy(HashMap *hm);

/**
 * Get the value of the specified key. Returns NULL if the key doesn't exist.
 */
void* hmGet(HashMap *hm, const char *key);

/**
 * Set the value of the specified key. If the value is NULL, the key is deleted.
 * Returns 0 on success, -1 if an allocation failed (and the hash map was therefore
 * not updated).
 * 
 * If a value already exists at the key, the value is changed, and nothing more is
 * done to the value. If cleanup is required, you have to get the value and clean it
 * up before calling this.
 */
int hmSet(HashMap *hm, const char *key, void *value);

/**
 * Begin iterating a hashmap.
 */
void hmBegin(HashMapIterator *it, HashMap *hm);

/**
 * Returns nonzero if the iterator points past the end of the hash map.
 */
int hmEnd(HashMapIterator *it);

/**
 * Move the iterator to the next entry.
 */
void hmNext(HashMapIterator *it);

#endif