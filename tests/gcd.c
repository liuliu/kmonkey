#include "case.h"

int gcd(int a, int b)
{
	int r;
	if (a == 0)
		r = b;
	while (b != 0)
	{
		if (a > b)
			a = a - b;
		else
			b = b - a;
	}
	r = a;
	return r;
}

TEST_CASE("gcd of 10 and 4")
{
	REQUIRE_EQ(2, gcd(10, 4), "");
}

TEST_CASE("gcd of 1 and 10")
{
	REQUIRE_EQ(1, gcd(1, 10), "");
}

TEST_CASE("gcd of 0 and 10")
{
	REQUIRE_EQ(10, gcd(0, 10), "");
}

#include "case_main.h"
