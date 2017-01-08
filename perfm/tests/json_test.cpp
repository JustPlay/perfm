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
    } catch (const std::exception &e) {
        p_err("%s\n", e.what());
        exit(EXIT_FAILURE);
    }

    std::string _e_name; // EventName
    std::string _e_code; // EventCode
    std::string _e_umsk; // UMask
    std::string _e_cmsk; // CounterMask
    std::string _e_desc; // BriefDescription
    std::string _e_inv;  // Invert
    std::string _e_any;  // AnyThread
    std::string _e_edge; // EdgeDetect  
    std::string _e_prid; // SampleAfterValue
    
    for (property_tree::ptree::iterator it = ptree.begin(); it != ptree.end(); ++it) {
        try { 
            _e_name = it->second.get<std::string>("EventName");
            _e_code = it->second.get<std::string>("EventCode");  
            _e_umsk = it->second.get<std::string>("UMask");
            _e_cmsk = it->second.get<std::string>("CounterMask");
            _e_desc = it->second.get<std::string>("BriefDescription");
            _e_inv  = it->second.get<std::string>("Invert");
            _e_any  = it->second.get<std::string>("AnyThread");
            _e_edge = it->second.get<std::string>("EdgeDetect");
            _e_prid = it->second.get<std::string>("SampleAfterValue");
        } catch (const property_tree::ptree_bad_path &e) {
            p_err("%s\n", e.what());
            exit(EXIT_FAILURE);
        } catch (const std::exception &e) {
            p_err("%s\n", e.what());
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv)
{
    json_parser_test(argv[1]);
}
