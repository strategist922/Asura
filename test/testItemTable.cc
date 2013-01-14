#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

#include <ParsableString.h>
#include <StringUtils.h>
using namespace mlpl;

#include <cstdio>
#include <cutter.h>
#include <cppcutter.h>
#include "ItemTable.h"

namespace testItemTable {

enum {
	DEFAULT_ITEM_ID,
	ITEM_ID_0,
	ITEM_ID_1,
	ITEM_ID_2,

	ITEM_ID_AGE,
	ITEM_ID_NAME,
	ITEM_ID_FAVORITE_COLOR,
	ITEM_ID_HEIGHT,
	ITEM_ID_NICKNAME,
};

struct TableStruct0 {
	int age;
	const char *name;
	const char *favoriteColor;
};

struct TableStruct1 {
	const char *name;
	int height;
	const char *nickname;
};

static TableStruct0 tableContent0[] = {
  {10, "anri", "red"},
  {20, "mai", "blue"},
};
static const int NUM_TABLE0 = sizeof(tableContent0) / sizeof(TableStruct0);

static TableStruct1 tableContent1[] = {
  {"anri", 150, "ann"},
  {"nobita",200, "snake"},
  {"maria", 170, "mai"},
};
static const int NUM_TABLE1 = sizeof(tableContent1) / sizeof(TableStruct1);

static void addItemTable0(ItemGroup *grp, TableStruct0 *table)
{
	grp->add(new ItemInt(ITEM_ID_AGE, table->age), false);
	grp->add(new ItemString(ITEM_ID_NAME, table->name), false);
	grp->add(new ItemString(ITEM_ID_FAVORITE_COLOR, table->favoriteColor), false);
}

static void addItemTable1(ItemGroup *grp, TableStruct1 *table)
{
	grp->add(new ItemString(ITEM_ID_NAME, table->name), false);
	grp->add(new ItemInt(ITEM_ID_HEIGHT, table->height), false);
	grp->add(new ItemString(ITEM_ID_NICKNAME, table->nickname), false);
}

template<typename T>
static ItemTable * addItems(T* srcTable, int numTable,
                            void (*addFunc)(ItemGroup *, T *))
{
	ItemTable *table = new ItemTable();
	for (int i = 0; i < numTable; i++) {
		ItemGroup *grp = table->addNewGroup();
		(*addFunc)(grp, &srcTable[i]);
	}
	return table;
}

static const int NUM_TABLE_POOL = 10;
static ItemTable *g_table[NUM_TABLE_POOL];
static ItemTable *&x_table = g_table[0];
static ItemTable *&y_table = g_table[1];
static ItemTable *&z_table = g_table[2];

void teardown(void)
{
	for (int i = 0; i < NUM_TABLE_POOL; i++) {
		if (g_table[i]) {
			g_table[i]->unref();
			g_table[i] = NULL;
		}
	}
}

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------
void test_constructor(void)
{
	x_table = new ItemTable();
	cppcut_assert_equal(1, x_table->getUsedCount());
}

void test_addNewGroup(void)
{
	x_table = new ItemTable();
	ItemGroup *grp = x_table->addNewGroup();
	cut_assert_not_null(grp);
	cppcut_assert_equal(1, grp->getUsedCount());
}

void test_addNoRef(void)
{
	x_table = new ItemTable();
	ItemGroup *grp = new ItemGroup();
	x_table->add(grp, false);
	cppcut_assert_equal(1, grp->getUsedCount());
}

void test_addRef(void)
{
	x_table = new ItemTable();
	ItemGroup *grp = new ItemGroup();
	x_table->add(grp, true);
	cppcut_assert_equal(2, grp->getUsedCount());
	grp->unref();
}

void test_getNumberOfRows(void)
{
	x_table = new ItemTable();
	cut_assert_equal_int(0, x_table->getNumberOfRows());

	ItemGroup *grp = x_table->addNewGroup();
	cut_assert_equal_int(1, x_table->getNumberOfRows());

	grp = x_table->addNewGroup();
	cut_assert_equal_int(2, x_table->getNumberOfRows());
}

void test_getNumberOfColumns(void)
{
	x_table = new ItemTable();
	cut_assert_equal_int(0, x_table->getNumberOfColumns());

	ItemGroup *grp = x_table->addNewGroup();
	cut_assert_equal_int(0, x_table->getNumberOfColumns());
}

void test_addWhenHasMoreThanOneGroup(void)
{
	x_table = new ItemTable();
	ItemGroup *grp = x_table->addNewGroup();
	grp->add(new ItemInt(ITEM_ID_0, 500), false);
	grp->add(new ItemString(ITEM_ID_1, "foo"), false);

	// add second group
	grp = x_table->addNewGroup();
	const ItemGroupType *itemGroupType = grp->getItemGroupType();
	cut_assert_not_null(itemGroupType);
	cppcut_assert_equal(ITEM_TYPE_INT, itemGroupType->getType(0));
	cppcut_assert_equal(ITEM_TYPE_STRING, itemGroupType->getType(1));

	// add items
	grp->add(new ItemInt(ITEM_ID_0, 500), false);
	grp->add(new ItemString(ITEM_ID_1, "foo"), false);

	// add thrid group
	grp = x_table->addNewGroup();
	grp->add(new ItemInt(ITEM_ID_0, -20), false);
	grp->add(new ItemString(ITEM_ID_1, "dog"), false);
	cppcut_assert_equal((size_t)3, x_table->getNumberOfRows());
}

void test_addInvalidItemsWhenHasMoreThanOneGroup(void)
{
	x_table = new ItemTable();
	ItemGroup *grp = x_table->addNewGroup();
	grp->add(new ItemInt(ITEM_ID_0, 500), false);
	grp->add(new ItemString(ITEM_ID_1, "foo"), false);

	// add second group
	grp = x_table->addNewGroup();
	ItemData *item = new ItemString(ITEM_ID_1, "foo");
	bool gotException = false;
	try {
		grp->add(item, false);
	} catch (invalid_argument e) {
		item->unref();
		gotException = true;
	}
	cppcut_assert_equal(true, gotException);
}

void test_addItemsWhenPreviousGroupIncompletion(void)
{
	x_table = new ItemTable();
	ItemGroup *grp = x_table->addNewGroup();
	grp->add(new ItemInt(ITEM_ID_0, 500), false);
	grp->add(new ItemString(ITEM_ID_1, "foo"), false);

	// add second group
	grp = x_table->addNewGroup();
	grp->add(new ItemInt(ITEM_ID_0, 200), false);

	// add thrid group
	bool gotException = false;
	ItemData *item = new ItemInt(ITEM_ID_0, -5);
	try {
		grp->add(item, false);
	} catch (invalid_argument e) {
		item->unref();
		gotException = true;
	}
	cppcut_assert_equal(true, gotException);
}

void test_crossJoin(void)
{
	x_table = addItems<TableStruct0>(tableContent0, NUM_TABLE0,
	                                 addItemTable0);

	y_table = addItems<TableStruct1>(tableContent1, NUM_TABLE1,
	                                 addItemTable1);

	z_table = x_table->crossJoin(y_table);

	// check the result

}

} // namespace testItemTable