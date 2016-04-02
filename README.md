# synth

A semantic syntax highlighting and code hyper-linking tool for C and C++.
It is written in C++ and based on [libclang][], Clang's C-Interface.

An example of what it generates is its output when run on itself (note that the
directory listing was not generated with synth itself but with the
``dir2html.py`` script run on the output):

> [**DEMO**: synth's source highlighted with synth](//oberon00.github.io/synth)

## Usage

synth is a commandline-tool with the following usage syntax:

    synth <OPTIONS> (<inroot> [-o <outroot>])... (--db <dbdir>|--cmd <cmd>)

synth has two usage modes: In ``--db`` mode it expects a directory with a Clang
compilation database (``compilation_commands.json``) in ``<dbdir>``. See
<http://clang.llvm.org/docs/HowToSetupToolingForLLVM.html> for how to create
such a file for your project (it's trivial if you already use CMake with Clang).

In ``--cmd`` mode a full clang command line is passed e.g.
``--cmd /usr/bin/clang++ myfile.cpp -I ~/my/include/dir``. It is important that
the clang path is correct because certain include files are searched relative to
it.

synth will execute all given commands (except in ``--db`` mode it only executes
commands without filename or with a filename that is under some ``<inroot>``).
For each file encountered (including all ``#include``d files) it produces an
output file only if the file is in some ``<inroot>``. The output file will then
be written as ``<outroot>/<relpath>.html`` where ``<outroot>`` is the one given
next to the matching ``<inroot>`` and ``<relpath>`` is the path of the file
relative to the matched ``<inroot>`` (if multiple ``<inroot>``s match, the first
one is used).

These options are allowed:
  * ``-j <n>``: Use ``<n>`` threads. If the option is omitted, the number of CPU
    cores is used (same when ``<n>`` is zero). Ignored in ``--cmd`` mode.
  * ``-t <templatefile>``: Use the ``<templatefile>`` as output-template. All
    outputs will be formatted according to this file. The following replacements
    are made:
      + ``@@code@@``: The highlighted code (without any surrounding ``<code>``
        or ``<pre>`` tags.
      + ``@@filename@@``: The name of the input file, relative to the matched
        ``<inroot>``.
      + ``@@rootdir@@``: A relative path to the output root. If multiple
        ``<outroot>``s are given and their common prefix is under the current
        working directory, that common prefix is used as output root for all
        files. Otherwise each file simply has the ``<outroot>`` of the matched
        ``<inroot>`` as its output root. This is useful e.g. to link to a
        CSS-file.
    By default, synth will use a minimal HTML5 template with the filename as
    ``<title>`` and referencing a ``@@rotdir@@/code.css`` stylesheet.
  * ``-e <arg>``: Can be given multiple times. ``<arg>`` will be appended to the
    arguments passed to clang. E.g. to specify an additional include directory
    use ``-e -I -e ~/my/include/dir``. Useful in ``--db`` mode.

### Example

    synth my-repo/ -o my-repo-html/ /usr/include/ -o my-repo-html/include \
        --db my-repo-builddir/

Process all commands of the compilation-database under ``my-repo-builddir/``,
generating output for files under ``my-repo/`` to ``my-repo-html/`` and for
files under ``/usr/include/`` to ``my-repo-html/include``. E.g. a file
``my-repo/src/main.cpp`` will be processed to ``my-repo-html/src/main.cpp``
and ``/usr/include/math.h`` will be processed to
``my-repo-html/include/math.h``. If e.g. a file
``/usr/local/include/boost/filesystem.hpp`` was also included, it would not be
processed by synth (it will still be parsed though as this is necessary for
highlighting files that use it).

## Higlighting

The highlighting classes are the same ones that
[Pygments](http://pygments.org/) uses. Additionally the CSS classes ``decl`` and
``def`` are given to declarations and definitions respectively (tokens that are
both get both classes). Note that most Pygments styles seem to assume that
classes such as ``nf`` (``Name.Function``) are only emitted for function
declarations/definitions but synth also highlights usages. This is the reason
for the included ``code.css`` style which is a version of the ``colorful`` style
where only declarations and definitions are bold.

## Building

1. Clone or download the repository
2. Set up dependencies (Boost 1.60 and libclang 3.7+ (tested with 3.8), a C++14
   compiler (tested with clang).
3. Create a build directory and from there run `cmake <path to repo root>`.
4. Run your build tool on the generated build files e.g. ``make`` or ``msbuild
   synth.sln``. If you have a not too ancient CMake you can just use
   ``cmake --build .``.

## License

This project is licensed under the MIT license. See [LICENSE.txt](LICENSE.txt)


[libclang]: http://clang.llvm.org/doxygen/group__CINDEX.html
