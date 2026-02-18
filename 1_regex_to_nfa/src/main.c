#include "regex.h"
#include "nfa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void print_postfix(regex r)
{
    for (int i = 0; i < r.size; i++)
    {
        printf("%c", r.items[i].value);
    }
    printf("\n");
}

void test_strings_stdin(const char *regex_str)
{
    regex r = parse_regex(regex_str);
    nfa n = regex_to_nfa(r);

    char buf[1024];
    while (fgets(buf, sizeof(buf), stdin))
    {
        buf[strcspn(buf, "\r\n")] = '\0';
        int result = match_nfa(n, buf, strlen(buf));
        printf("%d", result ? 1 : 0);
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    int opt;
    char regex_str[1024];

    while ((opt = getopt(argc, argv, "rt")) != -1)
    {
        switch (opt)
        {
            case 'r':
                if (!fgets(regex_str, sizeof(regex_str), stdin))
                    return 1;
                regex_str[strcspn(regex_str, "\r\n")] = '\0';
                print_postfix(parse_regex(regex_str));
                return 0;
            case 't':
                if (!fgets(regex_str, sizeof(regex_str), stdin))
                    return 1;
                regex_str[strcspn(regex_str, "\r\n")] = '\0';
                test_strings_stdin(regex_str);
                return 0;
            default:
                fprintf(stderr, "Usage: %s -r | -t\n", argv[0]);
                return 1;
        }
    }

    fprintf(stderr, "Usage: %s -r | -t\n", argv[0]);
    return 1;
}
