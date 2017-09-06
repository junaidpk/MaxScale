# MariaDB MaxScale 2.2.0 Release Notes

Release 2.2.0 is a Beta release.

This document describes the changes in release 2.2.0, when compared to
release 2.1.X.

For any problems you encounter, please consider submitting a bug
report at [Jira](https://jira.mariadb.org).

## Changed Features

### Whitespace in Object Names

Significant whitespace in object names is now deprecated. All object names
(services, servers, etc.) will be converted to a compatible format by
squeezing repeating whitespace and replacing it with hyphens. If any
object name conversions take place, a warning will be logged.

### Read-only Administrative Users

Users that can only perform read-only operations can be created with `add
readonly-user` and `enable readonly-account` commands.  To convert
administrative users to read-only users, delete the old administrative user and
create it as a read-only user.

For more information about administrative interface users, refer to the
[MaxAdmin](../Reference/MaxAdmin.md) documentation.

**Note:** Old users from pre-2.2 MaxScale versions will be converted to
administrative users that have full access to all commands.

### MaxAdmin

The `remove user` command now only expects one parameter, the username.

### Regular Expression Parameters

Modules may now use a built-in regular expression (regex) string parameter type
instead of a normal string when accepting patterns. The regex parameters are
checked by the config file loader to compile using the PCRE2 library embedded
within MaxScale. The only module using the new regex parameter type is currently
*QLAFilter*.

The only action users should take is enclose their regular expressions in
slashes, e.g. `match=/^select/` defines the pattern `^select`. The slashes allow
whitespace to be read from the ends of the regex string contrary to a normal
string parameter and are removed before compiling the pattern. For backwards
compatibility, the slashes are not yet mandatory. Omitting them is, however,
deprecated and will be rejected in the next release of MaxScale.

### `monitor_interval`

The default value of `monitor_interval` was changed from 10000 milliseconds to
2000 milliseconds.

### NamedServerFilter

The filter now accepts multiple match-server pairs. Please see the
[NamedServerFilter](../Filters/Named-Server-Filter.md) documentation for
details.

### Tee Filter

The `tee` filter has been rewritten to better suit the way MaxScale now
functions. The filter requires that the service where the branched session is
created has at least one network listener. The users must also be able to
connect from the local MaxScale host. Usually this means that an extra grant for
the loopback address is required (e.g. `myuser@127.0.0.1`).

In addition to the aforementioned requirements, a failure to create a branched
session no longer causes the actual client session to be closed. In most cases,
this is desired behavior.

The `match` and `exclude` parameters were changed to use PCRE2 syntax for the
regular expressions. The regular expression should be enclosed by slashes
e.g. `match=/select.*from.*test/`.

A tee filter instance can be disabled with the new `tee disable [FILTER]` and
`tee enable [FILTER]` module commands. Refer to the
[module command documentation](../Reference/Module-Commands.md) for more
details on module commands and the
[Tee Filter documentation](../Filters/Tee-Filter.md) for details on the tee
filter specific commands.

### Dbfwfilter

The `function` type rule will now match a query that does not use a function
when the filter is in whitelist mode (`action=allow`). This means that queries
that don't use functions are allowed though in whitelist mode.

### Logging

When known, the session id will be included in all logged messages. This allows
a range of logged messages related to a particular session (that is, client) to
be bound together, and makes it easier to investigate problems. In practice this
is visible so that if a logged message earlier looked like
```
2017-08-30 12:20:49   warning: [masking] The rule ...
```
it will now look like
```
2017-08-30 12:20:49   warning: (4711) [masking] The rule ...
```
where `4711` is the session id.

## Dropped Features

### MaxAdmin

The following deprecated commands have been removed:

* `enable log [debug|trace|message]`
* `disable log [debug|trace|message]`
* `enable sessionlog [debug|trace|message]`
* `disable sessionlog [debug|trace|message]`

The following commands have been deprecated:

* `enable sessionlog-priority <session-id> [debug|info|notice|warning]`
* `disable sessionlog-priority <session-id> [debug|info|notice|warning]`
* `reload config`

The `{ enable | disable } sessionlog-priority` commands can be issued, but they
have no effect.

#### Filenames as MaxAdmin Arguments

MaxAdmin no longer attempts to interpret additional command line parameters as a
file name to load commands from (e.g. `maxadmin mycommands.txt`). The shell
indirection operator `<` should be used to achieve the same effect (`maxadmin <
mycommands.txt`).

## New Features

### MySQL Monitor Crash Safety

The MySQL monitor keeps a journal of the state of the servers and the currently
elected master. This information will be read if MaxScale suffers an
uncontrolled shutdown. By doing the journaling of server states, the mysqlmon
monitor is able to keep track of stale master and stale slave states across
restarts and crashes.

### Avrorouter `deflate` compression

The Avrorouter now supports the `deflate` compression method. This allows the
stored Avro format files to be compressed on disk. For more information, refer
to the [Avrorouter](../Routers/Avrorouter.md) documentation.

### Preliminary proxy protocol support

The MySQL backend protocol module now supports sending a proxy protocol header
to the server. For more information, see the server section in the
[Configuration guide](../Getting-Started/Configuration-Guide.md).

### KILL command support

The MySQL client protocol now detects `KILL <thread_id>` statements (binary and
query forms) and kills the MaxScale session with the given id. This feature has
some limitations, see [Limitations](../About/Limitations.md) for more
information.

### New `uses_function` rule for dbfwfilter

The `uses_function` type rule prevents certain columns from being used
with functions. For more information about this new rule, read the
[dbfwfilter](../Filters/Database-Firewall-Filter.md) documentation.

## Bug fixes

[Here is a list of bugs fixed since the release of MaxScale 2.1.X.]()

## Known Issues and Limitations

There are some limitations and known issues within this version of MaxScale.
For more information, please refer to the [Limitations](../About/Limitations.md) document.

## Packaging

RPM and Debian packages are provided for the Linux distributions supported
by MariaDB Enterprise.

Packages can be downloaded [here](https://mariadb.com/resources/downloads).

## Source Code

The source code of MaxScale is tagged at GitHub with a tag, which is identical
with the version of MaxScale. For instance, the tag of version X.Y.Z of MaxScale
is X.Y.Z. Further, *master* always refers to the latest released non-beta version.

The source code is available [here](https://github.com/mariadb-corporation/MaxScale).