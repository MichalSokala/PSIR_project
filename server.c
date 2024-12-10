#include <stdio.h>
#include <stdlib.h>



int main() {
    const char *list[2];
    list[0] = "H";
    list[1] = "T";

    for (int i = 0; i < 20; i++) {
        int rand_index = rand() % 2;
        printf("rand element: %s\n",list[rand_index]);
    }
}