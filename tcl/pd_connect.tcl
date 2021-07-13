
package provide pd_connect 0.1

namespace eval ::pd_connect:: {
    variable pd_socket
    variable cmdbuf ""
    variable plugin_dispatch_receivers

    namespace export to_pd
    namespace export create_socket
    namespace export pdsend
    namespace export register_plugin_dispatch_receiver
}

# TODO figure out how to escape { } properly

proc ::pd_connect::configure_socket {sock} {
    fconfigure $sock -blocking 0 -buffering none -encoding utf-8;
    fileevent $sock readable {::pd_connect::pd_readsocket}
}

# if pd opens first, it starts pd-gui, then pd-gui connects to the port pd sent
proc ::pd_connect::to_pd {port {host localhost}} {
    variable pd_socket
    ::pdwindow::debug "'pd-gui' connecting to 'pd' on localhost $port ...\n"
    if {[catch {set pd_socket [socket $host $port]}]} {
        if {$host ne "localhost" || [catch {set pd_socket [socket 127.0.0.1 $port]}]} {
            puts stderr "WARNING: connect to pd failed, retrying port $host:$port."
            after 1000 ::pd_connect::to_pd $port $host
            return
        }
    }
    ::pd_connect::configure_socket $pd_socket
}

# if pd-gui opens first, it creates socket and requests a port.  The function
# then returns the portnumber it receives. pd then connects to that port.
proc ::pd_connect::create_socket {} {
    if {[catch {set sock [socket -server ::pd_connect::from_pd -myaddr localhost 0]}]} {
        puts stderr "ERROR: failed to allocate port, exiting!"
        exit 3
    }
    return [lindex [fconfigure $sock -sockname] 2]
}

proc ::pd_connect::from_pd {channel clientaddr clientport} {
    variable pd_socket $channel
    ::pdwindow::debug "connection from 'pd' to 'pd-gui' on $clientaddr:$clientport\n"
    ::pd_connect::configure_socket $pd_socket
}

# send a pd/FUDI message from Tcl to Pd. This function aims to behave like a
# [; message( in Pd or pdsend on the command line.  Basically, whatever is in
# quotes after the proc name will be sent as if it was sent from a message box
# with a leading semi-colon.
proc ::pd_connect::pdsend {message} {
    variable pd_socket
    append message \;
    if {[catch {puts $pd_socket $message} errorname]} {
        puts stderr "pdsend errorname: >>$errorname<<"
        error "not connected to 'pd' process"
    }
}

# register a plugin to receive messages from running Pd patches
proc ::pd_connect::register_plugin_dispatch_receiver { nameatom callback } {
    variable plugin_dispatch_receivers
    lappend plugin_dispatch_receivers($nameatom) $callback
}

proc ::pd_connect::assemble_cmd {A B} {
    # assembles A & B into two strings cmds & rest
    #  cmds: string that can be evaluated
    #  rest: the remainder
    # assembling is done from the end of $B (to get the maximum)
    set lfi end
    set ok 0
    while { [set lfi [string last "\n" $B $lfi]] >= 0 } {
        set ab $A[string range $B 0 $lfi]
        set b [string range $B [expr $lfi + 1] end]
        incr lfi -1
        if {[info complete $ab]} {
            set A $ab
            set B $b
            set ok 1
            break
        }
        set ab ""
        set b ""
    }
    if { $ok } {
        list $A $B
    } {
        list {} [append A $B]
    }
}

proc ::pd_connect::pd_readsocket {} {
     variable pd_socket
     variable cmdbuf

     if {[eof $pd_socket]} {
         # if we lose the socket connection, that means pd quit, so we quit
         close $pd_socket
         exit
     }

    foreach {docmds cmdbuf} [assemble_cmd $cmdbuf [read $pd_socket]] { break; }
    if { [string length $docmds] > 0 } {
         if {![catch {uplevel #0 $docmds} errorname]} {
             # we ran the command block without error, reset the buffer
         } else {
             # oops, error, alert the user:
             global errorInfo
             switch -regexp -- $errorname {
                 "missing close-brace" {
                     ::pdwindow::fatal \
                         [concat [_ "(Tcl) MISSING CLOSE-BRACE '\}': "] $errorInfo "\n"]
                 } "^invalid command name" {
                     ::pdwindow::fatal \
                         [concat [_ "(Tcl) INVALID COMMAND NAME: "] $errorInfo "\n"]
                 } default {
                     ::pdwindow::fatal \
                         [concat [_ "(Tcl) UNHANDLED ERROR: "] $errorInfo "\n"]
                 }
             }
         }
     }
}
