#define UNMAKE_IMPLEMENTATION
#include "unmake.h"

void echo_cat_txt(void)
{
    um_cmd("cat", "cat.txt");
}

void build_cat_txt(void)
{
    um_cmdf("cat.txt", "echo", "Hello nation!");
}

int main(int argc, char **argv)
{
    um_init(argc, argv);

    um_recipe(echo_cat_txt, "any", "cat.txt", "other");
    um_recipe(echo_cat_txt, "other", "cat.txt");
    um_recipe(build_cat_txt, "cat.txt");

    um_run();

    return EXIT_SUCCESS;
}
