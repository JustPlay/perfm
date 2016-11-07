#include "perfm_xml.hpp"
#include "perfm_util.hpp"

#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
    void *file = perfm::read_file("xml.xml");

    rapidxml::xml_document<char> xml;        // root of the XML DOM tree

    try {
        xml.parse<0>(static_cast<char *>(file)); // parsing...
    } catch (rapidxml::parse_error &e) {
        printf("rapidxml: %s\n", e.what());
        exit(EXIT_FAILURE);
    }

    for (rapidxml::xml_node<char> *node = xml.first_node(); node; node = node->next_sibling()) {
        printf("|-->name:%s  value:%s\n", node->name(), node->value());

        for (rapidxml::xml_attribute<char> *attr = node->first_attribute(); attr; attr = attr->next_attribute()) {
            printf("-- attr_name: %s, attr_value: %s\n", attr->name(), attr->value());
        }

        for (rapidxml::xml_node<char> *sub_node = node->first_node(); sub_node; sub_node = sub_node->next_sibling()) {
            printf("|---->name:%s  value:%s\n", sub_node->name(), sub_node->value());

            for (rapidxml::xml_attribute<char> *attr = sub_node->first_attribute(); attr; attr = attr->next_attribute()) {
                printf("|---->attr_name: %s, attr_value: %s\n", attr->name(), attr->value());
            }

            for (rapidxml::xml_node<char> *sub_sub_node = sub_node->first_node(); sub_sub_node; sub_sub_node = sub_sub_node->next_sibling()) {
                printf("|------>name:%s  value:%s\n", sub_sub_node->name(), sub_sub_node->value());

                for (rapidxml::xml_attribute<char> *attr = sub_sub_node->first_attribute(); attr; attr = attr->next_attribute()) {
                    printf("|------>attr_name: %s, attr_value: %s\n", attr->name(), attr->value());
                }
            }
            printf("|\n");
        }

        printf("|\n");
    }

    printf("\n\n");

    return 0; 
}
