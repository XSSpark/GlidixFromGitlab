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

#include <glidix/util/hashmap.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>

HashMap* hmNew()
{
	HashMap *hm = (HashMap*) kmalloc(sizeof(HashMap));
	if (hm == NULL)
	{
		return NULL;
	};

	memset(hm, 0, sizeof(HashMap));
	return hm;
};

static unsigned int hmHash(const char *key)
{
	int result = 0xABCD1234;

	while (*key != 0)
	{
		result <<= 6;
		result ^= *key++;
	};

	return result;
};

void hmDestroy(HashMap *hm)
{
	int i;
	for (i=0; i<HM_NUM_BUCKETS; i++)
	{
		while (hm->buckets[i] != NULL)
		{
			HashMapEntry *ent = hm->buckets[i];
			hm->buckets[i] = ent->next;

			kfree(ent->key);
			kfree(ent);
		};
	};
};

void* hmGet(HashMap *hm, const char *key)
{
	unsigned int hash = hmHash(key) % HM_NUM_BUCKETS;

	HashMapEntry *ent;
	for (ent=hm->buckets[hash]; ent!=NULL; ent=ent->next)
	{
		if (strcmp(ent->key, key) == 0)
		{
			return ent->value;
		};
	};

	return NULL;
};

int hmSet(HashMap *hm, const char *key, void *value)
{
	unsigned int hash = hmHash(key) % HM_NUM_BUCKETS;

	HashMapEntry *ent;
	for (ent=hm->buckets[hash]; ent!=NULL; ent=ent->next)
	{
		if (strcmp(ent->key, key) == 0)
		{
			ent->value = value;
			return 0;
		};
	};

	char *keydup = strdup(key);
	if (keydup == NULL)
	{
		return -1;
	};

	ent = (HashMapEntry*) kmalloc(sizeof(HashMapEntry));
	if (ent == NULL)
	{
		kfree(keydup);
		return -1;
	};

	ent->key = keydup;
	ent->value = value;
	ent->prev = NULL;
	ent->next = hm->buckets[hash];
	if (ent->next != NULL) ent->next->prev = ent;
	hm->buckets[hash] = ent;

	return 0;
};

static void hmLoadEnt(HashMapIterator *it)
{
	it->key = it->ent->key;
	it->value = it->ent->value;
};

static void hmLoadBucket(HashMapIterator *it, int minBucket)
{
	int i;
	for (i=minBucket; i<HM_NUM_BUCKETS; i++)
	{
		if (it->hm->buckets[i] != NULL)
		{
			it->ent = it->hm->buckets[i];
			hmLoadEnt(it);
			break;
		};
	};

	it->bucket = i;
};

void hmBegin(HashMapIterator *it, HashMap *hm)
{
	it->hm = hm;
	hmLoadBucket(it, 0);
};

int hmEnd(HashMapIterator *it)
{
	return it->bucket == HM_NUM_BUCKETS;
};

void hmNext(HashMapIterator *it)
{
	it->ent = it->ent->next;
	if (it->ent == NULL)
	{
		hmLoadBucket(it, it->bucket+1);
	};
};