#pragma once


/*------------------------------
	CRASH : 인위적으로 크래시를 냄
--------------------------------*/
#define CRASH(cause)					 \
{										 \
	uint32* crash = nullptr;			 \
	__analysis_assume(crash != nullptr); \
	*crash = 0xDEADBEEF;				 \
}


#define ASSERT_CRASH(expr)			\
{									\
	if (!(expr))					\
	{								\
		CRASH("ASSERT_CRASH")		\
		__analysis_assume(expr);	\
	}								\
}

