
#include "perfm_util.hpp"
#include "perfm_analyzer.hpp"

#include <cstdio>
#include <cstdlib>

#include <stack>
#include <string>

namespace perfm {

const std::string analyzer::_thread_view_file = "__perfm_thread_view_summary.csv";
const std::string analyzer::_core_view_file   = "__perfm_core_view_summary.csv";
const std::string analyzer::_socket_view_file = "__perfm_socket_view_summary.csv";
const std::string analyzer::_system_view_file = "__perfm_system_view_summary.csv";

metric::ptr_t metric::alloc() 
{
    metric *p = nullptr;
    
    try {
        p = new metric;
    } catch (const std::bad_alloc &) {
        p = nullptr;
    }

    return ptr_t(p);
}

void metric::events_parse(const std::string &filp)
{
    std::fstream fp(filp, std::ios::in);
    if (!fp.good()) {
        perfm_fatal("failed to open %s\n", filp.c_str());
    }

    std::string line;
    std::string evn;

    //
    // Performance Monitoring Events for Intel(R) Xeon(R)/Core Processor
    // https://download.01.org/perfmon/
    //

    while (std::getline(fp, line)) {
        if (line.empty()) {
            continue;
        }

        evn = str_trim(line);
        
        if (evn.empty() || evn[0] == '#' || evn[0] == ';') {
            continue;
        }

        // uncore PMU event 
        if (env.find("UNC_") != std::string::npos) {
            _ev_name.insert({env, PMU_UNCORE});
            continue;
        }

        // offcore PMU event
        if (env.find("OFFCORE_") != std::string::npos) {
            _ev_name.insert({evn, PMU_OFFCORE}); 
            continue;
        }

        // core PMU event
        _ev_name.insert({evn, PMU_CORE});
    }
}

void metric::metric_parse(const std::string &filp)
{
    void *xml_fil = read_file(filp.c_str());
    if (!xml_fil) {
        perfm_fatal("failed to read the metric file %s\n", filp.c_str());
    }
    
    xml::xml_document<char> xml_doc; /* root of the XML DOM tree */

    try {
        xml_doc.parse<0>(static_cast<char *>(xml_fil));
    } catch (const xml::parse_error &e) {
        perfm_fatal("%s\n", e.what()); 
        return;
    }

    xml::xml_node<char> *perfm = xml_doc.first_node("perfm");
    if (!perfm) {
        perfm_fatal("the root node must be <perfm></perfm>\n");
    }

    for (xml::xml_node<char> *metric = perfm->first_node(); metric; metric = metric->next_sibling()) {
        metric_parse(metric);
    }

    free(xml_fil);
}

bool metric::metric_parse(xml::xml_node<char> *m)
{
    // metric name (uniq & non-empty)
    xml::xml_attribute<char> *n = m->first_attribute("name");

    if (!n || !n->value_size()) {
        perfm_warn("metric must have a name\n");
        return false;
    }

    std::string m_name(n->value(), n->value_size());         // metric name
    std::string m_expr;                                      // metric expr
    std::map<std::string, _ev_name_map_t::iterator> e_alias; // metric expr alias

    // metric expr (non-empty)
    for (xml::xml_node<char> *nd = m->first_node(); nd; nd = nd->next_sibling()) {
        const size_t sz_nam = nd->name_size();
        const size_t sz_val = nd->value_size();

        if (!sz_nam || !sz_val) {
            perfm_warn("invalid metric event/constant/formula/..., skiped\n");
            continue;
        }

        const std::string nd_nam(nd->name(),  sz_nam); // namely "event", "constant", "forluma"
        const std::string nd_val(nd->value(), sz_val); // event's name, constant var, forluma

        if ("event" == nd_nam) {
            xml::xml_attribute<char> *attr = nd->first_attribute("alias"); 

            if (!attr || !attr->value_size()) {
                perfm_warn("invalid alias for %s, %s\n", nd_nam.c_str(), nd_val.c_str());
                continue;
            }

            std::string alias(attr->value(), attr->value_size());

            auto event = _ev_name.find(nd_val);
            if (event == _ev_name.end()) {
                perfm_warn("TODO\n");
            }

            e_alias.insert({alias, event});

            continue;
        }
        
        if ("constant" == nd_nam) {
            xml::xml_attribute<char> *attr = nd->first_attribute("alias"); 

            if (!attr || !attr->value_size()) {
                perfm_warn("invalid alias for %s, %s\n", nd_nam.c_str(), nd_val.c_str());
                continue;
            }
            
            std::string alias(attr->value(), attr->value_size());
            auto constant = _ev_name.insert({nd_val, PMU_CONSTANT});

            if (!constant.second && constant.first->second != PMU_CONSTANT) {
                perfm_warn("TODO\n");
            }

            e_alias.insert({alias, constant});

            continue;
        }

        if ("formula" == nd_nam) {
            m_expr = nd_val;
            break;
        }
    }

    if (!m_name.empty() && !m_expr.empty() && !e_alias.empty()) {
        this->_metrics_list.push_back(m_name);
        this->_formula_list.insert({m_name, {m_expr, e_alias}});
    } else {
        perfm_warn("metric's name, formula or formula alias may be empty\n");
        return false;
    }
   
    return true;
}

int metric::evn2type(const std::string &evn)
{
    auto it = _ev_name.find(evn);
    if (it != _ev_name.end()) {
        return it->second;
    }

    return PMU_UNKNOWN;
}

analyzer::ptr_t analyzer::alloc() 
{
    analyzer *p = nullptr;
    
    try {
        p = new analyzer;
    } catch (const std::bad_alloc &) {
        p = nullptr;
    }

    return ptr_t(p);
}

void analyzer::analyze()
{
    // 
    // build cpu topology
    //
    this->topology();

    //
    // parse event & metric list
    //
    this->_metric = metric::alloc();
    if (!this->_metric) {
        perfm_fatal("failed to alloc the metric object\n");
    }

    this->_metric->events_parse(/* TODO */);
    this->_metric->metric_parse(/* TODO */);

    //
    // gather the collected PMU events
    //
    this->collect();

    // 
    // compute & print
    //
    this->compute();
}

void analyzer::topology(const std::string &filp)
{
    for (size_t i = 0; i < NR_MAX_PROCESSOR; ++i) {
        _cpu_topology[i] = std::make_tuple(-1, -1, -1);    
    }

    _nr_thread = 0;
    _nr_core   = 0;
    _nr_socket = 0;
    _nr_system = 1;

    std::fstream fp;

    if (filp.empty()) {
        fp.open(perfm_options.sys_topology_filp, std::ios::in);
    } else {
        fp.open(filp, std::ios::in);
    }

    if (!fp.good()) {
        perfm_fatal("failed to open %s\n", filp.empty() ? perfm_options.sys_topology_filp.c_str() : filp.c_str());
    }

    const std::string title = "[Processor] - [Core] - [Socket] - [Online]";
    std::string line;

    while (std::getline(fp, line)) {
        if (line == title) {
            break;
        }
    }

    if (!fp.good()) {
        perfm_fatal("failed to read %s\n", filp.empty() ? perfm_options.sys_topology_filp.c_str() : filp.c_str());
    }

    int cpu, core, socket, online;
    while (fp >> cpu >> core >> socket >> online) {
        _cpu_topology[cpu] = std::make_tuple(online, core, socket); 
        ++_nr_thread;
    }

    std::set<int> skt;
    std::set<std::pair<int, int>> core;

    for (unsigned int c = 0, n = 0; n < _nr_thread; ++c) {
        if (processor_stauts(c) == -1) {
            continue;
        }
        ++n;

        int coreid = processor_coreid(c);
        int socket = processor_socket(c);

        if (!skt.count(socket)) {
            ++_nr_socket;
            skt.insert(socket);
        }

        if (!core.count({coreid, socket})) {
            ++_nr_core;
            core.insert({coreid, socket});
        }
    }
}

void analyzer::collect(const std::string &filp)
{
    std::fstream fp;

    if (filp.empty()) {
        fp.open(perfm_options.pmu_value_filp, std::ios::in);
    } else {
        fp.open(filp.c_str(), std::ios::in);
    }

    if (!fp.good()) {
        perfm_fatal("failed to open %s\n", filp.empty() ? perfm_options.pmu_value_filp.c_str() : filp.c_str());
    }

    const std::string delimiter = " ";
    std::string line;

    std::unordered_map<std::string, size_t> ev_count; // how many times one event has been counted

    // event_name, tsc_cycle, cpu0, cpu1, cpu2, ... (core PMU)
    // event_name, tsc_cycle, socket0, socket1, ... (uncore PMU)
    
    while (std::getline(fp, line)) {
        if (line.empty()) {
            continue;
        }

        auto slice = str_split(line, delimiter);

        std::string nam_event = std::move(slice[0]);
        uint64_t tsc_cycle    = slice[1];

        std::vector<double> pmu_value;

        bool is_valid = true;
        
        for (size_t i = 2; is_valid && i < slice.size(); ++i) {
            uint64_t val = 0;    
            try {
                val = std::stoull(slice[i]);
            } catch (const std::invalid_argument &) {
                is_valid = false;
            } catch (const std::out_of_range &) {
                is_valid = false;
            }

            if (!is_valid) {
                break;
            }

            /* FIXME */
            if (val > tsc_cycle) {
                is_valid = false;
                break;
            }

            // cpu0, cpu1, cpu2, ... (core PMU)
            // socket0, socket1, ... (uncore PMU)
            pmu_value.push_back(val);
        }

        if (!is_valid) {
            perfm_warn("invalid event sample %s\n", line.c_str()); 
            continue;
        }

        insert(nam_event, pmu_value);
    }

    average(ev_count);   
}

void analyzer::insert(const std::string &evn, const std::vector<double> &val)
{
    switch (this->_metric->evn2type(evn)) {
    case PMU_CORE:
    case PMU_OFFCORE:
        if (val.size() != this->_nr_thread) {
            perfm_fatal("core/offcore PMU event should be collected for each presented CPUs\n");
        }

        {
            auto it = this->_ev_thread.find(evn);
            if (it == this->_ev_thread.end()) {
                _ev_thrd_elem_t *p = nullptr;
                try {
                    p = new _ev_thrd_elem_t;
                } catch (const std::bad_alloc &) {
                    perfm_fatal("failed to alloc memory\n");
                }

                for (size_t c = 0, n = 0; n < val.size(); ++c) {
                    if (processor_status(c) == -1) {
                        continue;
                    }
                    ++n;

                    p[c] = val[n];
                }

                this->_ev_thread.insert({evn, std::shared_ptr<_ev_thrd_elem_t>(p)});

            } else {
                for (size_t c = 0, n = 0; n < val.size(); ++c) {
                    if (processor_status(c) == -1) {
                        continue;
                    }
                    ++n;

                    (*(it->second))[c] += val[n];
                }
            }
        }

        break;

    case PMU_UNCORE:
        if (val.size() != this->_nr_socket) {
            perfm_fatal("uncore PMU event should be collected for each presented sockets\n");
        }

        {
            auto it = this->_ev_socket.find(evn);
            if (it == this->_ev_socket.end()) {
                _ev_socket_elem_t *p = nullptr;
                try {
                    p = new _ev_socket_elem_t;
                } catch (const std::bad_alloc &) {
                    perfm_fatal("failed to alloc memory\n");
                }

                for (size_t i = 0; i < val.size(); ++i) {
                    p[i] = val[i];
                }

                this->_ev_socket.insert({evn, std::shared_ptr<_ev_socket_elem_t>(p)});

            } else {
                for (size_t i = 0; i < val.size(); ++i) {
                    (*(it->second))[i] += val[i];
                }
            }
        }

        break; 

    case PMU_CONSTANT:
        perfm_fatal("not impled\n");
        break;

    case PMU_UNKNOWN:
        perfm_fatal("not impled\n");
        break;

    default:
        perfm_fatal("not impled\n");
        ;
    }
}

void analyzer::average(const std::unordered_map<std::string, size_t> &ev_count)
{
    // core/offcore PMU events are related to each presented (logical) CPUs 
    for (auto it = _ev_thread.begin(); it != _ev_thread.end(); ++it) {
        size_t cntr = ev_count[it->first];

        for (unsigned int c = 0, n = 0; n < _nr_thread; ++c) {
            if (processor_status(c) == -1) {
                continue;
            }
            ++n;

            (*(it->second))[c] /= cntr;
        }
    }

    // uncore PMU events are related to socket (not CPUs)
    for (auto it = _ev_socket.begin(); it != _ev_socket.end(); ++it) {
        size_t cntr = ev_count[it->first];

        for (unsigned int c = 0, n = 0; n < _nr_socket; ++c) {
            if (processor_status(c) == -1) {
                continue;
            }
            ++n;

            (*(it->second))[c] /= cntr;
        }
    }
}

void analyzer::compute()
{
    if (perfm_options.thread_view) {
        perfm_fatal("TODO\n"); 
    }

    if (perfm_options.core_view) {
        perfm_fatal("TODO\n"); 
    }

    if (perfm_options.socket_view) {
        socket_compute();
    }

    if (perfm_options.system_view) {
        system_compute();
    }
}

void analyzer::thrd_compute()
{

    /* TODO */
}

void analyzer::core_compute()
{

}

void analyzer::socket_compute()
{
    // aggregate PMU data related to this socket
    for (auto it = _ev_thread.begin(); it != _ev_thread.end(); ++it) {
        _ev_socket_elem_t *p = nullptr;
        try {
            p = new _ev_socket_elem_t;
        } catch (const std::bad_alloc &) {
            perfm_fatal("failed to alloc memory\n"); 
        }

        for (unsigned int s = 0; s < NR_MAX_SOCKET; ++s) {
            p[s] = 0;
        }

        for (unsigned int c = 0, n = 0; n < _nr_thread; ++c) {
            if (processor_status(c) == -1) {
                continue;
            }
            ++n;
            
            p[processor_socket(c)] += (*(it->second))[c];
        }

        _ev_socket.insert({it->first, std::shared_ptr<_ev_socket_elem_t>(p)});
    }

    // parse the aggregated data
    /* TODO */
}

void analyzer::system_compute()
{
    if (perfm_options.socket_view) {
        for (auto it = _ev_socket.begin(); it != _ev_socket.end(); ++it) {
            _ev_system_elem_t *p = nullptr;
            try {
                p = new _ev_system_elem_t { 0 };
            } catch (const std::bad_alloc &) {
                perfm_fatal("failed to alloc memory\n"); 
            }

            for (unsigned int s = 0; s < _nr_socket; ++s) {
                p[0] += (*(it->second))[s];
            }

            _ev_system.insert({it->first, std::shared_ptr<_ev_system_elem_t>(p)});
        }
    
    } else {
        for (auto it = _ev_thread.begin(); it != _ev_thread.end(); ++it) {
            _ev_system_elem_t *p = nullptr;
            try {
                p = new _ev_system_elem_t { 0 };
            } catch (const std::bad_alloc &) {
                perfm_fatal("failed to alloc memory\n"); 
            }

            for (unsigned int c = 0, n = 0; n < _nr_thread; ++c) {
                if (processor_status(c) == -1) {
                    continue;
                }
                ++n;
                
                p[0] += (*(it->second))[c];
            }

            _ev_system.insert({it->first, std::shared_ptr<_ev_system_elem_t>(p)});
        }
    }

    /* TODO */
}

void analyzer::metric_eval()
{
    if (_metrics_list.empty()) {
        perfm_warn("no metric for evaluate\n");
        return;
    }

    for (auto iter = _metrics_list.begin(); iter != _metrics_list.end(); ++iter) {
        metric_eval(*iter);
    }
}

void analyzer::metric_eval(const _metric_nam_t &metric)
{
    auto iter = _formula_list.find(metric);
    if (iter == _formula_list.end()) {
        perfm_warn("invalid metric %s\n", metric.c_str());
        return;
    }

    double metric_val = expr_eval(iter->second);

    /* TODO (how to use/show/display this value) */
}

std::string analyzer::expr_in2postfix(const std::string &expr_infix) const
{
    if (expr_infix.empty()) {
        return "";
    } 

    enum {
        EXPR_ERROR = 0,
        EXPR_OPERATOR, // "*/%+-"
        EXPR_OPERAND,
        EXPR_BRACKET   // "()"
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

                return EXPR_OPERAND;

            case '*':  case '/':  case '%':  case '+':  case '-':
                if (res.empty()) {
                    res = str[pos++];
                    return EXPR_OPERATOR;
                } else {
                    return EXPR_OPERAND;
                }

            case '(':  case ')':
                if (res.empty()) {
                    res = str[pos++];
                    return EXPR_BRACKET;
                } else {
                    return EXPR_OPERAND;
                }

            default:
                res += str[pos++];
            }
        }

        return res.empty() ? EXPR_ERROR : EXPR_OPERAND;
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

    std::string expr_postfix;
    std::string elem;
    std::stack<char> stk;

    auto str_append = [&expr_postfix] (const std::string &str) mutable -> void {
        expr_postfix += expr_postfix.empty() ? str : " " + str;
    };

    for (int stat = get_elem(expr_infix, elem); stat != EXPR_ERROR; stat = get_elem(expr_infix, elem)) {
        switch (stat) {
        case EXPR_OPERAND:
            str_append(elem);
            break;

        case EXPR_OPERATOR:
            while (!stk.empty() && !less_priority(stk.top(), elem[0])) {
                str_append(std::string(1, stk.top()));
                stk.pop();
            }
            stk.push(elem[0]);
            break;

        case EXPR_BRACKET:
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
                    perfm_fatal("formula invalid %s\n", expr_infix.c_str());
                }
            }
            break;
        }
    }

    while (!stk.empty()) {
        str_append(std::string(1, stk.top()));
        stk.pop();
    }

    return std::move(expr_postfix);
}

double analyzer::expr_eval(const _expression_t &formula, size_t column) const
{
    const auto  exprn = expr_in2postfix(formula.first); // std::string
    const auto &alias = formula.second;                 // std::map

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

        // each elem (operator & operand) in the postfix expr was separated by a ' '
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

    for (int stat = get_elem(exprn, elem); stat != ERROR; stat = get_elem(exprn, elem)) {
        switch (stat) {
        case OPERAND: {
                // fetch the real name (event name) for this alias
                auto name = alias.find(elem);
                if (name == alias.end()) {
                    perfm_fatal("invalid operand %s\n", elem.c_str());
                }

                // fetch data for the given event
                auto data = _ev_data.find(name->second->first);
                if (data == _ev_data.end()) {
                    perfm_fatal("invalid event name %s\n", name->second->first.c_str());
                }

                /* FIXME */
                stk.push(data->second[column]);
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
        perfm_fatal("invalid formula expression %s\n", formula.first.c_str());
    }

    return stk.top();
}

} /* namespace perfm */
