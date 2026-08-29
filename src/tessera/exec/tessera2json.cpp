#include <tessera.hpp>
#include <tessera/sync/noinst/client_history_impl.hpp>
#include <iostream>

const char* legend =
    "Simple tool to output the JSON representation of a database:\n"
    "  tessera2json [--output-mode N] [--filter <filterexpr>] <.tess file>\n"
    "\n"
    "Options:\n"
    " --schema: Just output the schema of the realm\n"
    " --output-mode: Optional formatting for the output \n"
    "      0 - JSON Object\n"
    "      1 - MongoDB Extended JSON (XJSON)\n"
    "      2 - An extension of XJSON that adds wrappers for embdded objects, links, dictionaries, etc\n"
    " --filter: Only output objects matching the filter. Filter syntax: '<table name>:<RQL filter expression>'"
    "\n";

template <typename FormatStr>
void abort_if(bool cond, FormatStr fmt)
{
    if (!cond) {
        return;
    }

    fputs(fmt, stderr);
    std::exit(1);
}

template <typename FormatStr, typename... Args>
void abort_if(bool cond, FormatStr fmt, Args... args)
{
    if (!cond) {
        return;
    }

    fprintf(stderr, fmt, args...);
    std::exit(1);
}

int main(int argc, char const* argv[])
{
    bool output_schema = false;
    tessera::JSONOutputMode output_mode = tessera::output_mode_json;

    abort_if(argc <= 1, legend);
    std::string table_filter, query_filter;
    // Parse from 1'st argument until before source args
    for (int idx = 1; idx < argc - 1; ++idx) {
        tessera::StringData arg(argv[idx]);
        if (arg == "--schema") {
            output_schema = true;
        }
        else if (arg == "--output-mode") {
            auto output_mode_val = strtol(argv[++idx], nullptr, 0);
            abort_if(output_mode_val > 2, "Received unknown value for output_mode option: %d", output_mode_val);

            switch (output_mode_val) {
                case 0: {
                    output_mode = tessera::output_mode_json;
                    break;
                }
                case 1: {
                    output_mode = tessera::output_mode_xjson;
                    break;
                }
                case 2: {
                    output_mode = tessera::output_mode_xjson_plus;
                    break;
                }
            }
        }
        else if (arg == "--filter") {
            std::string filter_val = argv[++idx];
            auto sep = filter_val.find(":");
            abort_if(filter_val.size() < 3, "Expected filter of form 'class_Name:query'");
            abort_if(sep == std::string::npos, "Expected filter of form 'class_Name:query'");
            table_filter = filter_val.substr(0, sep);
            query_filter = filter_val.substr(sep + 1);
        }
        else {
            abort_if(true, "Received unknown option '%s' - please see description below\n\n%s", argv[idx], legend);
        }
    }

    std::string path = argv[argc - 1];

    auto print = [&](tessera::TransactionRef tr) {
        if (output_schema) {
            tr->schema_to_json(std::cout);
        }
        else if (table_filter.size()) {
            tessera::TableRef target = tr->get_table(table_filter);
            abort_if(!target, "table not found: '%s'", table_filter.c_str());
            tessera::Query q = target->query(query_filter);
            tessera::TableView results = q.find_all();
            std::cout << tessera::util::format("filter '%1' found %2 results", query_filter, results.size())
                      << std::endl;
            results.to_json(std::cout, output_mode);
        }
        else {
            tr->to_json(std::cout, output_mode);
        }
    };

    auto hist = tessera::make_in_realm_history();
    tessera::DBOptions options;
    // First we try to open in read-only mode.
    options.is_immutable = true;

    for (;;) {
        try {
            auto db = tessera::DB::create(*hist, path, options);
            if (!options.is_immutable) {
                std::cerr << "History schema upgraded: " << path << std::endl;
            }
            print(db->start_read());
            return 0;
        }
        catch (const tessera::IncompatibleHistories&) {
            hist = tessera::sync::make_client_replication();
            options.is_immutable = true;
        }
        catch (const tessera::FileAccessError& e) {
            if (e.code() != tessera::ErrorCodes::FileFormatUpgradeRequired) {
                throw;
            }
            // Tessera: file formats are never upgraded -- a non-current file is
            // rejected outright. This error now means the *history schema* needs
            // upgrading, which requires opening the file writable. The former
            // allow_file_format_upgrade flag is gone; writability is the whole
            // mechanism. Guard against looping if reopening does not help.
            if (!options.is_immutable) {
                throw;
            }
            options.is_immutable = false;
        }
    }

    return 0;
}
