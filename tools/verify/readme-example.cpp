#include <tessera/engine.hpp>
#include <tessera/history.hpp>
#include <cstdio>

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "/tmp/tessera-readme-example.tess";
    auto db = tessera::DB::create(tessera::make_in_realm_history(), path);

    {
        auto wt = db->start_write();
        auto table = wt->add_table("Person");
        auto name = table->add_column(tessera::type_String, "name");
        auto age  = table->add_column(tessera::type_Int, "age");
        table->create_object().set(name, "Ada").set(age, 36);
        wt->commit();
    }

    auto rt = db->start_read();
    auto table = rt->get_table("Person");
    auto results = table->where().greater(table->get_column_key("age"), 30).find_all();
    std::printf("found %zu\n", results.size());
    return 0;
}
