#pragma once
#include "2plsf.h"

class Row_2plsf {
public:
	void init(row_t * row);
    RC lock_get(lock_t type, txn_man * txn);
    RC lock_release(txn_man * txn);
	
private:
	row_t * _row;
};

