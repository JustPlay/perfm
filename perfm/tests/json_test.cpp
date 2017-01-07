#include <cstdio>
#include <cstdlib>

#include <string>
#include <iostream>
#include <fstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace property_tree = boost::property_tree;
namespace json          = property_tree::json_parser;

#define p_err(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define p_msg(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)

void json_parser_test(const char *json_filp)
{
    property_tree::ptree ptree; 

    try {
        json::read_json(json_filp, ptree);
    } catch (json::json_parser_error &e) {
        p_err("%s\n", e.what());
        exit(EXIT_FAILURE);
    }

    for (property_tree::ptree::value_type &e : ptree.get_child("")) {
        std::string event_code  = e.second.get<std::string>("EventCode");
        std::string event_umask = e.second.get<std::string>("UMask");
        std::string event_name  = e.second.get<std::string>("EventName");

        std::cout << event_name << " " << event_code << " " << event_umask << std::endl;
    }
}

int main(int argc, char **argv)
{
    json_parser_test(argv[1]);
}
