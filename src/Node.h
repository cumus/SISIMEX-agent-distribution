#pragma once
#include "ItemList.h"
#include <memory>

class Node
{
public:

	// Constructor and destructor
	Node(int id, int x = 0, int y = 0);
	~Node();

	// Getters
	int id() { return _id; }
	int x() { return _x; }
	int y() { return _y; }
	ItemList &itemList() { return _itemList; }
	const ItemList &itemList() const { return _itemList; }

private:

	int _id; /**< Id of this node. */
	ItemList _itemList; /**< All items owned by this node. */
	
	// Node position
	int _x = 0;
	int _y = 0;
};

using NodePtr = std::shared_ptr<Node>;
