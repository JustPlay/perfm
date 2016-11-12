
#include <cstdio>
#include <cstdlib>

#include "perfm_xml.hpp"
#include "perfm_util.hpp"
#include "perfm_analyzer.hpp"

namespace perfm {

namespace xml = rapidxml;

bool analyzer_t::metric_parse(const char *filp)
{
    void *xml_fil = read_file(filp);
    if (!xml_fil) {
        return false;
    }
    
    xml::xml_document<char> xml_doc; /* root of the XML DOM tree */

    try {
        xml_doc.parse<0>(static_cast<char *>(xml_fil));
    } catch (const xml::parse_error &e) {
        perfm_warn("%s\n", e.what()); 
        free(xml_fil);
        return false;
    }

    xml::xml_node<char> *perfm = xml_doc.first_node("perfm");
    if (!perfm) {
        perfm_warn("the root node must be <perfm></perfm>\n");    
        free(xml_fil);
        return false;
    }

    std::map<std::string, std::string> alias;
    xml::xml_node<char> *metric = perfm->first_node();

    while (metric) {
        xml::xml_attribute<char> *attr = metric->first_attribute("name");

        // parsing metric name (uniq & non-empty)
        if (!attr || !attr->value_size()) {
            metric = metric->next_sibling();
            continue;
        }

        alias.clear();

        std::string metric_name(attr->value(), attr->value_size());
        std::string metric_formula;

        const char *entry = "\0";

        // parsing metric formula (non-empty)
        for (xml::xml_node<char> *nod = metric->first_node(); nod; nod = nod->next_sibling()) {
            size_t sz_nam = nod->name_size();

            entry = "event";
            if (!std::strncmp(entry, nod->name(), sz_nam)) {
                xml::xml_attribute<char> *att = nod->first_attribute("alias"); 
                if (att && att->value_size() && nod->value_size()) {
                    alias.insert({att->value(), nod->value()});
                }
                continue;
            }
            
            entry = "constant";
            if (!std::strncmp(entry, nod->name(), sz_nam)) {
                xml::xml_attribute<char> *att = nod->first_attribute("alias"); 
                if (att && att->value_size() && nod->value_size()) {
                    alias.insert({att->value(), nod->value()});
                }
                continue;
            }
           
            entry = "formula";
            if (!std::strncmp(entry, nod->name(), sz_nam)) {
                // TODO
                continue;
            } 
        }

        if (!metric_name.empty() && !metric_formula.empty()) {
            this->metrics_list.push_back(metric_name);
            this->formula_list.insert({metric_name, {metric_formula, alias}});
        }
        
        metric = metric->next_sibling();
    }

    free(xml_fil);
    return true;
}

} /* namespace perfm */
