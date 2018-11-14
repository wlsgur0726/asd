#include "stdafx.h"
#include "test.h"
#include "asd/sysres.h"
#include "asd/util.h"
#include <stdio.h>
#include <thread>

void Test()
{
	return;
}



#if asd_Compiler_GCC
#include <sql.h>
#include "../../hiredis/hiredis.h"
void fix_undef_ref_error()
{
	::SQLAllocHandle(SQLSMALLINT(), SQLHANDLE(), NULL);
	::redisConnect("", 0);
}
#endif
