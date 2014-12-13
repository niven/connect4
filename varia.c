#include <stdio.h>

typedef struct foo {
	int a;
	float b;
} foo;

foo get_foo( int a ) {
	foo out = (struct foo){ .a = a, .b = 3.14f };
	return out;
}

int main() {

	foo f = get_foo(3);
	printf("%d, %f\n", f.a, f.b);
	f.a = 6;
	printf("%d, %f\n", f.a, f.b);
	foo g = f;
	printf("%d, %f\n", g.a, g.b);
	g.b = 2.1828f;
	printf("%d, %f\n", g.a, g.b);
	printf("%d, %f\n", f.a, f.b);

	printf("Done\n");

	return 0;
}