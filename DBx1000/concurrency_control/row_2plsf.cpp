#include "row.h"
#include "txn.h"
#include "row_2plsf.h"
#include "mem_alloc.h"
#include "manager.h"
#include "2plsf.h"

/* rc can be RCOK, Abort, Wait */

void Row_2plsf::init(row_t * row) {
	_row = row;
}

// 'type' can be LOCK_SH or LOCK_EX
RC Row_2plsf::lock_get(lock_t type, txn_man * txn) {
    assert(this != nullptr);
    assert(_row != nullptr);
    if (type == LOCK_SH) {
        //if (twoplundodistsf::tryReadLock(_row, sizeof(Row_2plsf))) return RCOK;
        if (twoplsf::tryReadLock(_row, sizeof(uint64_t))) return RCOK; // TODO: temporary, should be ok
    } else {
        // type == LOCK_EX
        //if (twoplundodistsf::tryWriteLock(_row, sizeof(Row_2plsf))) return RCOK;
        if (twoplsf::tryWriteLock(_row, sizeof(uint64_t))) return RCOK; // TODO: temporary, should be ok
    }
    return Abort;
}


RC Row_2plsf::lock_release(txn_man * txn) {
	return RCOK;
}

