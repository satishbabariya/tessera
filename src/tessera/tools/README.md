# Table generators

Neither file here is part of the build, and that is correct. They are run by
hand, once, when a Unicode table in `src/tessera/unicode.cpp` needs
regenerating, and their output is pasted into it.

| | |
|---|---|
| `unicode_table_generator.cpp` | generates the `upper_lower` array in `unicode.cpp`. Reads a Unicode data file; see the comments at the top |
| `unicode.cpp` | generates the `lowers[]` array used by `unicode.cpp::to_lower()`. Mostly a comment holding the source data |

The second one shares a name with the file it generates a table *for*, which is
`src/tessera/unicode.cpp`. That one is the implementation and is compiled; this
one is a snippet and is not. The names were inherited and are kept so that the
comments in `unicode.cpp` referring to "this snippet" still resolve.

Both compile with a plain `c++ -std=c++20`, which is worth confirming before
relying on either: a generator that no longer builds means a generated table
that can no longer be regenerated, and nothing in the build would report it.
