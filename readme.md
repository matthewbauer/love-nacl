This is a port of Journey to the Center of Hawkthorne for Chrome OS.

Setting up the Repo
-------------------

You'll probably need linux (or something linux-like).

    # Find a good place to put love-nacl
    $ git clone git://github.com/matthewbauer/love-nacl
    $ cd love-nacl
    $ git submodule init
    $ git submodule update


Building
--------

    $ make

The output is put in $PWD/out. The data needed for the package is in
$PWD/out/package.


Running
-------

    $ export CHROME_PATH=/path/to/chrome
    $ make run-package
