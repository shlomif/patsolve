%module rng

%include typemaps.i

%typemap(python,out) unsigned long
{
	$target = PyLong_FromUnsignedLong($source);
}

%name(seed) extern void seedMT(unsigned long seed);
%name(random) extern unsigned long randomMT(void);

int flip(long p, long q);
void torat(double x, long *T_OUTPUT, long *T_OUTPUT);

%{

static long gcd(long a, long b)
{
	long q;

	while (b != 0) {
		q = a % b;
		a = b;
		b = q;
	}
	return a;
}

void torat(double x, long *p, long *q)
{
	long i, g;
	double f;

	f = x;
	i = 1;
	while (i < (1<<30)) {
		if (fabs(floor(f + .5) - f) < 1e-6) {
			break;
		}
		f *= 10.;
		i *= 10;
	}
	x *= i;
	g = gcd((long)floor(x + .5), i);
	*p = (long)floor(x / g + .5);
	*q = (long)floor(i / g + .5);
}

/* Return true with probability p / q. */

int flip(long p, long q)
{
	unsigned long r;

	r = randomMT();
	return (r % q) < p;
}

%}
