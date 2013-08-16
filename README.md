pez
===

Command-line XPath evaluation tool

Build
-----

    $ gcc `xml2-config --cflags` `xml2-config --libs` -lcurl -o pez pez.c 

Examples
--------

Extract all links from page:

    $ pez "http://target.url.com" "//a/@href"

