/** 
 * code:	PeZ
 * based:	Based on examples of libxml2 by Aleksey Sanin and libcurl.
 * author:	Roberto Abdelkader Martinez Perez (nilp0inter) wwww.rusoblanco.com
 * date:	2010/11/07
 * synopsis:	Evaluate XPath expression in HTML files in local disk or fetching from the web
 * usage:	pez <html-file or uri> <xpath-expr> [<known-ns-list>]
 * copy:	Under GPL2 License
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/HTMLparser.h>

#include <curl/curl.h>

#if !defined(LIBXML_XPATH_ENABLED) || !defined(LIBXML_SAX1_ENABLED)
#error XPath support not compiled in libxml2
#endif


struct MemoryStruct {
  char *memory;
  size_t size;
};
 
 
static size_t
WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)data;
 
  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    exit(EXIT_FAILURE);
  }
 
  memcpy(&(mem->memory[mem->size]), ptr, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

static void usage(const char *name);
int  execute_xpath_expression(const char* filename, const xmlChar* xpathExpr, const xmlChar* nsList);
int  register_namespaces(xmlXPathContextPtr xpathCtx, const xmlChar* nsList);
void print_xpath_nodes(xmlNodeSetPtr nodes, FILE* output);

int 
main(int argc, char **argv) {
    const char *xml_file = NULL;
    const char *xpath_expr = NULL;
    const char *ns_list = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "N:")) != -1) {
        switch (opt) {
            case 'N':
                ns_list = BAD_CAST optarg;
                break;
            default:
                usage(argv[0]);
                return -1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Missing required XPath expression.\n");
        usage(argv[0]);
        return -1;
    }

    xpath_expr = argv[optind++];

    if (optind < argc) {
        xml_file = argv[optind++];
    }

    /* Init libxml */     
    xmlInitParser();
    LIBXML_TEST_VERSION

    /* Process input */
    if (execute_xpath_expression(xml_file, BAD_CAST xpath_expr, ns_list) < 0) {
        usage(argv[0]);
        return -1;
    }

    /* Shutdown libxml */
    xmlCleanupParser();
    
    return 0;
}

/**
 * usage:
 * @name:		the program name.
 *
 * Prints usage information.
 */
static void 
usage(const char *name) {
    assert(name);
    
    fprintf(stderr, "Usage: %s [-N <known-ns-list>] <xpath-expr> [<xml-file>]\n", name);
    fprintf(stderr, "where <known-ns-list> is a list of known namespaces\n");
    fprintf(stderr, "in \"<prefix1>=<href1> <prefix2>=href2> ...\" format\n");
    fprintf(stderr, "If <xml-file> is not provided, the tool reads from stdin.\n");
}

/**
 * execute_xpath_expression:
 * @filename:		the input XML filename (can be NULL to read from stdin).
 * @xpathExpr:		the xpath expression for evaluation.
 * @nsList:		the optional list of known namespaces in 
 *			"<prefix1>=<href1> <prefix2>=href2> ..." format.
 *
 * Parses input XML file or reads from stdin, evaluates XPath expression, and prints results.
 *
 * Returns 0 on success and a negative value otherwise.
 */
int 
execute_xpath_expression(const char* filename, const xmlChar* xpathExpr, const xmlChar* nsList) {
    xmlDocPtr doc = NULL;
    xmlXPathContextPtr xpathCtx; 
    xmlXPathObjectPtr xpathObj; 
    
    assert(xpathExpr);

    htmlParserCtxtPtr parser = htmlNewParserCtxt();

    if (filename == NULL) {
        // Read XML from stdin
        doc = htmlReadFd(fileno(stdin), "stdin", NULL, HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET);
    } else if (access(filename, F_OK) != -1) {
        // Check if the file exists, read it if it does
        doc = htmlCtxtReadFile(parser, filename, NULL, HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET);
    } else {
        // Handle URL fetching
        CURL *curl_handle;
        struct MemoryStruct chunk;
        chunk.memory = malloc(1);  /* will be grown as needed by realloc */ 
        chunk.size = 0;    /* no data at this point */ 
 
        curl_global_init(CURL_GLOBAL_ALL);
        curl_handle = curl_easy_init();
        curl_easy_setopt(curl_handle, CURLOPT_URL, filename);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "pez/1.0");
        
        CURLcode res = curl_easy_perform(curl_handle);
        
        if(res != CURLE_OK) {
            fprintf(stderr, "Error: failed to fetch URL \"%s\"\n", filename);
            curl_easy_cleanup(curl_handle);
            curl_global_cleanup();
            if (chunk.memory) free(chunk.memory);
            return -1;
        }
        
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();

        doc = htmlCtxtReadMemory(parser, chunk.memory, chunk.size, filename, NULL, HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET);
        free(chunk.memory);
    }

    if (doc == NULL) {
        fprintf(stderr, "Error: unable to parse input \"%s\"\n", filename ? filename : "stdin");
        return -1;
    }

    /* Create XPath evaluation context */
    xpathCtx = xmlXPathNewContext(doc);
    if (xpathCtx == NULL) {
        fprintf(stderr, "Error: unable to create new XPath context\n");
        xmlFreeDoc(doc); 
        return -1;
    }
    
    /* Register namespaces from list (if any) */
    if (nsList != NULL && register_namespaces(xpathCtx, nsList) < 0) {
        fprintf(stderr, "Error: failed to register namespaces list \"%s\"\n", nsList);
        xmlXPathFreeContext(xpathCtx); 
        xmlFreeDoc(doc); 
        return -1;
    }

    /* Evaluate XPath expression */
    xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    if (xpathObj == NULL) {
        fprintf(stderr, "Error: unable to evaluate XPath expression \"%s\"\n", xpathExpr);
        xmlXPathFreeContext(xpathCtx); 
        xmlFreeDoc(doc); 
        return -1;
    }

    /* Print results */
    print_xpath_nodes(xpathObj->nodesetval, stdout);

    /* Cleanup */
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx); 
    xmlFreeDoc(doc); 
    
    return 0;
}

/**
 * register_namespaces:
 * @xpathCtx:		the pointer to an XPath context.
 * @nsList:		the list of known namespaces in 
 *			"<prefix1>=<href1> <prefix2>=href2> ..." format.
 *
 * Registers namespaces from @nsList in @xpathCtx.
 *
 * Returns 0 on success and a negative value otherwise.
 */
int 
register_namespaces(xmlXPathContextPtr xpathCtx, const xmlChar* nsList) {
    xmlChar* nsListDup;
    xmlChar* prefix;
    xmlChar* href;
    xmlChar* next;
    
    assert(xpathCtx);
    assert(nsList);

    nsListDup = xmlStrdup(nsList);
    if(nsListDup == NULL) {
	fprintf(stderr, "Error: unable to strdup namespaces list\n");
	return(-1);	
    }
    
    next = nsListDup; 
    while(next != NULL) {
	/* skip spaces */
	while((*next) == ' ') next++;
	if((*next) == '\0') break;

	/* find prefix */
	prefix = next;
	next = (xmlChar*)xmlStrchr(next, '=');
	if(next == NULL) {
	    fprintf(stderr,"Error: invalid namespaces list format\n");
	    xmlFree(nsListDup);
	    return(-1);	
	}
	*(next++) = '\0';	
	
	/* find href */
	href = next;
	next = (xmlChar*)xmlStrchr(next, ' ');
	if(next != NULL) {
	    *(next++) = '\0';	
	}

	/* do register namespace */
	if(xmlXPathRegisterNs(xpathCtx, prefix, href) != 0) {
	    fprintf(stderr,"Error: unable to register NS with prefix=\"%s\" and href=\"%s\"\n", prefix, href);
	    xmlFree(nsListDup);
	    return(-1);	
	}
    }
    
    xmlFree(nsListDup);
    return(0);
}

/**
 * print_xpath_nodes:
 * @nodes:		the nodes set.
 * @output:		the output file handle.
 *
 * Prints the @nodes content to @output.
 */
void
print_xpath_nodes(xmlNodeSetPtr nodes, FILE* output) {
    xmlBufferPtr buffer;
    xmlNodePtr cur;
    int size;
    int i;
    
    assert(output);
    size = (nodes) ? nodes->nodeNr : 0;

    // Create a buffer to hold the pretty-printed XML
    buffer = xmlBufferCreate();
    if (buffer == NULL) {
        fprintf(stderr, "Error: unable to create xml buffer\n");
        return;
    }

    for(i = 0; i < size; ++i) {
        assert(nodes->nodeTab[i]);
        cur = nodes->nodeTab[i];

        // Check if the node is a text node
        if (cur->type == XML_TEXT_NODE) {
            // Print the content of the text node
            fprintf(output, "%s\n", (const char *)cur->content);
        } else if (cur->type == XML_ELEMENT_NODE) {
            // Dump the node into the buffer with pretty formatting
            xmlNodeDump(buffer, cur->doc, cur, 0, 1);
	    
            // Print the buffer content
            fprintf(output, "%s\n", (const char *)buffer->content);
	    
            // Clear the buffer for the next node
            xmlBufferEmpty(buffer);
        }
    }

    // Clean up
    xmlBufferFree(buffer);
}
