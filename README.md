pez
===

Command-line XPath evaluation tool

Build
-----

    $ gcc `xml2-config --cflags` `xml2-config --libs` -o pez pez.c -lxml2 -lm -lcurl

Examples
--------

Extract all links from page:

    $ pez "http://target.url.com" "//a/@href"

