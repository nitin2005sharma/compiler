#include "cfg.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

bool is_generated_label(const std::string& label) {
    if (label.size() < 2 || label[0] != 'L') {
        return false;
    }

    for (size_t index = 1; index < label.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(label[index]))) {
            return false;
        }
    }

    return true;
}

std::string sanitize_id(const std::string& value) {
    std::string sanitized;
    for (char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            sanitized += ch;
        } else {
            sanitized += '_';
        }
    }
    return sanitized;
}

std::string escape_dot_label(const std::string& value) {
    std::string escaped;
    for (char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string entry_node_id(const std::string& function_name) {
    return "Entry_" + sanitize_id(function_name);
}

std::string exit_node_id(const std::string& function_name) {
    return "Exit_" + sanitize_id(function_name);
}

}  // namespace

void CFGBuilder::log(const std::string& message) const {
    if (trace != nullptr) {
        trace->log("CFG", message);
    }
}

CFG CFGBuilder::build(const IRProgram& ir) {
    CFG cfg;
    int n = static_cast<int>(ir.quads.size());
    if (n == 0) {
        log("build requested for empty IR");
        return cfg;
    }

    std::unordered_map<std::string, int> label_pos;
    for (int i = 0; i < n; ++i) {
        if (ir.quads[static_cast<size_t>(i)].op == "label") {
            label_pos[ir.quads[static_cast<size_t>(i)].res] = i;
            log("label " + ir.quads[static_cast<size_t>(i)].res + " at quad " + std::to_string(i));
        }
    }

    std::vector<int> leaders;
    leaders.push_back(0);
    for (int i = 0; i < n; ++i) {
        const auto& q = ir.quads[static_cast<size_t>(i)];
        if (q.op == "label") {
            leaders.push_back(i);
            log("leader from label at quad " + std::to_string(i));
        }
        if (q.op == "ifFalse" || q.op == "goto") {
            if (label_pos.count(q.res)) {
                leaders.push_back(label_pos[q.res]);
                log("leader from jump target " + q.res + " at quad " + std::to_string(label_pos[q.res]));
            }
            if (i + 1 < n) {
                leaders.push_back(i + 1);
                log("leader from fallthrough at quad " + std::to_string(i + 1));
            }
        }
    }

    std::sort(leaders.begin(), leaders.end());
    leaders.erase(std::unique(leaders.begin(), leaders.end()), leaders.end());

    std::unordered_map<int, int> lead_to_block;
    for (size_t bi = 0; bi < leaders.size(); ++bi) {
        BasicBlock block;
        block.id = static_cast<int>(bi);
        int start = leaders[bi];
        int end = (bi + 1 < leaders.size()) ? leaders[bi + 1] : n;
        for (int k = start; k < end; ++k) {
            block.quads.push_back(k);
            block.instructions.push_back(std::to_string(k) + ": " + ir.quads[static_cast<size_t>(k)].to_string());
        }
        cfg.blocks.push_back(block);
        lead_to_block[start] = block.id;
        log("block B" + std::to_string(block.id) + " covers quads " + std::to_string(start) + "-" + std::to_string(end - 1));
    }

    std::string current_function;
    std::unordered_map<std::string, size_t> function_index_by_name;
    for (auto& block : cfg.blocks) {
        if (!block.quads.empty()) {
            const auto& first_quad = ir.quads[static_cast<size_t>(block.quads.front())];
            if (first_quad.op == "label" && !is_generated_label(first_quad.res)) {
                current_function = first_quad.res;
                function_index_by_name[current_function] = cfg.functions.size();
                cfg.functions.push_back(FunctionFlow{current_function, block.id, {}});
                log("function " + current_function + " entry block B" + std::to_string(block.id));
            }
        }
        block.function_name = current_function;
    }

    auto resolve_block = [&](int quad_index) -> int {
        auto exact = lead_to_block.find(quad_index);
        if (exact != lead_to_block.end()) {
            return exact->second;
        }

        int best_leader = -1;
        for (const auto& entry : lead_to_block) {
            if (entry.first <= quad_index && entry.first > best_leader) {
                best_leader = entry.first;
            }
        }
        return best_leader >= 0 ? lead_to_block[best_leader] : -1;
    };

    for (const auto& block : cfg.blocks) {
        if (block.quads.empty()) {
            continue;
        }

        for (int quad_index : block.quads) {
            const auto& quad = ir.quads[static_cast<size_t>(quad_index)];
            if (quad.op == "call") {
                auto function_it = function_index_by_name.find(quad.a);
                if (function_it != function_index_by_name.end()) {
                    int target_block = cfg.functions[function_it->second].entry_block;
                    cfg.call_edges.push_back(CFGEdge{block.id, target_block, "call " + quad.a});
                    log("call edge B" + std::to_string(block.id) + " -> B" + std::to_string(target_block) + " (" + quad.a + ")");
                }
            }
        }

        int lastq = block.quads.back();
        const auto& q = ir.quads[static_cast<size_t>(lastq)];
        if (q.op == "ifFalse") {
            if (label_pos.count(q.res)) {
                int target = resolve_block(label_pos[q.res]);
                if (target >= 0) {
                    cfg.edges.push_back(CFGEdge{block.id, target, "false"});
                    log("edge B" + std::to_string(block.id) + " -> B" + std::to_string(target) + " (branch)");
                }
            }
            int fall = resolve_block(lastq + 1);
            if (lastq + 1 < n && fall >= 0) {
                cfg.edges.push_back(CFGEdge{block.id, fall, "true"});
                log("edge B" + std::to_string(block.id) + " -> B" + std::to_string(fall) + " (fallthrough)");
            }
        } else if (q.op == "goto") {
            if (label_pos.count(q.res)) {
                int target = resolve_block(label_pos[q.res]);
                if (target >= 0) {
                    cfg.edges.push_back(CFGEdge{block.id, target, "goto"});
                    log("edge B" + std::to_string(block.id) + " -> B" + std::to_string(target) + " (goto)");
                }
            }
        } else if (q.op == "ret") {
            log("block B" + std::to_string(block.id) + " exits via return");
            if (!block.function_name.empty()) {
                auto function_it = function_index_by_name.find(block.function_name);
                if (function_it != function_index_by_name.end()) {
                    auto& exits = cfg.functions[function_it->second].exit_blocks;
                    if (std::find(exits.begin(), exits.end(), block.id) == exits.end()) {
                        exits.push_back(block.id);
                    }
                }
            }
        } else {
            int fall = resolve_block(lastq + 1);
            if (lastq + 1 < n && fall >= 0) {
                cfg.edges.push_back(CFGEdge{block.id, fall, "next"});
                log("edge B" + std::to_string(block.id) + " -> B" + std::to_string(fall) + " (sequential)");
            }
        }
    }

    return cfg;
}

std::string CFG::to_dot() const {
    std::ostringstream os;
    os << "digraph CFG{graph[compound=true,ranksep=0.65,nodesep=0.5];\n";
    os << "node[shape=box,fontname=\"Consolas\"];\n";
    os << "edge[fontname=\"Consolas\"];\n";

    for (const auto& function : functions) {
        os << "subgraph cluster_" << sanitize_id(function.name) << "{\n";
        os << "label=\"Function " << escape_dot_label(function.name) << "\";\n";
        os << "color=\"#7aa6c2\";\n";
        os << "style=\"rounded\";\n";
        os << entry_node_id(function.name)
           << "[shape=oval,style=\"filled\",fillcolor=\"#d8f3dc\",label=\"Entry\\n"
           << escape_dot_label(function.name) << "\"];\n";

        for (const auto& block : blocks) {
            if (block.function_name != function.name) {
                continue;
            }
            os << "B" << block.id
               << "[style=\"rounded,filled\",fillcolor=\"#eaf4ff\",label=\"B" << block.id << "\\l";
            for (const auto& line : block.instructions) {
                os << escape_dot_label(line) << "\\l";
            }
            os << "\"];\n";
        }

        os << exit_node_id(function.name)
           << "[shape=oval,style=\"filled\",fillcolor=\"#f8d7da\",label=\"Exit\\n"
           << escape_dot_label(function.name) << "\"];\n";
        os << "}\n";
    }

    for (const auto& block : blocks) {
        if (!block.function_name.empty()) {
            continue;
        }
        os << "B" << block.id
           << "[style=\"rounded,filled\",fillcolor=\"#eaf4ff\",label=\"B" << block.id << "\\l";
        for (const auto& line : block.instructions) {
            os << escape_dot_label(line) << "\\l";
        }
        os << "\"];\n";
    }

    for (const auto& function : functions) {
        if (function.entry_block >= 0) {
            os << entry_node_id(function.name) << " -> B" << function.entry_block
               << "[label=\"entry\",color=\"#2a9d8f\",penwidth=1.4];\n";
        }
        for (int exit_block : function.exit_blocks) {
            os << "B" << exit_block << " -> " << exit_node_id(function.name)
               << "[label=\"ret\",color=\"#e76f51\",penwidth=1.4];\n";
        }
    }

    for (const auto& edge : edges) {
        os << "B" << edge.from << " -> B" << edge.to;
        if (!edge.label.empty()) {
            os << "[label=\"" << escape_dot_label(edge.label) << "\"]";
        }
        os << ";\n";
    }

    for (const auto& edge : call_edges) {
        std::string target_function;
        for (const auto& block : blocks) {
            if (block.id == edge.to) {
                target_function = block.function_name;
                break;
            }
        }
        std::string target = target_function.empty() ? "B" + std::to_string(edge.to) : entry_node_id(target_function);
        os << "B" << edge.from << " -> " << target
           << "[label=\"" << escape_dot_label(edge.label)
           << "\",style=\"dashed\",color=\"#457b9d\",constraint=false];\n";
    }

    os << "}\n";
    return os.str();
}
