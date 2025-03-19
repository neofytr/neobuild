#include <stdio.h>
#include <stdlib.h>

int main()
{
    int arr[10] = {-1, 3, -6, 7, -9, 4, 8, -8, 5, 10};
    int suffix[10];

    int curr = 0;

    for (size_t index = 0; index < 10; index++)
    {
        curr += arr[index];
        suffix[index] = curr;
    }

    int max = suffix[0];
    int sub1 = 0;
    int min = suffix[0];
    int sub2 = 0;

    for (size_t index = 0; index < 10; index++)
    {
        if (suffix[index] > max)
        {
            max = suffix[index];
            sub1 = index;
        }
        if (suffix[index] < min)
        {
            min = suffix[index];
            sub2 = index;
        }
    }

    printf("%d\n", max - min);
    printf("%d %d\n", sub1, sub2);
    return EXIT_SUCCESS;
}