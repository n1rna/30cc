#include "test_header.h"

int main()
{
    print_message("Hello from included function!");

    int array[MAX_SIZE];
    printf("Array size: %d\n", MAX_SIZE);

    int sum = add_numbers(5, 7);
    printf("5 + 7 = %d\n", sum);

    struct Point p;
    p.x = 10;
    p.y = 20;
    printf("Point: (%d, %d)\n", p.x, p.y);

    return 0;
}
