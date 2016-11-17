
#include <cstdio>
#include <cstdlib>

#include <stack>
#include <string>
#include <iostream>

#include <cstdio>
#include <cstdlib>

#include <vector>
#include <string>

#include <errno.h>

//
// http://www.cnblogs.com/caosiyang/archive/2012/08/21/2648870.html
//
// #define LOG(format, ...)     fprintf(stdout, format, ##__VA_ARGS__)
// #define LOG(format, args...) fprintf(stdout, format, ##args)
//

#define PERFM_BUFERR 256
#define PERFM_BUFSIZ 8192

#define program "perfm"

#ifndef NDEBUG

#define perfm_fatal(fmt, ...) do {                                                      \
    fprintf(stderr, "[%s, %d, %s()] %s: ", __FILE__, __LINE__, __FUNCTION__, program);  \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                                \
    exit(EXIT_FAILURE);                                                                 \
} while (0)

#define perfm_warn(fmt, ...) do {                                                       \
    fprintf(stderr, "[%s, %d, %s()] %s: ", __FILE__, __LINE__, __FUNCTION__, program);  \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                                \
} while (0)

#define perfm_info(fmt, ...) do {                                                       \
    fprintf(stdout, fmt, ##__VA_ARGS__);                                                \
} while (0)

#else  /* NDEBUG */

#define perfm_fatal(fmt, ...) do {                                                      \
    fprintf(stderr, program ": " fmt, ##__VA_ARGS__);                                   \
    exit(EXIT_FAILURE);                                                                 \
} while (0)

#define perfm_warn(fmt, ...) do {                                                       \
    fprintf(stderr, program ": " fmt, ##__VA_ARGS__);                                   \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                                \
} while (0)

#define perfm_info(fmt, ...) do {                                                       \
    fprintf(stdout, fmt, ##__VA_ARGS__);                                                \
} while (0)

#endif /* NDEBUG */

std::string formula_in2postfix(const std::string &formula_infix)
{
    if (formula_infix.empty()) {
        return "";
    } 

    enum {
        ELEM_error = 0,
        ELEM_operator,   // "*/%+-"
        ELEM_operation,  
        ELEM_parenthesis // "()"
    };

    size_t pos = 0;

    auto get_elem = [&pos] (const std::string &str, std::string &res) -> int {
        const size_t sz_str = str.size(); 

        while (pos < sz_str && std::isspace(str[pos])) { 
            ++pos;
        }

        res.clear();

        while (pos < sz_str) {
            switch (str[pos]) {
            case ' ':
                if (res.empty()) {
                    perfm_fatal("this should never happen\n");
                }

                return ELEM_operation;

            case '*':  case '/':  case '%':  case '+':  case '-':
                if (res.empty()) {
                    res = str[pos++];
                    return ELEM_operator;
                } else {
                    return ELEM_operation;
                }

            case '(':  case ')':
                if (res.empty()) {
                    res = str[pos++];
                    return ELEM_parenthesis;
                } else {
                    return ELEM_operation;
                }

            default:
                res += str[pos++];
            }
        }

        return res.empty() ? ELEM_error : ELEM_operation;
    };

    int priority[256] = { 0 };

    priority['('] = 0;
    priority[')'] = 0;

    priority['+'] = 2;
    priority['-'] = 2;

    priority['*'] = 4;
    priority['/'] = 4;
    priority['%'] = 4;

    auto less_priority = [&priority] (int a, int b) -> bool {
        return priority[a] < priority[b];
    };

    auto more_priority = [&priority] (int a, int b) -> bool {
        return priority[a] > priority[b];
    };

    std::string formula_postfix;
    std::string elem;
    std::stack<char> stk;

    auto str_append = [&formula_postfix] (const std::string &str) -> void {
        formula_postfix += formula_postfix.empty() ? str : " " + str;
    };

    int stat = get_elem(formula_infix, elem); 
    while (stat != ELEM_error) {
        switch (stat) {
        case ELEM_operation:
            str_append(elem);
            break;

        case ELEM_operator:
            while (!stk.empty() && !less_priority(stk.top(), elem[0])) {
                str_append(std::string(1, stk.top()));
                stk.pop();
            }
            stk.push(elem[0]);
            break;

        case ELEM_parenthesis:
            if ("(" == elem) {
                stk.push('('); 
            } else {
                while (!stk.empty() && stk.top() != '(') {
                    str_append(std::string(1, stk.top()));
                    stk.pop();
                }
                
                if (!stk.empty()) {
                    stk.pop();
                } else {
                    perfm_fatal("formula invalid %s\n", formula_infix.c_str());
                }
            }
            break;
        }

        stat = get_elem(formula_infix, elem);
    }

    while (!stk.empty()) {
        str_append(std::string(1, stk.top()));
        stk.pop();
    }

    return std::move(formula_postfix);
}

int main(int argc, char **argv)
{
    std::string formula;

    while (std::getline(std::cin, formula)) {
        std::cout << formula_in2postfix(formula).c_str() << std::endl << std::endl;
    }
}
