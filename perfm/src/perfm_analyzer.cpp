
#include "perfm_util.hpp"
#include "perfm_xml.hpp"
#include "perfm_analyzer.hpp"

#include <cstdio>
#include <cstdlib>

#include <stack>
#include <string>

namespace perfm {

bool analyzer_t::metric_parse(xml::xml_node<char> *metric)
{
    // parsing metric name (uniq & non-empty)
    xml::xml_attribute<char> *name = metric->first_attribute("name");

    if (!name || !name->value_size()) {
        perfm_warn("metric's name is empty, ignored\n");
        return false;
    }

    std::string metric_name(name->value(), name->value_size());
    std::string metric_formula;
    std::map<std::string, std::string> formula_alias;

    // parsing metric formula (non-empty)
    for (xml::xml_node<char> *node = metric->first_node(); node; node = node->next_sibling()) {
        const size_t sz_nam = node->name_size();
        const size_t sz_val = node->value_size();

        if (!sz_nam || !sz_val) {
            perfm_warn("invalid metric event/constant/formula/...\n");
            continue;
        }

        const std::string nod_nam(node->name(),  sz_nam);
        const std::string nod_val(node->value(), sz_val);

        if ("event" == nod_nam || "constant" == nod_nam) {
            xml::xml_attribute<char> *attr = node->first_attribute("alias"); 
            if (attr && attr->value_size()) {
                std::string att_val(attr->value(), attr->value_size());
                formula_alias.insert({att_val, nod_val});
            }
            continue;
        }
        
        if ("formula" == nod_nam) {
            metric_formula = nod_val;
            break;
        }
    }

    if (!metric_name.empty() && !metric_formula.empty() && !formula_alias.empty()) {
        this->metrics_list.push_back(metric_name);
        this->formula_list.insert({metric_name, {metric_formula, formula_alias}});
    } else {
        perfm_warn("metric's name, formula or formula alias may be empty\n");
        return false;
    }
   
    return true;
}

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

    for (xml::xml_node<char> *metric = perfm->first_node(); metric; metric = metric->next_sibling()) {
        metric_parse(metric);
    }

    free(xml_fil);
    return true;
}

void analyzer_t::metric_eval(const std::string &metric)
{
    auto iter = this->formula_list.find(metric);
    if (iter == this->formula_list.end()) {
        perfm_warn("invalid metric %s\n", metric.c_str());
        return;
    }

    const auto formula = iter->second.first;  // std::string
    const auto f_alias = iter->second.second; // std::map

    /* TODO */
}

std::string analyzer_t::formula_infix2postfix(const std::string &formula_infix) const
{
    if (formula_infix.empty()) {
        return "";
    } 

    /* TODO */
}

} /* namespace perfm */
