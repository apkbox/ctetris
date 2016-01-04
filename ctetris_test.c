#include <stdio.h>

#define NOMAIN
#include "ctetris.c"

#define TEST(name)     int test__##name() {         \
            char *test_name__ = #name;              \
            int test_result__ = 1;                  \
            printf("[RUN     ] %s\n", test_name__);
        
#define END_TEST       {}                                   \
            if (test_result__)                              \
                printf("[      OK] %s\n", test_name__);     \
            else                                            \
                printf("[ FAILED ] %s\n", test_name__);     \
            return test_result__; }

#define RUN_TEST(name) test__##name()
#define ASSERT_EQ(a, b) {}                                          \
            {if((a)!=(b)) {                                         \
                printf("Assertion failed at %s(%d): %s != %s\n",    \
                    __FILE__, __LINE__, #a, #b);                    \
                test_result__ = 0;                                  \
            }}

TEST(swap_int) {
    int a = 1, b = 2;
    swap_int(a, b);
    ASSERT_EQ(a, 2);
    ASSERT_EQ(b, 1);
} END_TEST

TEST(max_int) {
    int max = max_int(10, 100);
    ASSERT_EQ(max, 100);
    max = max_int(100, 10);
    ASSERT_EQ(max, 100);
    max = max_int(100, 100);
    ASSERT_EQ(max, 100);
} END_TEST

TEST(collapse_rows) {
    int i;
    
    for (i = 0; i < (WIDTH * HEIGHT); ++i) {
        gameboard[i] = 42;
    }

    for (i = 0; i < HEIGHT; ++i) {
        gameboard[i * WIDTH] = i;
    }
    
    collapse_rows(4, 6);

    for (i = (HEIGHT - (6 - 4)); i < HEIGHT; ++i) {
        int c = gameboard[i * WIDTH];
        ASSERT_EQ(c, 0);
    }

    for (i = 4; i < (HEIGHT - 6 - (6 - 4)); ++i) {
        int c = gameboard[i * WIDTH];
        ASSERT_EQ(c, i + 2);
    }
    
    for (i = 0; i < 4; ++i) {
        int c = gameboard[i * WIDTH];
        ASSERT_EQ(c, i);
    }
} END_TEST

int main() {

    RUN_TEST(swap_int);
    RUN_TEST(max_int);
    RUN_TEST(collapse_rows);
    
/*
    int matrix[] = { 
        0, 0, 0, 0,
        8, 8, 8, 8,
        0, 0, 0, 0,
        0, 0, 0, 0
        };
    int x, y;
        
    for (y = 0; y < 4; ++y) {
        for (x = 0; x < 4; ++x) {
            printf("%d ", matrix[y * 4 + x]);
        }
        printf("\n");
    }
    
    transpose(matrix, 4, 4);
    mirror_y(matrix, 4, 4);

    printf("\n");

    for (y = 0; y < 4; ++y) {
        for (x = 0; x < 4; ++x) {
            printf("%d ", matrix[y * 4 + x]);
        }
        printf("\n");
    }
*/  
    return 0;
}
