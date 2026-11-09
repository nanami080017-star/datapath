#include "resource_sharing.h"
USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN
struct XlsSharePass : public Pass {
    XlsSharePass() : Pass("opt_share", "datapath_logic XLS sharing adaptation") {}
    bool replace_existing_pass() const override { return true; }
    void run_register() override {
        auto original = pass_register.at(pass_name);
        Pass::run_register();
        pass_register.emplace("xls_share_original", original);
    }
    void help() override {
        log("\n    opt_share [-estimate|-rewrite-test] [selection]\n\n");
        log("Select the smallest negative area change among legal selector and sharing rewrites.\n");
        log("-estimate reports candidates; -rewrite-test applies the largest legal sharing group.\n");
    }

    static bool select_once(Design *design, Module *module, bool estimate, bool rewrite_test)
    {
        ModWalker walker(design, module);
        // A kept wire is observable for all candidates, even if it has no cell consumer.
        for (auto wire : module->wires())
            if (wire->get_bool_attribute(ID::keep))
                for (auto bit : walker.sigmap(wire))
                    if (bit.wire) walker.signal_outputs.insert(bit);

        xls_share::RewriteSearch search(walker);
        if (!rewrite_test) {
            xls_share::Visibility proof(walker);
            xls_share::collapse_candidates(design, module, proof, search);
            xls_share::lift_candidates(design, module, proof, search);
        }
        xls_share::Visibility visibility(walker);
        auto groups = xls_share::candidates(module, visibility, estimate || rewrite_test);
        log("XLS resource candidates=%d.\n", GetSize(groups));
        if (rewrite_test) {
            auto group = std::max_element(groups.begin(), groups.end(),
                [](const xls_share::Group &a, const xls_share::Group &b) { return a.size() < b.size(); });
            if (group == groups.end())
                return false;
            xls_share::make_share(module, visibility, *group)->apply(design, module);
            return true;
        }
        for (const auto &group : groups) {
            if (estimate) {
                log("XLS estimate group %d:", search.count);
                for (auto name : group)
                    log(" %s", log_id(name));
                log("\n");
            }
            search.offer(xls_share::make_share(module, visibility, group));
        }
        return !estimate && search.apply(design, module);
    }

    void execute(std::vector<std::string> args, Design *design) override {
        bool estimate = args.size() > 1 && args[1] == "-estimate";
        bool rewrite_test = args.size() > 1 && args[1] == "-rewrite-test";
        bool diagnostic = estimate || rewrite_test;
        extra_args(args, diagnostic ? 2 : 1, design);
        if (!diagnostic)
            Pass::call(design, "xls_share_original");
        for (auto module : design->selected_modules()) {
            if (diagnostic) {
                if (select_once(design, module, estimate, rewrite_test))
                    Pass::call_on_module(design, module, "opt_clean");
                continue;
            }
            while (xls_share::specialize_once(design, module))
                Pass::call_on_module(design, module, "opt_clean");
            while (select_once(design, module, false, false))
                Pass::call_on_module(design, module, "opt_clean");
        }
    }
} XlsSharePass;
PRIVATE_NAMESPACE_END
