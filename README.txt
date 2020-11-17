# smallsh

smallsh is a shell that has status, exit, and cd built in, handles input and output redirection, expands $$ into the pid, ignores comments "$ # text" and blank lines, 
can execute in the background and foreground and can handle reetrancy via the signals SIGINT & SIGSTP

# how to compile 

gcc --std=c99 smallsh.c -o smallsh

# how to run 

./smallsh

Example: 
$: ls

$: ls > junk 

$: sleep 100 &
