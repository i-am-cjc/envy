# Envy

Minimal text editor written as an excuse to get my head around C again

Based on the kilo editor from:
http://viewsourcecode.org/snaptoken/kilo/

Loosely based on how I use vim.

### Key Bindings

In Normal Mode:
* o/O: Add a new line and enter insert mode
* i: Enter insert mode
* g/G: Go to top/bottom of file
* /: Find in file
* w: Write file
* q: Quit 
* Q: Quit without saving
* d: Delete line
* hjkl/cursor keys: Move around

In Insert mode:
* esc: Return to normal mode
* cursor keys: move around

### TODO
* Yank / Delete and paste (y/d/p/P)
* Fix some of the segfaults since breaking up the files
* Simple syntax highlighting, based off the latter parts of the kilo tutorial
