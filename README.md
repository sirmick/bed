# **bed** [Browser EDitor]
Browser based text editor

Have you ever wanted to do some file editing on a remote host, but found vim/pico/nano/emacs to be just too hard? Find you files littered with ";wq"? Is remote sync like SSHFS a pain? Hangs and frequently requires reconnection?

Well try **bed**! Launch bed the same way you would any text editor on your remote box, but then use the interface from your browser. No more vi, no more sshfs, just edit the files in a familiar sublime type interface, from a web browser.

# Usage

~~~~
Bed (Browser EDitor) is a browser based text editor, you start bed on your remote machine
pointing to a directory or file you wish to edit and then use your browser to connect and edit

Usage: bed <options> to-edit
Options:
  -h [ --help ]               Help screen
  --port arg (=8081)          The port to listen on
  --ssl arg (=0)              Use HTTPS
  --cert arg (=localhost.crt) Cert file to use
  --key arg (=localhost.key)  Key file to use
  --to-edit arg (=.)          File or directory to edit
~~~~

# Compilation

Requires boost, openssl and C++ 11y. It also requires [crow](https://github.com/ipkn/crow), a header file only distribution is included. License is the same for both products: BSD-3.

## OSX
- Install brew (visit [brew](https://brew.sh/) website)
- Install clang (`brew install llvm`)
- Install openssl (`brew install openssl`)
- Install boost (`brew install boost`)
- Compile (`./compile.osx.sh`)

## Ubuntu
