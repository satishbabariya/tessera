# `is_admin` returned true for a token that could not upload

`AccessControl::is_admin` decided administrator status like this:

```cpp
bool AccessControl::is_admin(const AccessToken& token) const noexcept
{
    if (token.admin_field)
        return token.admin;

    if (!token.path)          // <- a token with no path scope is an admin
        return true;

    // This will catch admins due to the way ROS makes access tokens.
    // It is not safe since it might be too liberal. This function will be
    // replaced as described above.
    if (token.access & (Privilege::ModifySchema | Privilege::SetPermissions))
        return true;

    return false;
}
```

`admin_field` is false unless the token carries an explicit `admin` or `isAdmin`
key, which none of the suite's fixtures do. So for an ordinary token the second
branch decides, and it grants administrator status to any token that is *not*
scoped to a path.

That is backwards. A path scope is a restriction; its absence is the ordinary
case, not a privilege. Run against the fixtures:

```
readonly (access=[download]):        path=<none>  admin_field=0  is_admin=1  can_upload=0
default  (access=[download,upload]): path=<none>  admin_field=0  is_admin=1  can_upload=1
for_path (path=/valid):              path=/valid  admin_field=0  is_admin=0  can_upload=0
```

The first line is the finding. A token whose entire grant is `["download"]` --
one that `can()` correctly refuses permission to upload anything anywhere --
was an administrator. The third line is the same fact from the other side: a
token that *is* scoped, and holds upload rights within its scope, was not.
`is_admin` ran opposite to privilege across the whole fixture set: seven of the
eight tokens are unscoped, so seven of eight were administrators, including the
one written to have the fewest rights possible.

## It was never called

Not by the server, not by a test, not by anything. The entire footprint was:

```
access_control.hpp:42:    bool is_admin(const AccessToken&) const noexcept;
access_control.cpp:83:    bool AccessControl::is_admin(const AccessToken& token) const noexcept
```

So this was not an exploitable defect. It was a loaded one: a function in the
authorization class, named for the strongest privilege the system has, that
would have granted that privilege to almost every token presented to it. Its
own comment says so -- "It is not safe since it might be too liberal" -- and
that comment sat unread for as long as nobody called the function.

## What was done

Deleted. Tessera implements no administrator concept anywhere, so there is
nothing to preserve; keeping a broken implementation of a feature that does not
exist only guarantees that the first person to need one finds this and uses it.
When an admin concept is wanted it should be designed against Tessera's token
format, not inherited from ROS's.

`AccessToken::admin` and `admin_field` stay. They are parsed token data, and the
parser is tested; what was wrong was the policy built on top of them.

## The general form

Dead code is normally a tidiness problem. In an authorization class it is a
different thing: the cost of being wrong is unbounded, and the usual signal that
something is wrong -- it misbehaves -- cannot fire, because nothing runs it.
This is the third file found in that state, after
[`tools/verify/clean-clone-test.sh`](0b-uncompiled-test-file.md) and
[`tools/pre-push`](0b-hooks-nobody-runs.md). All three were found by reading.
None of them could have been found by testing, and two of the three were wrong.
