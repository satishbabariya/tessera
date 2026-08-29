#include <tessera/engine.hpp>
#include <tessera/history.hpp>
#include <cstdio>

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "/tmp/tessera-readme-example.tess";
    auto db = tessera::DB::create(tessera::make_in_realm_history(), path);

    {
        auto wt = db->start_write();
        if (!wt->has_table("Person")) {
            auto t = wt->add_table("Person");
            t->add_column(tessera::type_String, "name");
            t->add_column(tessera::type_Int, "age");
        }
        auto t = wt->get_table("Person");
        t->create_object()
            .set(t->get_column_key("name"), "Ada")
            .set(t->get_column_key("age"), 36);
        wt->commit();
    }

    auto rt = db->start_read();
    auto table = rt->get_table("Person");
    auto results = table->where().greater(table->get_column_key("age"), 30).find_all();
    std::printf("found %zu\n", results.size());
    return 0;
}
