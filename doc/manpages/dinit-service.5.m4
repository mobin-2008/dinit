changequote(`@@@',`$$$')dnl
@@@.TH DINIT-SERVICE "5" "$$$MONTH YEAR@@@" "Dinit $$$VERSION@@@" "Dinit \- service management system"
.SH NAME
Dinit service description files
.\"
.SH SYNOPSIS
.\"
.ft CR
/etc/dinit.d/\fIservice-name\fR, $XDG_CONFIG_HOME/dinit.d/\fIservice-name\fR
.ft
.\"
.SH DESCRIPTION
.\"
The service description files for \fBDinit\fR each describe a service. The name
of the file corresponds to the name of the service it describes (minus its argument;
see \fBSERVICE ARGUMENTS\fR). 
.LP
Service description files specify the various attributes of a service.
A service description file is a plain-text file with simple key-value format.
The description files are located in a service description directory;
See \fBdinit\fR(8) for more details of the default service description directories,
and how and when service descriptions are loaded.
.LP
All services have a \fItype\fR and a set of \fIdependencies\fR.
These are discussed in the following subsections.
The type, dependencies, and other attributes are specified via property settings, the format of
which are documented in the \fBSERVICE PROPERTIES\fR subsection, which also lists the available
properties.
.LP
In addition to service properties, some meta-commands can be used within service
description files.
See the \fBMETA-COMMANDS\fR subsection for more information. 
.\"
.SS SERVICE ARGUMENTS
.\"
A service description may act as a template for multiple related services.
The full name of a service can include an argument, following the base name of the service
suffixed by an 'at' symbol (\fB@\fR), in the form \fIbase-name@argument\fR.
The argument value may be substituted into various service setting values by using
a '\fB$1\fR' marker in the value specified in the service description (see \fBVARIABLE SUBSTITUTION\fR).
The full name (base name together with argument) uniquely identifies a service instance, and each
instance is loaded separately, remaining independent of other instances.
.\"
.SS SERVICE TYPES
.\"
There are five basic types of service:
.IP \(bu
\fBProcess\fR services. This kind of service runs a single supervised process; the process
is started when the service is started and stopped when the service is stopped. If the
process stops this also affects the service state, i.e. the service's started/stopped state is
linked to the state of its associated process.
.IP \(bu
\fBBgprocess\fR services ("background process" services).
This kind of service is similar to a regular process service, but is for a process which
"daemonizes" or otherwise forks from the original process which starts it, and writes its
new process ID to a file.
Dinit will read the process ID from the file and, if running as the system init process or if the
system provides the necessary facilities, can supervise the process just as for a \fBprocess\fR
service.
When starting a \fBbgprocess\fR service, Dinit will not consider the service to be fully started
until the original process forks and terminates.
.IP \(bu
\fBScripted\fR services are services which are started and stopped by executing commands (which
need not actually be scripts, despite the name).
Once a command completes successfully the service is considered started (or stopped, as appropriate)
by Dinit.
.IP \(bu
\fBInternal\fR services do not run as an external process at all.
They can be started and stopped without any external action.
They are useful for grouping other services (via service dependencies).
.IP \(bu
\fBTriggered\fR services are similar to internal processes, but an external trigger is required
before they will start (i.e. Dinit will not consider them as started until the trigger is issued).
The \fBdinitctl trigger\fR command can be used to trigger such a service; see \fBdinitctl\fR(8).
.LP
Independent of their type, the state of services can be linked to other
services via dependency relationships, which are discussed in the next section.
.\"
.SS SERVICE DEPENDENCIES
.\"
A service dependency relationship, broadly speaking, specifies that for one
service to run, another must also be running; when starting a service Dinit will wait until
dependencies are satisfied before starting any processes associated with the service.
The first service is the \fIdependent\fR service and the latter is the \fIdependency\fR
service (we will henceforth generally refer to the the dependency relationship as the
\fIrelationship\fR and use \fIdependency\fR to refer to the service).
A dependency relationship is specified via the properties of the dependent.
There are different relationship types, as follows:
.IP \(bu
A \fBneed\fR (or "hard") relationship specifies that the dependent must wait
for the dependency to be started before it starts, and that the dependency
must remain started while the dependent is started.
Starting the dependent will start the dependency, and stopping the dependency will stop the
dependent. This type of relationship is specified using a \fBdepends-on\fR property.
.IP \(bu
A \fBmilestone\fR relationship specifies that the dependency must
start successfully before the dependent starts.
Starting the dependent will therefore start the dependency.
Once started, the relationship is satisfied; if the dependency then stops, it
has no effect on the dependent.
However, if the dependency fails to start or has its startup cancelled, the dependent will
not start (and will return to the stopped state).
This type of relationship is specified using a \fBdepends-ms\fR property.
.IP \(bu
A \fBwaits-for\fR relationship specifies that the dependency must
start successfully, or fail to start, before the dependent starts.
Starting the dependent will attempt to first start the dependency, but failure will
not prevent the dependent from starting.
If the dependency starts, stopping it will have no effect on the dependent.
This type of relationship is specified using a \fBwaits-for\fR property.
.LP
See the \fBSERVICE ACTIVATION MODEL\fR section in \fBdinit\fR(8) for more details of how service
dependencies affect starting and stopping of services.
.\"
.SS SERVICE PROPERTIES
.\"
This section described the various service properties that can be specified
in a service description file. The properties specify the type of the service,
dependencies of the service, and other service configuration.
.LP
Each line of the file can specify a single property value, expressed as `\fIproperty-name\fR =
\fIvalue\fR', or `\fIproperty-name\fR: \fIvalue\fR'.
There is currently no functional difference between either form of assignment, but note that some
settings will override any previous setting of the same property whereas some effectively add a
new distinct property, and it is recommended to use `=' or `:' (respectively) to distinguish them. 
.LP
A small selection of properties can have their value appended to, once set on a previous line,
by specifying the property name again and using the `+=' operator in place of `=' (or `:').
.LP
Comments begin with a hash mark (#) and extend to the end of the line (they must be
separated from setting values by at least one whitespace character).
Values are interpreted literally, except that:
.\"
.IP \(bu
White space (comprised of spaces, tabs, etc) is collapsed to a single space, except
leading or trailing white space around the property value, which is stripped.
.IP \(bu
For settings which specify a command with arguments, the value is interpreted as a
series of tokens separated by white space, rather than a single string of characters. 
.IP \(bu
Double quotes (") can be used around all or part of a property value, to
prevent whitespace collapse and prevent interpretation of other special
characters (such as "#") inside the quotes.
The quote characters are not considered part of the property value.
White space appearing inside quotes does not act as a delimiter for tokens.
.IP \(bu
A backslash (\\) can be used (even inside double quotes) to escape the next character, causing it
to lose any special meaning and become part of the property value (escaped newlines are an
exception\(em\&they mark the end of a comment, and otherwise are treated as an unescaped space,
allowing a property value to extend to the next line; in this case, the following line must begin
with leading whitespace).
A double backslash (\\\\) is collapsed to a single backslash within the parameter value.
White space preceded by a backslash can be used to include whitespace within a token.
.LP
Setting a property generally overrides any previous setting (from prior lines).
However some properties are set additively; these include dependency relationships and \fBoptions\fR
properties.
.LP
Some properties that specify file paths are currently resolved (if the specified path is relative)
starting from the directory containing the top-level service description file, whereas others are
resolved from the directory containing the service description fragment in which the setting value
is defined (a "fragment" may be the service description file itself, or it may be a file included
via \fB@include\fR or similar; see \fBMETA-COMMANDS\fR). In particular, the `\-\-\-.d' settings
(such as \fBwaits-for.d\fR) are resolved from the containing fragment. For all other settings, it
is recommended to provide absolute paths to be robust against future changes in Dinit.
.LP
The following properties can be specified:
.TP
\fBtype\fR = {process | bgprocess | scripted | internal | triggered}
Specifies the service type; see the \fBSERVICE TYPES\fR section.
.TP
\fBcommand\fR = \fIcommand-string\fR
.TQ
\fBcommand\fR += \fIcommand-string-addendum\fR
Specifies the command, including command-line arguments, for starting the process.
Applies only to \fBprocess\fR, \fBbgprocess\fR and \fBscripted\fR services.
The value is subject to variable substitution (see \fBVARIABLE SUBSTITUTION\fR).
.IP
The `+=' operator can be used with this setting to append to a command set previously.
.TP
\fBstop\-command\fR = \fIcommand-string\fR
.TQ
\fBstop\-command\fR += \fIcommand-string-addendum\fR
Specifies the command to stop the service (optional). Applicable to \fBprocess\fR, \fBbgprocess\fR and
\fBscripted\fR services.  If specified for \fBprocess\fR or \fBbgprocess\fR services, the "stop
command" will be executed in order to stop the service, instead of signalling the service process. 
The value is subject to variable substitution (see \fBVARIABLE SUBSTITUTION\fR).
.IP
The `+=' operator can be used with this setting to append to a command set previously.
.TP
\fBworking\-dir\fR = \fIdirectory\fR
Specifies the working directory for this service.
For a scripted service, this affects both the start command and the stop command.
The default is the directory containing the service description.
The value is subject to variable substitution (see \fBVARIABLE SUBSTITUTION\fR).
.TP
\fBrun\-as\fR = \fIuser-id\fR
Specifies which user to run the process(es) for this service as.
Specify as a username or numeric ID.
If specified by name, the group for the process will also be set to the primary
group of the specified user, and supplementary groups will be initialised (unless support
for them is disabled) according to the system's group database.
If specified by number, the group for the process will remain the same as that of the
running \fBdinit\fR process, and all supplementary groups will be dropped (unless support
has been disabled).
.TP
\fBenv\-file\fR = \fIfile\fR
Specifies a file containing value assignments for environment variables, in the same
format recognised by the \fBdinit\fR command's \fB\-\-env\-file\fR option (see \fBdinit\fR(8)).
The file is read when the service is loaded, therefore values from it can be used in variable
substitutions (see \fBVARIABLE SUBSTITUTION\fR).
Minimal variable substitution is performed on the \fBenv\-file\fR property value itself.
If the path is not absolute, it is resolved relative to the directory containing the service
description.
.TP
\fBrestart\fR = {yes | true | on-failure | no | false}
Indicates whether the service should automatically restart if it stops, including due to
unexpected process termination or a dependency stopping.
Specifying \fBon-failure\fR for a \fBprocess\fR or \fBbgprocess\fR service causes the service to
be restarted only when the exit status of the service process is non-zero, or if the process was
terminated via a signal (other than SIGHUP, SIGINT, SIGUSR1, SIGUSR2 or SIGTERM, which indicate
deliberate termination).
Specifying \fBon-failure\fR for any other type of service is the same as specifying \fBfalse\fR
(the service will not restart automatically).
Note that if a service stops due to user request, automatic restart is inhibited.
$$$changequote(`,')dnl
ifelse(DEFAULT_AUTO_RESTART, ALWAYS,
    ``The default is to automatically restart.'',
    DEFAULT_AUTO_RESTART, ON_FAILURE,
    ``The default is to automatically restart only on process failure (\fBon-failure\fR).'',
    ``The default is to not automatically restart.'')
changequote(`@@@',`$$$')dnl
@@@.TP
\fBsmooth\-recovery\fR = {yes | true | no | false}
Applies only to \fBprocess\fR and \fBbgprocess\fR services.
When set to true/yes, if the process terminates unexpectedly (i.e. without a stop order having been
issued), an automatic process restart is performed, without first stopping any dependent services
and without the service changing state.
The normal restart restrictions (such as \fBrestart\-limit\-count\fR) apply.
.TP
\fBrestart\-delay\fR = \fIXXX.YYYY\fR
Specifies the minimum time (in seconds) between automatic restarts.
The default is 0.2 (200 milliseconds).
.TP
\fBrestart\-limit\-interval\fR = \fIXXX.YYYY\fR
Sets the interval (in seconds) over which restarts are limited.
If a process automatically restarts more than a certain number of times (specified by the
\fBrestart-limit-count\fR setting) in this time interval, it will not be restarted again.
The default value is 10 seconds.
.TP
\fBrestart\-limit\-count\fR = \fINNN\fR
Specifies the maximum number of times that a service can automatically restart
over the interval specified by \fBrestart\-limit\-interval\fR.
Specify a value of 0 to disable the restart limit.
The default value is 3.
.TP
\fBstart\-timeout\fR = \fIXXX.YYY\fR
Specifies the time in seconds allowed for the service to start.
If the service takes longer than this, its process group is sent a SIGINT signal
and enters the "stopping" state (this may be subject to a stop timeout, as
specified via \fBstop\-timeout\fR, after which the process group will be
terminated via SIGKILL).
The timeout period begins only when all dependencies have been satisfied.
The default value is $$$DEFAULT_START_TIMEOUT@@@.
A value of 0 allows unlimited start time.
.TP
\fBstop\-timeout\fR = \fIXXX.YYY\fR
Specifies the time in seconds allowed for the service to stop.
If the service takes longer than this, its process group is sent a SIGKILL signal
which should cause it to terminate immediately.
The timeout period begins only when all dependent services have already stopped.
The default value is $$$DEFAULT_STOP_TIMEOUT@@@.
A value of 0 allows unlimited stop time.
.TP
\fBpid\-file\fR = \fIpath-to-file\fR
For \fBbgprocess\fR type services only; specifies the path of the file where
daemon will write its process ID before detaching.
Dinit will read the contents of this file when starting the service, once the initial process
exits, and will supervise the process with the discovered process ID.
Dinit may also send signals to the process ID to stop the service; if \fBdinit\fR runs as a
privileged user the path should have appropriate permissions to permit abuse by untrusted
unprivileged processes.
.IP
The value is subject to variable substitution (see \fBVARIABLE SUBSTITUTION\fR).
.TP
\fBdepends\-on\fR: \fIservice-name\fR
This service depends on the named service.
Starting this service will start the named service; the command to start this service will not be executed
until the named service has started.
If the named service stops then this service will also be stopped.
The \fIservice-name\fR is subject to pre-load variable substitution
(see \fBVARIABLE SUBSTITUTION\fR).
.TP
\fBdepends\-ms\fR: \fIservice-name\fR
This service has a "milestone" dependency on the named service. Starting this
service will start the named service; this service will not start until the
named service has started, and will fail to start if the named service does
not start.
Once the named (dependent) service reaches the started state, however, the
dependency may stop without affecting the dependent service.
The name is likewise subject to pre-load variable substitution.
.TP
\fBwaits\-for\fR: \fIservice-name\fR
When this service is started, wait for the named service to finish starting
(or to fail starting) before commencing the start procedure for this service.
Starting this service will automatically start the named service.
If the named service fails to start, this service will start as usual (subject to
other dependencies being met).
The name is likewise subject to pre-load variable substitution.
.TP
\fBdepends\-on.d\fR: \fIdirectory-path\fR
For each file name in \fIdirectory-path\fR which does not begin with a dot,
add a \fBdepends-on\fR dependency to the service with the same name.
Note that contents of files in the specified directory are not significant; expected
usage is to have symbolic links to the associated service description files,
but this is not required.
Failure to read the directory contents, or to find any of the services named within,
is not considered fatal.
.IP
The directory path, if not absolute, is relative to the directory containing the service
description file.
No variable substitution is done for path dependencies.
.TP
\fBdepends\-ms.d\fR: \fIdirectory-path\fR
As for \fBdepends-on.d\fR, but with dependency type \fBdepends\-ms\fR.
.TP
\fBwaits\-for.d\fR: \fIdirectory-path\fR
As for \fBdepends-on.d\fR, but with dependency type \fBwaits\-for\fR.
.TP
\fBafter\fR: \fIservice-name\fR
When starting this service, if the named service is also starting, wait for the named service
to finish starting before bringing this service up. This is similar to a \fBwaits\-for\fR
dependency except no dependency relationship is implied; if the named service is not starting,
starting this service will not cause it to start (nor wait for it in that case).
It does not by itself cause the named service to be loaded (if loaded later, the "after"
relationship will be enforced from that point).
.IP
The name is subject to pre-load variable substitution (see \fBVARIABLE SUBSTITUTION\fR).
.TP
\fBbefore\fR: \fIservice-name\fR
When starting the named service, if this service is also starting, wait for this service
to finish starting before bringing the named service up. This is largely equivalent to specifying
an \fBafter\fR relationship to this service from the named service.
However, it does not by itself cause the named service to be loaded (if loaded later, the "before"
relationship will be enforced from that point).
.IP
The name is subject to pre-load variable substitution (see \fBVARIABLE SUBSTITUTION\fR).
.TP
\fBchain\-to\fR = \fIservice-name\fR
When this service terminates (i.e. starts successfully, and then stops of its
own accord), the named service should be started.
Note that the named service is not loaded until that time; naming an invalid service will
not cause this service to fail to load.
.IP
This can be used for a service that supplies an interactive "recovery mode"
for another service; once the user exits the recovery shell, the primary
service (as named via this setting) will then start.
It also supports multi-stage system startup where later service description files reside on
a separate filesystem that is mounted during the first stage; such service
descriptions will not be found at initial start, and so cannot be started
directly, but can be chained via this directive.
.IP
The chain is not executed if the initial service was explicitly stopped,
stopped due to a dependency stopping (for any reason), if it will restart
(including due to a dependent restarting), or if its process terminates
abnormally or with an exit status indicating an error.
However, if the \fBalways-chain\fR option is set the chain is started regardless of the
reason and the status of this service termination.
.IP
The name is subject to pre-load variable substitution (see \fBVARIABLE SUBSTITUTION\fR).
.TP
\fBsocket\-listen\fR = \fIsocket-path\fR
Pre-open a socket for the service and pass it to the service using the
\fBsystemd\fR activation protocol.
This by itself does not give so called "socket activation", but does allow any
process trying to connect to the specified socket to do so immediately after
the service is started (even before the service process is properly prepared
to accept connections).
.IP
The path value is subject to variable substitution (see \fBVARIABLE SUBSTITUTION\fR).
.TP
\fBsocket\-permissions\fR = \fIoctal-permissions-mask\fR
Gives the permissions for the socket specified using \fBsocket\-listen\fR.
Normally this will be 600 (user access only), 660 (user and group
access), or 666 (all users).
The default is 666.
.TP
\fBsocket\-uid\fR = {\fInumeric-user-id\fR | \fIusername\fR}
Specifies the user (name or numeric ID) that should own the activation socket.
If \fBsocket\-uid\fR is specified as a name without also specifying \fBsocket\-gid\fR, then
the socket group is the primary group of the specified user (as found in the
system user database, normally \fI/etc/passwd\fR).
If the \fBsocket\-uid\fR setting is not provided, the socket will be owned by the user id of the \fBdinit\fR process.
.TP
\fBsocket\-gid\fR = {\fInumeric-group-id\fR | \fIgroup-name\fR}
Specifies the group of the activation socket. See discussion of \fBsocket\-uid\fR.
.TP
\fBterm\-signal\fR = {\fBnone\fR | \fIsignal-name\fR}
Specifies the signal to send to the process when requesting it to terminate (applies to `process'
and `bgprocess' services only).
Signal names are specified as the POSIX signal name without the \fBSIG\fR- prefix.
At least \fBHUP\fR, \fBTERM\fR, and \fBKILL\fR are supported (use \fBdinitctl signal \-\-list\fR
for the full list of supported signals).
The default is TERM (the SIGTERM signal).
See also the discussion of \fBstop\-timeout\fR.
.TP
\fBready\-notification\fR = {\fBpipefd:\fR\fIfd-number\fR | \fBpipevar:\fR\fIenv-var-name\fR}
Specifies the mechanism, if any, by which a process service will notify that it is ready
(successfully started).
If not specified, a process service is considered started as soon as it has begun execution.
The two options are:
.RS
.IP \(bu
\fBpipefd:\fR\fIfd-number\fR \(em the service will write a message to the specified file descriptor,
which \fBdinit\fR sets up as the write end of a pipe before execution.
This mechanism is compatible with the S6 supervision suite.
.IP \(bu
\fBpipevar:\fR\fIenv-var-name\fR \(em the service will write a message to file descriptor identified
using the contents of the specified environment variable, which will be set by \fBdinit\fR before
execution to a file descriptor (chosen arbitrarily) attached to the write end of a pipe.
.RE
.TP
\fBlog\-type\fR = {\fBfile\fR | \fBbuffer\fR | \fBpipe\fR | \fBnone\fR}
Specifies how the output of this service is logged.
This setting is valid only for process-based services (including \fBscripted\fR services).
.RS
.IP \(bu
\fBfile\fR: output will be written to a file; see the \fBlogfile\fR setting.
.IP \(bu
\fBbuffer\fR: output will be buffered in memory, up to a limit specified via the
\fBlog\-buffer\-size\fR setting.
The buffer contents can be examined via the \fBdinitctl\fR(8) \fBcatlog\fR subcommand. 
.IP \(bu
\fBpipe\fR: output will be written to a pipe, and may be consumed by another service
(see the \fBconsumer\-of\fR setting); note that, if output is not consumed promptly, the pipe buffer
may become full which may cause the service process to stall.
.IP \(bu
\fBnone\fR: output is discarded.
.RE
.IP
The default log type is \fBnone\fR, but note that specifying a \fBlogfile\fR setting can change the
log type to \fBfile\fR. For \fBpipe\fR (and \fBbuffer\fR, which uses a pipe internally),
note that the pipe created may outlive the service process and be re-used if the service is stopped
and restarted.
.\"
.TP
\fBlogfile\fR = \fIlog-file-path\fR
Specifies the log file for the service.
Output from the service process (standard output and standard error streams) will be appended to this file,
which will be created if it does not already exist. The file ownership and permissions are adjusted
according to the \fBlogfile\-uid\fR, \fBlogfile\-gid\fR and \fBlogfile\-permissions\fR settings.
This setting has no effect if the service is set to run on the console (via the \fBruns\-on\-console\fR,
\fBstarts\-on\-console\fR, or \fBshares\-console\fR options).
.IP
The log file path is subject to variable substitution (see \fBVARIABLE SUBSTITUTION\fR).
.IP
Note that if the directory in which the logfile resides does not exist (or is not otherwise accessible to
\fBdinit\fR) when the service is started, the service will not start successfully.
.IP
If this settings is specified and \fBlog\-type\fR is not specified or is currently \fBnone\fR, then
the log type will be changed to \fBfile\fR.
.TP
\fBlogfile\-permissions\fR = \fIoctal-permissions-mask\fR
Gives the permissions for the log file specified using \fBlogfile\fR. Normally this will be 600 (user access
only), 640 (also readable by the group), or 644 (readable by all users).
If the log file already exists when the service starts, its permissions will be changed in accordance with
the value of this setting.
The default is value 600 (accessible to only the owning user).
.TP
\fBlogfile\-uid\fR = {\fInumeric-user-id\fR | \fIusername\fR}
Specifies the user (name or numeric ID) that should own the log file.
If \fBlogfile\-uid\fR is specified as a name without also specifying \fBlogfile\-gid\fR, then
the log file group is the primary group of the specified user (as found in the
system user database, normally \fI/etc/passwd\fR).
If the log file already exists when the service starts, its ownership will be changed in accordance with
the value of this setting.
The default value is the user id of the \fBdinit\fR process.
.TP
\fBlogfile\-gid\fR = {\fInumeric-group-id\fR | \fIgroup-name\fR}
Specifies the group of the log file. See discussion of \fBlogfile\-uid\fR.
.TP
\fBlog\-buffer\-size\fR = \fIsize-in-bytes\fR
If the log type (see \fBlog\-type\fR) is set to \fBbuffer\fR, this setting controls the maximum
size of the buffer used to store process output. If the buffer becomes full, further output from
the service process will be discarded.
.TP
\fBconsumer\-of\fR = \fIservice-name\fR
Specifies that this service consumes (as its standard input) the output of another service.
For example, this allows this service to act as a logging agent for another service.
The named service must be a process-based service with \fBlog\-type\fR set to \fBpipe\fR.
This setting is only valid for \fBprocess\fR and \fBbgprocess\fR services.
The \fIservice-name\fR is subject to pre-load variable substitution
(see \fBVARIABLE SUBSTITUTION\fR).
.TP
\fBoptions\fR: \fIoption\fR...
Specifies various options for this service. See the \fBOPTIONS\fR section.
.TP
\fBload\-options\fR: \fIload_option\fR...
Specifies options for interpreting other settings when loading this service description.
Currently there are two available options. One is \fBexport-passwd-vars\fR, which
specifies that the environment variables `\fBUSER\fR', `\fBLOGNAME\fR' (same as
`\fBUSER\fR'), `\fBHOME\fR', `\fBSHELL\fR', `\fBUID\fR', and `\fBGID\fR' should
be exported into the service's load environment (that is, overriding any global
environment including the global environment file, but being overridable by the
service's environment file). The other is \fBexport-service-name\fR, which will
set the environment variable `\fBDINIT_SERVICE\fR' containing the name of the
current service.
.TP
\fBinittab\-id\fR = \fIid-string\fR
When this service is started, if this setting (or the \fBinittab\-line\fR setting) has a
specified value, an entry will be created in the system "utmp" database which tracks
processes and logged-in users.
Typically this database is used by the "who" command to list logged-in users.
The entry will be cleared when the service terminates.
.IP
The \fBinittab\-id\fR setting specifies the "inittab id" to be written in the entry for
the process.
The value is normally quite meaningless.
However, it should be distinct (or unset) for separate processes.
It is typically limited to a very short length.
.IP
The "utmp" database is mostly a historical artifact.
Access to it on some systems is prone to denial-of-service by unprivileged users.
It is therefore recommended that this setting not be used.
However, "who" and similar utilities may not work correctly without this setting
(or \fBinittab\-line\fR) enabled appropriately.
.IP
This setting has no effect if Dinit was not built with support for writing to the "utmp"
database. It applies only to \fBprocess\fR services.
.TP
\fBinittab\-line\fR = \fItty-name-string\fR
This specifies the tty line that will be written to the "utmp" database when this service
is started.
Normally, for a terminal login service, it would match the terminal device name on which
the login process runs, without the "/dev/" prefix.
.IP
See the description of the \fBinittab\-id\fR setting for details.
.TP
\fBrlimit\-nofile\fR = \fIresource-limits\fR
Specifies the number of file descriptors that a process may have open simultaneously.
See the \fBRESOURCE LIMITS\fR section.
.TP
\fBrlimit\-core\fR = \fIresource-limits\fR
Specifies the maximum size of the core dump file that will be generated for the process if it
crashes (in a way that would result in a core dump).
See the \fBRESOURCE LIMITS\fR section.
.TP
\fBrlimit\-data\fR = \fIresource-limits\fR
Specifies the maximum size of the data segment for the process, including statically allocated
data and heap allocations.
Precise meaning may vary between operating systems.
See the \fBRESOURCE LIMITS\fR section.
.TP
\fBrlimit\-addrspace\fR = \fIresource-limits\fR
Specifies the maximum size of the address space of the process.
See the \fBRESOURCE LIMITS\fR section.
Note that some operating systems (notably, OpenBSD) do not support this limit; the
setting will be ignored on such systems.
.TP
\fBnice\fR = \fInice-value\fR
Specifies the CPU priority of the process.
When the given value is out of range for the operating system, it will be clamped to
supported range, but no error will be issued.
On Linux, this also sets the autogroup priority, assuming procfs is mounted.
.TP
\fBrun\-in\-cgroup\fR = \fIcgroup-path\fR
Run the service process(es) in the specified cgroup (see \fBcgroups\fR(7)).
The cgroup is specified as a path; if it has a leading slash, the remainder of the path is
interpreted as relative to \fI/sys/fs/cgroup\fR, and otherwise the entire path is interpreted
relative to the cgroup in which \fBdinit\fR is running (as determined at startup or specified
by options).
The latter can only be used if there is only a single cgroup hierarchy (either the cgroups v2
hierarchy with no cgroups v1 hierarchies, or a single cgroups v1 hierarchy).
.IP
Note that due to the "no internal processes" rule in cgroups v2, a relative path must typically
begin with ".." if cgroups v2 are used.
.IP
The named cgroup must already exist prior to the service starting; it will not be created by
\fBdinit\fR.
.IP
This setting is only available if \fBdinit\fR was built with cgroups support.
.TP
\fBcapabilities\fR = \fIiab\fR
.TQ
\fBcapabilities\fR += \fIiab-addendum\fR
Run the service process(es) with capabilities specified by \fIiab\fR.
The syntax follows the regular capabilities "IAB" format, with comma-separated capabilities
(see \fBcapabilities\fR(7), \fBcap_iab\fR(3)).
The append form of this setting will add to the previous IAB string, automatically inserting
a comma as separator.
.IP
To provide a capability to an otherwise unprivileged process, add the capability to the "Ambient"
(and "Inherited") capability sets using \fB^CAP_NAME\fR, where CAP_NAME is the name of the capability.
For example, to allow a process to bind to privileged TCP/IP ports, use \fB^CAP_NET_BIND_SERVICE\fR. 
.IP
This setting is only available if \fBdinit\fR was built with capabilities support.
.TP
\fBsecurebits\fR = \fIsecurebits-flags\fR
.TQ
\fBsecure\-bits\fR += \fIsecurebits-flags-addendum\fR
This is a companion option to \fBcapabilities\fR, specifying the `securebits' flags
(see \fBcapabilities\fR(7)) for the service process(es).
It is specified as a list of flag names separated by white space.
The allowed flags are \fBkeep\-caps\fR,
\fBno\-setuid\-fixup\fR, \fBnoroot\fR, and each of these with \fB\-locked\fR appended.
The `+=' operator used with this setting can be used to add additional securebits flags on top of
those specified previously.
.IP
This setting is only available if \fBdinit\fR was built with capabilities support.
.TP
\fBioprio\fR = \fIioprio-value\fR
Specifies the I/O priority class and value for the service's process(es).
The permitted values are \fBnone\fR, \fBidle\fR, \fBrealtime:\fR\fIPRIO\fR, and
\fBbest\-effort:\fR\fIPRIO\fR, where \fIPRIO\fR is an integer value no less than 0
and no more than 7.
.IP
This setting is only available if \fBdinit\fR was built with ioprio support.
.TP
\fBoom-score-adj\fR = \fIadj-value\fR
Specifies the OOM killer score adjustment for the service's process(es).
The value is an integer no less than -1000 and no more than 1000.
.IP
This setting is only available if \fBdinit\fR was built with OOM score adjustment support.
.IP
This setting requires the `proc' filesystem to be mounted (at \fB/proc\fR) before the service
process begins execution, and will result in a service startup failure if that is not the case.
.\"
.SS OPTIONS
.\"
These options are specified via the \fBoptions\fR parameter. 
.\"
.TP
\fBruns\-on\-console\fR
Specifies that this service uses the console; its input and output should be
directed to the console (or precisely, to the device to which \fBdinit\fR's standard
output stream is connected).
A service running on the console prevents other services from running on the
console (they will queue for access to the console), and inhibits \fBdinit\fR's own output to it
(some output will be buffered and displayed later, but some may be dropped completely).
.IP
Proper operation of this option (and related options) assumes that \fBdinit\fR
is itself attached correctly to the console device (or a terminal, in which case
that terminal will be used as the "console").
.IP
The \fIinterrupt\fR key (normally control-C) may be active for process / scripted
services that run on the console, depending on terminal configuration and operating-system
specifics.
The interrupt signal (SIGINT), however, is masked by default (but see \fBunmask\-intr\fR).
.TP
\fBstarts\-on\-console\fR
Specifies that this service uses the console during service startup.
This is identical to \fBruns\-on\-console\fR except that the console will be released
(available for running other services) once the service has started.
It is applicable only for \fBbgprocess\fR and \fBscripted\fR services.
.IP
As for the \fBruns\-on\-console\fR option, the \fIinterrupt\fR key will be enabled
while the service has the console.
.TP
\fBshares\-console\fR
Specifies that this service should be given access to the console (input and output
will be connected to the console), but that it should not exclusively hold the
console. A service given access to the console in this way will not delay the startup of services
which require exclusive access to the console (see \fBstarts\-on\-console\fR,
\fBruns\-on\-console\fR) nor will it be itself delayed if such services are already running.
.IP
This is mutually exclusive with both \fBstarts\-on\-console\fR and \fBruns\-on\-console\fR;
setting this option unsets both those options, and setting either of those options unsets
this option.
.TP
\fBunmask\-intr\fR
For services that run or start on the console, specifies that the terminal interrupt signal
(SIGINT, normally invoked by control-C) should be unmasked.
Handling of an interrupt is determined by the service process, but typically will
cause it to terminate.
This option may therefore be used to allow a service to be terminated by the user via
a keypress combination.
In combination with \fBskippable\fR, it may allow service startup to be skipped.
.IP
A service with this option will typically also have the \fBstart\-interruptible\fR option
set.
.IP
Note that whether an interrupt can be generated, and the key combination required to do so,
depends on the operating system's handling of the console device and, if it is a terminal,
how the terminal is configured; see \fBstty\fR(1).
.IP
Note also that a process may choose to mask or unmask the interrupt signal of its own accord,
once it has started.
Shells, in particular, may unmask the signal; it might not be possible to reliably run a shell
script on the console without allowing a user to interrupt it.
.TP
\fBstarts\-rwfs\fR
This service mounts the root filesystem read/write (or at least mounts the
normal writable filesystems for the system).
This prompts Dinit to attempt to create its control socket, if it has not already managed to do so,
and similarly log boot time to the system \fBwtmp\fR(5) database (if supported) if not yet done.
This option may be specified on multiple services, which may be useful if the \fBwtmp\fR database becomes
writable at a different stage than the control socket location becomes writable, for example.
If the control socket has already been created, this option currently causes Dinit to check that
the socket "file" still exists and re-create it if not. It is not recommended to rely on this
behaviour.
.TP
\fBstarts\-log\fR
This service starts the system log daemon.
Dinit will begin logging via the \fI/dev/log\fR socket.
.TP
\fBpass\-cs\-fd\fR
Pass an open Dinit control socket to the process when launching it (the
\fIDINIT_CS_FD\fR environment variable will be set to the file descriptor of
the socket).
This allows the service to issue commands to Dinit even if the regular control socket is not available yet.
.IP
Using this option has security implications! The service which receives the
control socket must close it before launching any untrusted processes.
You should not use this option unless the service is designed to receive a Dinit
control socket.
.TP
\fBstart\-interruptible\fR
Indicates that this service can have its startup interrupted (cancelled), by sending it the SIGINT signal.
If service state changes such that this service will stop, but it is currently starting, and this option
is set, then Dinit will attempt to interrupt it rather than waiting for its startup to complete.
This is meaningful only for \fBbgprocess\fR and \fBscripted\fR services.
.TP
\fBskippable\fR
For scripted services, indicates that if the service startup process terminates
via an interrupt signal (SIGINT), then the service should be considered started.
Note that if the interrupt was issued by Dinit to cancel startup, the service
will instead be considered stopped.
.IP
This can be combined with options such as \fBstarts\-on\-console\fR to allow
skipping certain non-essential services (such as filesystem checks) using the
\fIinterrupt\fR key (typically control-C).
.TP
\fBsignal\-process-only\fR
Signal the service process only, rather than its entire process group, whenever
sending it a signal for any reason.
.TP
\fBalways\-chain\fR
Alters behaviour of the \fBchain-to\fR property, forcing the chained service to
always start on termination of this service (instead of only when this service
terminates with an exit status indicating success).
.TP
\fBkill\-all\-on\-stop\fR
Before stopping this service, send a TERM signal and then (after a short pause) a
KILL signal to all other processes in the system, forcibly terminating them.
This option is intended to allow system shutdown scripts to run without any possible
interference from "leftover" or orphaned processes (for example, unmounting file systems usually
requires that the file systems are no longer in use).
.IP
This option must be used with care since the signal broadcast does not discriminate and
potentially kills other services (or their shutdown scripts); a strict dependency ordering
is suggested, i.e. every other service should either be a (possibly transitive) dependency or
dependent of the service with this option set.
.IP
This option can be used for scripted and internal services only.
.TP
\fBno\-new\-privs\fR
Normally, child processes can gain privileges that their parent did not have, such
as setuid or setgid and file capabilities. This option can be specified to prevent
the service from gaining such privileges.
.IP
This setting is only available if \fBdinit\fR was built with capabilities support.
.\"
.SS RESOURCE LIMITS
.\"
There are several settings for specifying process resource limits: \fBrlimit\-nofile\fR,
\fBrlimit\-core\fR, \fBrlimit\-data\fR and \fBrlimit\-addrspace\fR.
See the descriptions of each above.
These settings place a limit on resource usage directly by the process.
Note that resource limits are inherited by subprocesses, but that usage of a resource
and subprocess are counted separately (in other words, a process can effectively bypass
its resource limits by spawning a subprocess and allocating further resources within it).
.LP
Resources have both a \fIhard\fR and \fIsoft\fR limit.
The soft limit is the effective limit, but note that a process can raise its soft limit up
to the hard limit for any given resource.
Therefore the soft limit acts more as a sanity-check; a process can exceed the soft limit
only by deliberately raising it first.
.LP
Resource limits are specified in the following format:
.sp
.RS
\fIsoft-limit\fR:\fIhard-limit\fR
.RE
.sp
Either the soft limit or the hard limit can be omitted (in which case it will be unchanged).
A limit can be specified as a dash, `\fB\-\fR', in which case the limit will be removed.
If only one value is specified with no colon separator, it affects both the soft and hard limit.
.\"
.SS VARIABLE SUBSTITUTION
.\"
Some service properties specify a path to a file or directory, or a command line.
For these properties, the specified value may contain one or more environment
variable names, each preceded by a single `\fB$\fR' character, as in `\fB$NAME\fR'.
In each case the value of the named environment variable will be substituted.
The name must begin with a non-punctuation, non-space, non-digit character, and ends
before the first control character, space, or punctuation character other than `\fB_\fR'.
To avoid substitution, a single `\fB$\fR' can be escaped with a second, as in `\fB$$\fR'.
.LP
Variable substitution also supports a limited subset of shell syntax. You can use curly
braces to enclose the variable, as in `\fB${NAME}\fR'.
Limited parameter expansion is also supported, specifically the forms `\fB${NAME:\-word}\fR'
(substitute `\fBword\fR' if variable is unset or empty), `\fB${NAME\-word}\fR' (substitute
`\fBword\fR' if variable is unset), `\fB${NAME:+word}\fR' (substitute `\fBword\fR' if variable is
set and non\-empty), and `\fB${NAME+word}\fR' (substitute `\fBword\fR' if variable is set).
Unlike in shell expansion, the substituted \fBword\fR does not itself undergo expansion and
cannot contain closing brace characters or whitespace, even if quoted.
.LP
To substitute the service argument, the `\fB$1\fR' syntax may be used.
The complete syntax of the substitution is supported here.
Services without an argument are treated as if the variable was unset, which
affects some of the curly brace syntax variants.
.LP
Note that by default, command-line variable substitution occurs after splitting the line into
separate arguments and so
a single environment variable cannot be used to add multiple arguments to a command line.
If a designated variable is not defined, it is replaced with an empty (zero-length) string, possibly producing a
zero-length argument.
To alter this behaviour use a slash after \fB$\fR, as in `\fB$/NAME\fR'; the expanded value will then
be split into several arguments separated by whitespace or, if the value is empty or consists only
of whitespace, will collapse (instead of producing an empty or whitespace argument).
.LP
Variable substitution occurs when the service is loaded.
Therefore, it is typically not useful for dynamically changing service parameters (including
command line) based on a variable that is inserted into \fBdinit\fR's environment once it is
running (for example via \fBdinitctl setenv\fR). 
.LP
The effective environment for variable substitution in setting values matches the environment supplied to the process
for a service when it is launched. The priority of environment variables, from highest to lowest, for both is:
.IP \(bu
variables from the service \fBenv\-file\fR
.IP \(bu
variables set by the \fBexport\-passwd\-vars\fR and \fBexport\-service\-name\fR load options
.IP \(bu
the process environment of \fBdinit\fR (which is established on launch by the process environment of the
parent, amended by loading the environment file (if any) as specified in \fBdinit\fR(8), and further
amended via \fBdinitctl setenv\fR commands or equivalent).
.LP
Note that since variable substitution is performed on service load, the values seen by a service
process may differ from those used for substitution, if they have been changed in the meantime.
Using environment variable values in service commands and parameters can be used as means to
provide easily-adjustable service configuration, but is not ideal for this purpose and alternatives
should be considered. 
.LP
A "pre-load" variable substitution is performed for certain service properties (as documented),
including \fBdepends\-on\fR as well as \fBbefore\fR/\fBafter\fR and similar, instead of the usual
(post-load) substitution.
This form of substitution is performed before the service environment is loaded.
It can substitute service arguments and environment variables set within \fBdinit\fR only; any
service-specific variables that will be loaded from file (as specified using \fBenv\-file\fR) are
not available. 
.\"
.SS META-COMMANDS
.\"
A number of meta-commands can be used in service description files.
A meta-command is indicated by an 'at' sign, \fB@\fR, at the beginning of the line (possibly preceded by whitespace).
Arguments to a meta-command follow on the same line and are interpreted as for setting values.
.LP  
The following commands are available:
.TP
\fB@include\fR \fIpath\fR
Include the contents of another file, specified via its full path.
If the specified file does not exist, an error is produced.
The \fIpath\fR is subject to pre-load variable substitution
(see \fBVARIABLE SUBSTITUTION\fR).
It is resolved relative to the path of the service description file or fragment containing the
directive.
.TP
\fB@include\-opt\fR \fIpath\fR
As for \fB@include\fR, but produces no error if the named file does not exist.
.\"
.SH EXAMPLES
.\"
Here is an example service description for the \fBmysql\fR database server.
It has a dependency on the \fBrcboot\fR service (not shown) which is
expected to have set up the system to a level suitable for basic operation.
.sp
.RS
.nf
.gcolor blue
.ft CR
# mysqld service
type = process
command = /usr/bin/mysqld --user=mysql
logfile = /var/log/mysqld.log
smooth-recovery = true
restart = false
depends-on: rcboot # Basic system services must be ready
.ft
.gcolor
.RE
.fi
.LP
Here is an examples for a filesystem check "service", run by a script
(\fI/etc/dinit.d/scripts/rootfscheck.sh\fR).
The script may need to reboot the system, but the control socket may not have been
created, so it uses the \fBpass-cs-fd\fR option to allow the \fBreboot\fR command
to issue control commands to Dinit.
It runs on the console, so that output is visible and the process can be interrupted
using control-C, in which case the check is skipped but dependent services continue to start.
.sp
.RS
.nf
.gcolor blue
.ft CR
# rootfscheck service
type = scripted
command = /etc/dinit.d/scripts/rootfscheck.sh
restart = false
options: starts-on-console pass-cs-fd
options: start-interruptible skippable
depends-on: early-filesystems  # /proc and /dev
depends-on: device-node-daemon
.ft
.gcolor
.fi
.RE
.sp
More examples are provided with the Dinit distribution.
.\"
.SH AUTHOR
.\"
Dinit, and this manual, were written by Davin McCall.
$$$dnl
