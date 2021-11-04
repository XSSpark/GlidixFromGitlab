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

#include <glidix/util/treemap.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>

TreeMap* treemapNew()
{
	TreeMap *map = (TreeMap*) kmalloc(sizeof(TreeMap));
	if (map == NULL)
	{
		return map;
	};

	// zero out the master node
	memset(map, 0, sizeof(TreeMap));

	return map;
};

static void _treemapReleaseNode(TreeMapNode *node, int depth)
{
	if (depth == 3) return;
	if (node == NULL) return;

	int i;
	for (i=0; i<TREEMAP_NUM_CHILDREN; i++)
	{
		_treemapReleaseNode(node->children[i], depth+1);
		kfree(node->children[i]);
	};
};

void treemapDestroy(TreeMap *map)
{
	_treemapReleaseNode(&map->masterNode, 0);
	kfree(map);
};

void* treemapGet(TreeMap *map, uint32_t index)
{
	TreeMapNode *node = &map->masterNode;
	index = __builtin_bswap32(index);

	int i;
	for (i=0; i<TREEMAP_DEPTH; i++)
	{
		if (node == NULL)
		{
			break;
		};

		node = (TreeMapNode*) node->children[index & (TREEMAP_NUM_CHILDREN-1)];
		index >>= 8;
	};

	return node;
};

errno_t treemapSet(TreeMap *map, uint32_t index, void *ptr)
{
	TreeMapNode *node = &map->masterNode;
	index = __builtin_bswap32(index);
	
	int i;
	for (i=0; i<TREEMAP_DEPTH-1; i++)
	{
		uint32_t indexIntoNode = index & (TREEMAP_NUM_CHILDREN-1);
		if (node->children[indexIntoNode] == NULL)
		{
			void *sub = kmalloc(sizeof(TreeMapNode));
			if (sub == NULL)
			{
				return ENOMEM;
			};

			memset(sub, 0, sizeof(TreeMapNode));
			node->children[indexIntoNode] = sub;
		};

		node = (TreeMapNode*) node->children[indexIntoNode];
		index >>= 8;
	};

	node->children[index] = ptr;
	return 0;
};

static void treemapWalkRecur(
	TreeMap *treemap,
	TreeMapNode *node,
	int depth,
	uint32_t indexBuilder,
	TreeMapWalkCallback callback,
	void *context
)
{
	if (depth == TREEMAP_DEPTH)
	{
		callback(treemap, indexBuilder, node, context);
		return;
	};

	uint32_t subIndex;
	for (subIndex=0; subIndex<TREEMAP_NUM_CHILDREN; subIndex++)
	{
		TreeMapNode *subnode = node->children[subIndex];
		if (subnode != NULL)
		{
			treemapWalkRecur(treemap, subnode, depth+1, (indexBuilder << 8) | subIndex, callback, context);
		};
	};
};

void treemapWalk(TreeMap *treemap, TreeMapWalkCallback callback, void *context)
{
	treemapWalkRecur(treemap, &treemap->masterNode, 0, 0, callback, context);
};