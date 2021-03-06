/* Asura
   Copyright (C) 2013 MIRACLE LINUX CORPORATION
 
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Logger.h>
using namespace mlpl;

#include <stdexcept>
#include "Utils.h"
#include "ItemTable.h"

struct ItemTable::CrossJoinArg
{
	ItemTable *newTable;
	const ItemTable *rightTable;
	const ItemGroup *itemGroupLTable;
};

struct ItemTable::InnerJoinArg
{
	ItemTable *newTable;
	const ItemTable *rightTable;
	const ItemGroup *itemGroupLTable;
	const size_t indexLeftColumn;
	const size_t indexRightColumn;
};

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------
ItemTable::ItemTable(void)
{
}

ItemTable::ItemTable(const ItemTable &itemTable)
{
	itemTable.readLock();
	ItemGroupListConstIterator it = itemTable.m_groupList.begin();
	for (; it != itemTable.m_groupList.end(); ++it)
		m_groupList.push_back(*it);
	itemTable.readUnlock();
}

ItemGroup *ItemTable::addNewGroup(void)
{
	ItemGroup *grp = new ItemGroup();
	add(grp, false);
	return grp;
}

void ItemTable::add(ItemGroup *group, bool doRef)
{
	writeLock();
	if (!m_groupList.empty()) {
		ItemGroup *tail = m_groupList.back();
		if (!freezeTailGroupIfFirstGroup(tail)) {
			writeUnlock();
			string msg;
			TRMSG(msg, "Failed to freeze the tail group.");
			throw invalid_argument(msg);
		}
		const ItemGroupType *groupType0 = tail->getItemGroupType();
		const ItemGroupType *groupType1 = group->getItemGroupType();
		if (groupType1 == NULL) {
			if (!group->setItemGroupType(groupType0)) {
				writeUnlock();
				throw invalid_argument("Failed to call "
				                       "setItemGroupType.");
			}
		} else if (*groupType0 != *groupType1) {
			writeUnlock();
			string msg;
			TRMSG(msg, "ItemGroupTypes unmatched");
			throw invalid_argument(msg);
		}
	}

	m_groupList.push_back(group);
	writeUnlock();
	if (doRef)
		group->ref();
}

size_t ItemTable::getNumberOfColumns(void) const
{
	readLock();
	if (m_groupList.empty()) {
		readUnlock();
		return 0;
	}
	size_t ret = (*m_groupList.begin())->getNumberOfItems();
	readUnlock();
	return ret;
}

size_t ItemTable::getNumberOfRows(void) const
{
	readLock();
	size_t ret = m_groupList.size();
	readUnlock();
	return ret;
}

ItemTable *ItemTable::innerJoin
  (const ItemTable *itemTable,
   size_t indexLeftColumn, size_t indexRightColumn) const
{
	readLock();
	itemTable->readLock();
	if (m_groupList.empty() || itemTable->m_groupList.empty()) {
		itemTable->readUnlock();
		readUnlock();
		return new ItemTable();
	}

	size_t numColumnLTable = getNumberOfColumns();
	size_t numColumnRTable = itemTable->getNumberOfColumns();
	if (indexLeftColumn >= numColumnLTable ||
	    indexRightColumn >= numColumnRTable) {
		itemTable->readUnlock();
		readUnlock();
		MLPL_BUG("Invalid parameter: numColumnL: %zd, indexL: %zd, "
		         "numColumnR: %zd, indexR: %zd\n",
		         numColumnLTable, indexLeftColumn,
		         numColumnRTable, indexRightColumn);
		return new ItemTable();
	}

	ItemTable *table = new ItemTable();
	InnerJoinArg arg = {
	  table, itemTable, NULL, indexLeftColumn, indexRightColumn};
	try {
		foreach<InnerJoinArg &>(innerJoinForeach, arg);
	} catch (...) {
		itemTable->readUnlock();
		readUnlock();
		throw;
	}
	itemTable->readUnlock();
	readUnlock();
	return table;
}

ItemTable *ItemTable::leftOuterJoin(const ItemTable *itemTable) const
{
	MLPL_BUG("Not implemneted: %s\n", __PRETTY_FUNCTION__);
	return NULL;
}

ItemTable *ItemTable::rightOuterJoin(const ItemTable *itemTable) const
{
	MLPL_BUG("Not implemneted: %s\n", __PRETTY_FUNCTION__);
	return NULL;
}

ItemTable *ItemTable::fullOuterJoin(const ItemTable *itemTable) const
{
	MLPL_BUG("Not implemneted: %s\n", __PRETTY_FUNCTION__);
	return NULL;
}

ItemTable *ItemTable::crossJoin(const ItemTable *itemTable) const
{
	readLock();
	itemTable->readLock();
	if (m_groupList.empty() || itemTable->m_groupList.empty()) {
		itemTable->readUnlock();
		readUnlock();
		return new ItemTable();
	}

	ItemTable *table = new ItemTable();
	CrossJoinArg arg = {table, itemTable};
	try {
		foreach<CrossJoinArg &>(crossJoinForeach, arg);
	} catch (...) {
		itemTable->readUnlock();
		readUnlock();
		throw;
	}
	itemTable->readUnlock();
	readUnlock();
	return table;
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------
ItemTable::~ItemTable()
{
	// We don't need to take a lock, because this object is no longer used.
	ItemGroupListIterator it = m_groupList.begin();
	for (; it != m_groupList.end(); ++it) {
		ItemGroup *group = *it;
		group->unref();
	}
}

bool ItemTable::freezeTailGroupIfFirstGroup(ItemGroup *tail)
{
	if (tail->isFreezed())
		return true;

	size_t numList = m_groupList.size();
	if (numList == 1) {
		tail->freeze();
		return true;
	}
	return false;
}

void ItemTable::joinForeachCore(ItemTable *newTable,
                                const ItemGroup *itemGroupLTable,
                                const ItemGroup *itemGroupRTable)
{
	ItemGroup *newGroup = newTable->addNewGroup();
	const ItemGroup *itemGroupArray[] = {
	  itemGroupLTable, itemGroupRTable, NULL};
	for (size_t index = 0; itemGroupArray[index] != NULL; index++) {
		const ItemGroup *itemGroup = itemGroupArray[index];
		size_t numItems = itemGroup->getNumberOfItems();
		for (size_t i = 0; i < numItems; i++)
			newGroup->add(itemGroup->getItemAt(i));
	}
}

bool ItemTable::crossJoinForeachRTable(const ItemGroup *itemGroupRTable,
                                       CrossJoinArg &arg)
{
	joinForeachCore(arg.newTable, arg.itemGroupLTable, itemGroupRTable);
	return true;
}

bool ItemTable::crossJoinForeach(const ItemGroup *itemGroup, CrossJoinArg &arg)
{
	arg.itemGroupLTable = itemGroup;
	arg.rightTable->foreach<CrossJoinArg &>(crossJoinForeachRTable, arg);
	return true;
}

bool ItemTable::innerJoinForeachRTable(const ItemGroup *itemGroupRTable,
                                       InnerJoinArg &arg)
{
	ItemData *leftData =
	  arg.itemGroupLTable->getItemAt(arg.indexLeftColumn);
	ItemData *rightData = itemGroupRTable->getItemAt(arg.indexRightColumn);
	if (*leftData != *rightData)
		return true;

	joinForeachCore(arg.newTable, arg.itemGroupLTable, itemGroupRTable);
	return true;
}

bool ItemTable::innerJoinForeach(const ItemGroup *itemGroup, InnerJoinArg &arg)
{
	arg.itemGroupLTable = itemGroup;
	arg.rightTable->foreach<InnerJoinArg &>(innerJoinForeachRTable, arg);
	return true;
}
