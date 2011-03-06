//
// This file is a part of Biceps.
// See the file COPYRIGHT for the copyright notice and license information
//
//
// Test of aggregations.

#include <utest/Utest.h>
#include <string.h>

#include <type/AllTypes.h>
#include <common/StringUtil.h>
#include <table/Table.h>
#include <table/HashedIndex.h>
#include <type/BasicAggregatorType.h>
#include <table/BasicAggregator.h>
#include <mem/Rhref.h>

// Make fields of all simple types
void mkfields(RowType::FieldVec &fields)
{
	fields.clear();
	fields.push_back(RowType::Field("a", Type::r_uint8, 10));
	fields.push_back(RowType::Field("b", Type::r_int32,0));
	fields.push_back(RowType::Field("c", Type::r_int64));
	fields.push_back(RowType::Field("d", Type::r_float64));
	fields.push_back(RowType::Field("e", Type::r_string));
}

uint8_t v_uint8[10] = "123456789";
int32_t v_int32 = 1234;
int64_t v_int64 = 0xdeadbeefc00c;
double v_float64 = 9.99e99;
char v_string[] = "hello world";

void mkfdata(FdataVec &fd)
{
	fd.resize(4);
	fd[0].setPtr(true, &v_uint8, sizeof(v_uint8));
	fd[1].setPtr(true, &v_int32, sizeof(v_int32));
	fd[2].setPtr(true, &v_int64, sizeof(v_int64));
	fd[3].setPtr(true, &v_float64, sizeof(v_float64));
	// test the constructor
	fd.push_back(Fdata(true, &v_string, sizeof(v_string)));
}

Onceref<TableType> mktabtype(Onceref<RowType> rt)
{
	return (new TableType(rt))
			->addIndex("primary", (new HashedIndexType(
				(new NameSet())->add("b")
				))->addNested("level2", new HashedIndexType(
					(new NameSet())->add("c")
				)
			)
		);
}

// an "aggregator" that records the history of calls
Erref aggHistory(new Errors);
void recordHistory(Table *table, AggregatorGadget *gadget, Index *index,
        const IndexType *parentIndexType, GroupHandle *gh,
		Aggregator::AggOp aggop, Rowop::Opcode opcode, RowHandle *rh)
{
	aggHistory->appendMsg(false, strprintf("%s ao=%s op=%s count=%d",
		gadget->getName().c_str(), Aggregator::aggOpString(aggop),
		Rowop::opcodeString(opcode), (int)parentIndexType->groupSize(gh)));
}

UTESTCASE badName(Utest *utest)
{
	RowType::FieldVec fld;
	mkfields(fld);

	Autoref<Unit> unit = new Unit("u");
	Autoref<RowType> rt1 = new CompactRowType(fld);
	UT_ASSERT(rt1->getErrors().isNull());

	// Adds aggregators with names duplicating the embedded gadgets
	Autoref<TableType> tt = ( new TableType(rt1))
			->addIndex("primary", (new HashedIndexType(
				(new NameSet())->add("b")
				))->addNested("level2", (new HashedIndexType(
						(new NameSet())->add("c")
					))->setAggregator(
						new BasicAggregatorType("in", rt1, recordHistory)
					)
				)->setAggregator(
					new BasicAggregatorType("out", rt1, recordHistory)
				)
			);

	UT_ASSERT(tt);
	tt->initialize();
	UT_ASSERT(!tt->getErrors().isNull());
	UT_ASSERT(tt->getErrors()->hasError());
	UT_IS(tt->getErrors()->print(), "duplicate aggregator/label name 'out'\nduplicate aggregator/label name 'in'\n");
}

UTESTCASE tableops(Utest *utest)
{
	RowType::FieldVec fld;
	mkfields(fld);

	Autoref<Unit> unit = new Unit("u");
	Autoref<RowType> rt1 = new CompactRowType(fld);
	UT_ASSERT(rt1->getErrors().isNull());

	// same as mktabtype but adds an aggregator to count collapses
	Autoref<TableType> tt = ( new TableType(rt1))
			->addIndex("primary", (new HashedIndexType(
				(new NameSet())->add("b")
				))->addNested("level2", (new HashedIndexType(
						(new NameSet())->add("c")
					))->setAggregator(
						new BasicAggregatorType("onLevel2", rt1, recordHistory)
					)
				)->setAggregator(
					new BasicAggregatorType("onPrimary", rt1, recordHistory)
				)
			);

	aggHistory = new Errors;

	UT_ASSERT(tt);
	tt->initialize();
	UT_ASSERT(tt->getErrors().isNull());
	UT_ASSERT(!tt->getErrors()->hasError());

	Autoref<Table> t = tt->makeTable(unit, Table::SM_IGNORE, "t");
	UT_ASSERT(!t.isNull());

	IndexType *prim = tt->findIndex("primary");
	UT_ASSERT(prim != NULL);

	IndexType *sec = prim->findNested("level2");
	UT_ASSERT(sec != NULL);

	// above here was a copy of primaryIndex()

	// create a matrix of records, across both axes of indexing

	RowHandle *iter, *iter2;
	Fdata v1, v2;
	int i;
	FdataVec dv;
	mkfdata(dv);

	int32_t one32 = 1, two32 = 2;
	int64_t one64 = 1, two64 = 2;

	dv[1].data_ = (char *)&one32; dv[2].data_ = (char *)&one64;
	Rowref r11(rt1,  rt1->makeRow(dv));
	Rhref rh11(t, t->makeRowHandle(r11));

	dv[1].data_ = (char *)&one32; dv[2].data_ = (char *)&two64;
	Rowref r12(rt1,  rt1->makeRow(dv));
	Rhref rh12(t, t->makeRowHandle(r12));

	dv[1].data_ = (char *)&two32; dv[2].data_ = (char *)&one64;
	Rowref r21(rt1,  rt1->makeRow(dv));
	Rhref rh21(t, t->makeRowHandle(r21));

	dv[1].data_ = (char *)&two32; dv[2].data_ = (char *)&two64;
	Rowref r22(rt1,  rt1->makeRow(dv));
	Rhref rh22(t, t->makeRowHandle(r22));

	// so far the table must be empty
	iter = t->begin();
	UT_IS(iter, NULL);

	// basic insertion
	aggHistory->appendMsg(false, "+insert 11");
	UT_ASSERT(t->insert(rh11));
	aggHistory->appendMsg(false, "+insert 22");
	UT_ASSERT(t->insert(rh22));
	aggHistory->appendMsg(false, "+insert 12");
	UT_ASSERT(t->insert(rh12));
	aggHistory->appendMsg(false, "+insert 21");
	UT_ASSERT(t->insert(rh21));

	// see that they can be found by index
	iter = t->find(sec, rh11);
	UT_IS(iter, rh11);
	iter = t->find(sec, rh12);
	UT_IS(iter, rh12);
	iter = t->find(sec, rh21);
	UT_IS(iter, rh21);
	iter = t->find(sec, rh22);
	UT_IS(iter, rh22);

	// now must have 4 records, grouped by field b
	iter = t->begin();
	UT_ASSERT(iter != NULL);
	iter2 = t->next(iter);
	UT_ASSERT(iter2 != NULL);

		// check the grouping
		v1.setFrom(rt1, iter->getRow(), 1);
		v2.setFrom(rt1, iter2->getRow(), 1);
		UT_ASSERT(!memcmp(v1.data_, v2.data_, sizeof(int32_t)));

	iter = t->next(iter2);
	UT_ASSERT(iter != NULL);
	iter2 = t->next(iter);
	UT_ASSERT(iter2 != NULL);

		// check the grouping
		v1.setFrom(rt1, iter->getRow(), 1);
		v2.setFrom(rt1, iter2->getRow(), 1);
		UT_ASSERT(!memcmp(v1.data_, v2.data_, sizeof(int32_t)));

	iter = t->next(iter2);
	UT_IS(iter, NULL);

	// this should replace the row with an identical one but with auto-created handle
	aggHistory->appendMsg(false, "+replace 11");
	UT_ASSERT(t->insertRow(r11));
	// check that the old record is not in the table any more
	i = 0;
	iter2 = NULL;
	for (iter = t->begin(); iter != NULL; iter = t->next(iter)) {
		++i;
		UT_ASSERT(iter != rh11);

		// remember the record with the macthing key
		v1.setFrom(rt1, iter->getRow(), 1);
		v2.setFrom(rt1, iter->getRow(), 2);
		if (!memcmp(v1.data_, &one32, sizeof(int32_t)) 
		&& !memcmp(v2.data_, &one64, sizeof(int64_t)) )
			iter2 = iter;
	}
	UT_IS(i, 4);

	// check that the newly inserted record can be found by find on the same key
	iter = t->find(sec, rh11);
	UT_IS(iter, iter2);

	// check that search on a non-leaf index returns NULL
	iter = t->find(prim, rh11);
	UT_IS(iter, NULL);

	// check that iteration with NULL doesn't crash
	UT_ASSERT(t->next(NULL) == NULL);

	// and remove the remembered copy
	aggHistory->appendMsg(false, "+remove 11");
	t->remove(iter2);

	// check that the record is not there any more
	iter = t->find(sec, rh11);
	UT_ASSERT(iter == NULL);

	// check that now have 3 records
	i = 0;
	for (iter = t->begin(); iter != NULL; iter = t->next(iter)) {
		++i;
	}
	UT_IS(i, 3);

	// remove the 2nd record from the same group, collapsing it
	aggHistory->appendMsg(false, "+remove 12");
	t->remove(rh12);
	
	// check that now have 2 records
	i = 0;
	for (iter = t->begin(); iter != NULL; iter = t->next(iter)) {
		++i;
	}
	UT_IS(i, 2);

	// now check the history collected by the aggregators
	const char *expect = 
		// XXX this is not exactly right altogether but for now only remove ops are
		// expected to be proper
		"+insert 11\n"
		"t.onPrimary ao=BEFORE_MOD op=DELETE count=0\n"
		"t.onPrimary ao=AFTER_INSERT op=INSERT count=1\n"
		"t.onLevel2 ao=AFTER_INSERT op=INSERT count=1\n"
		"+insert 22\n"
		"t.onPrimary ao=BEFORE_MOD op=DELETE count=1\n"
		"t.onPrimary ao=AFTER_INSERT op=INSERT count=2\n"
		"t.onLevel2 ao=AFTER_INSERT op=INSERT count=1\n"
		"+insert 12\n"
		"t.onPrimary ao=BEFORE_MOD op=DELETE count=2\n"
		"t.onPrimary ao=AFTER_INSERT op=INSERT count=3\n"
		"t.onLevel2 ao=AFTER_INSERT op=INSERT count=2\n"
		"+insert 21\n"
		"t.onPrimary ao=BEFORE_MOD op=DELETE count=3\n"
		"t.onPrimary ao=AFTER_INSERT op=INSERT count=4\n"
		"t.onLevel2 ao=AFTER_INSERT op=INSERT count=2\n"
		"+replace 11\n"
		"t.onPrimary ao=BEFORE_MOD op=DELETE count=4\n"
		"t.onLevel2 ao=BEFORE_MOD op=DELETE count=2\n"
		"t.onPrimary ao=AFTER_DELETE op=NOP count=4\n"
		"t.onLevel2 ao=AFTER_DELETE op=NOP count=2\n"
		"t.onPrimary ao=AFTER_INSERT op=INSERT count=4\n"
		"t.onLevel2 ao=AFTER_INSERT op=INSERT count=2\n"
		"+remove 11\n"
		"t.onPrimary ao=BEFORE_MOD op=DELETE count=4\n"
		"t.onLevel2 ao=BEFORE_MOD op=DELETE count=2\n"
		"t.onPrimary ao=AFTER_DELETE op=INSERT count=3\n"
		"t.onLevel2 ao=AFTER_DELETE op=INSERT count=1\n"
		"+remove 12\n"
		"t.onPrimary ao=BEFORE_MOD op=DELETE count=3\n"
		"t.onLevel2 ao=BEFORE_MOD op=DELETE count=1\n"
		"t.onPrimary ao=AFTER_DELETE op=INSERT count=2\n"
		"t.onLevel2 ao=AFTER_DELETE op=INSERT count=0\n"
		"t.onLevel2 ao=COLLAPSE op=DELETE count=0\n"
	;
	string hist = aggHistory->print();
	UT_IS(hist, expect);
}