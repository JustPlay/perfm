
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

    std::string m_name(name->value(), name->value_size());
    std::string m_expr;
    std::map<std::string, evnam_set_t::iterator> f_alias;

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
                std::string alias_nam(attr->value(), attr->value_size());
                auto alias_val = this->_evnam.insert(nod_nam).first;
                f_alias.insert({alias_nam, alias_val});
            }
            continue;
        }
        
        if ("formula" == nod_nam) {
            m_expr = nod_val;
            break;
        }
    }

    if (!m_name.empty() && !m_expr.empty() && !f_alias.empty()) {
        this->metrics_list.push_back(m_name);
        this->formula_list.insert({m_name, {m_expr, f_alias}});
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

std::string analyzer_t::formula_in2postfix(const std::string &formula_infix) const
{
    if (formula_infix.empty()) {
        return "";
    } 

    enum {
        ERROR = 0,
        OPERATOR, // "*/%+-"
        OPERAND,  
        BRACKET   // "()"
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

                return OPERAND;

            case '*':  case '/':  case '%':  case '+':  case '-':
                if (res.empty()) {
                    res = str[pos++];
                    return OPERATOR;
                } else {
                    return OPERAND;
                }

            case '(':  case ')':
                if (res.empty()) {
                    res = str[pos++];
                    return BRACKET;
                } else {
                    return OPERAND;
                }

            default:
                res += str[pos++];
            }
        }

        return res.empty() ? ERROR : OPERAND;
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

    auto str_append = [&formula_postfix] (const std::string &str) mutable -> void {
        formula_postfix += formula_postfix.empty() ? str : " " + str;
    };

    int stat = get_elem(formula_infix, elem); 
    while (stat != ERROR) {
        switch (stat) {
        case OPERAND:
            str_append(elem);
            break;

        case OPERATOR:
            while (!stk.empty() && !less_priority(stk.top(), elem[0])) {
                str_append(std::string(1, stk.top()));
                stk.pop();
            }
            stk.push(elem[0]);
            break;

        case BRACKET:
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

double analyzer_t::formula_eval(const me_formula_t &formula) const
{
   /* FIXME */ 

    const auto  f_exprn = formula_in2postfix(formula.first); // std::string
    const auto &f_alias = formula.second;                    // std::map

    enum {
        ERROR = 0,
        OPERATOR,
        OPERAND
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
                return OPERAND;

            case '*':  case '/':  case '%':  case '+':  case '-':
                res = str[pos++];
                return OPERATOR;

            default:
                res += str[pos++];
            }
        }

        return res.empty() ? ERROR : OPERAND;
    };

    std::stack<double> stk;
    std::string elem;

    for (int stat = get_elem(f_exprn, elem); stat != ERROR; stat = get_elem(f_exprn, elem)) {
        switch (stat) {
        case OPERAND: {
                // fetch the real name (event name) for this alias
                auto nam = f_alias.find(elem);
                if (nam == f_alias.end()) {
                    perfm_fatal("invalid operand %s\n", elem.c_str());
                }

                // fetch data for the given event
                auto dat = _evdat.find(*nam->second);
                if (dat == _evdat.end()) {
                    perfm_fatal("invalid event name %s\n", nam->second->c_str());
                }

                /* FIXME */
                stk.push(std::get<0>(dat->second));
            }
            break;

        case OPERATOR: {
                if (stk.size() < 2) {
                    perfm_fatal("invalid formula expression %s\n", formula.first.c_str()); 
                }

                double r = stk.top(); stk.pop(); // right operand
                double l = stk.top(); stk.pop(); // left  operand
                double v = 0;

                switch (elem[0]) {
                case '*':
                    v = l * r;
                    break;

                case '/':
                    v = l / r;
                    break;

                case '+':
                    v = l + r;
                    break;

                case '-':
                    v = l - r;
                    break;

                default:
                    perfm_fatal("invalid operator %s\n", elem.c_str()); 
                }

                stk.push(v);
            }
            break;
        }
    }

    if (stk.empty()) {
        perfm_fatal("what ???\n");
    }

    return stk.top();
}

void analyzer_t::metric_eval(const std::string &metric)
{
    auto iter = this->formula_list.find(metric);
    if (iter == this->formula_list.end()) {
        perfm_warn("invalid metric %s\n", metric.c_str());
        return;
    }

    double val = formula_eval(iter->second);

    /* TODO */
}

void analyzer_t::metric_eval()
{
    for (auto iter = metrics_list.begin(); iter != metrics_list.end(); ++iter) {
        metric_eval(*iter);
    }
}

} /* namespace perfm */
