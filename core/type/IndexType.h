//
// This file is a part of Biceps.
// See the file COPYRIGHT for the copyright notice and license information
//
//
// Type for creation of indexes in the tables.

#ifndef __Biceps_IndexType_h__
#define __Biceps_IndexType_h__

#include <type/Type.h>
#include <common/Errors.h>

namespace BICEPS_NS {

class IndexType;
class TableType;
class Index;
class Table;
class IndexVec;

// connection of indexes into a tree
class  IndexTypeRef 
{
public:
	IndexTypeRef(const string &n, IndexType *it);
	IndexTypeRef();
	// IndexTypeRef(const IndexTypeRef &orig); // the default one should be fine

	string name_; // name of the index, for finding it later
	Autoref<IndexType> index_;
};

class IndexTypeVec: public  vector<IndexTypeRef>
{
public:
	IndexTypeVec();
	IndexTypeVec(size_t size);
	// Populate with the copy of the original types
	IndexTypeVec(const IndexTypeVec &orig);

	// Initialize and validate all indexes in the vector.
	// The errors are returned through parent's getErrors().
	// Includes the checkDups().
	// @param table - table type where this index belongs
	// @param parentErr - parent's error collection, to append the
	//        indexes' errors
	void initialize(TableType *table, Erref parentErr);

	// Check for dups in names.
	// @param err - place to report the name dup errors
	// @return - true on success, false on error
	bool checkDups(Erref parentErr);

	// create the indexes for the types stored here
	// @param tabtype - table type where this index belongs
	// @param table - the actuall table instance where this index belongs
	// @param idx - the index vector to keep the created indexes
	void makeIndexes(const TableType *tabtype, Table *table, IndexVec *ivec) const;

private:
	void operator=(const IndexTypeVec &);
};

class IndexType : public Type
{
public:
	// subtype of index
	enum IndexId {
		IT_PRIMARY, // PrimaryIndexType
		IT_FIFO, // FifoIndexType
		// add new types here
		IT_LAST
	};

	// from Type
	virtual Erref getErrors() const; 
	virtual bool equals(const Type *t) const;
	virtual bool match(const Type *t) const;

	// The idea of the configuration methods is that they return back "this",
	// making possible to chain them together with "->".

	// Add a nested index under this one.
	// @param name - name of the nested index
	// @param index - the nested index
	// @return - this
	IndexType *addNested(const string &name, IndexType *index);

	// For access of subclasses to the subtype id.
	IndexId getIndexId() const
	{ 
		return indexId_; 
	}

	// Make a copy of this type. The copy is always uninitialized, no
	// matter whther it was made from an initialized one or not.
	// The subclasses must define the actual copying.
	virtual IndexType *copy() const = 0;

	// Initialize and validate.
	// If already initialized, must return right away.
	// The errors are returned through getErrors().
	// @param tabtype - table type where this index belongs
	virtual void initialize(TableType *tabtype) = 0;

	// Make a new instance of the index.
	// @param tabtype - table type where this index belongs
	// @param table - the actuall table instance where this index belongs
	// @return - the new instance, or NULL if not initialized or had an error.
	virtual Index *makeIndex(const TableType *tabtype, Table *table) const = 0;

protected:
	// can be constructed only from subclasses
	IndexType(IndexId it);
	IndexType(const IndexType &orig); 

	// wrapper to access friend-only data
	// @return - ind->nested_
	static IndexVec *getIndexVec(Index *ind);

protected:
	bool isInitialized() const
	{
		return initialized_;
	}

	IndexTypeVec nested_; // nested indices
	TableType *table_; // NOT autoref, to avoid reference loops
	IndexType *parent_; // NOT autoref, to avoid reference loops; NULL for top-level indexes
	IndexId indexId_; // identity in case if casting to subtypes is needed (should use typeid instead?)
	Erref errors_;
	bool initialized_; // flag: already initialized, no future changes

private:
	IndexType();
	void operator=(const IndexType &);
};

}; // BICEPS_NS

#endif // __Biceps_IndexType_h__