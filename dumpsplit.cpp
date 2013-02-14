/*
 * wikiblame - edit attribution for mediawiki
 * Copyright (c) 2013  Greg Hewgill <greg@hewgill.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/sha.h>

char buf[16384];
size_t size;
char text[2560];
size_t textsize;
bool textcap;
bool textcopy;
char textfirstchar;
char pagefn[2000];

int main(int argc, char *argv[])
{
    const char *outdir = NULL;
    if (argc > 2) {
        outdir = argv[2];
    }
    char cmd[200];
    snprintf(cmd, sizeof(cmd), "7za x -so %s", argv[1]);
    FILE *f = popen(cmd, "r");
    textcap = false;
    textcopy = false;
    bool firstid = false;
    FILE *outfile = NULL;
    std::string id;
    std::string timestamp;
    std::string contributor;
    size_t p = 0;
    while (true) {
restart:
        size_t n = fread(buf+p, 1, sizeof(buf)-p, f);
        if (n == 0) {
            break;
        }
        size = p + n;
        p = 0;
        while (p < size) {
            const char *a = static_cast<const char *>(memchr(buf+p, '<', size-p));
            if (a != NULL) {
                if (textcap) {
                    size_t n = a - (buf+p);
                    assert(textsize + n <= sizeof(text));
                    memcpy(text+textsize, buf+p, n);
                    textsize += n;
                } else if (textcopy) {
                    if (textfirstchar == 0) {
                        textfirstchar = buf[p];
                    }
                    if (outfile != NULL) {
                        fwrite(buf+p, 1, a - (buf+p), outfile);
                    }
                }
                const char *e = static_cast<const char *>(memchr(a, '>', buf+size-a));
                if (e == NULL) {
                    p = buf + size - a;
                    memmove(buf, a, p);
                    goto restart;
                }
                if (strncmp(a, "</page>", 7) == 0) {
                    if (outfile != NULL) {
                        pclose(outfile);
                        outfile = NULL;
                        if (textfirstchar == '#') {
                            char fn[2000];
                            snprintf(fn, sizeof(fn), "%s/%s.xz", outdir, pagefn);
                            unlink(fn);
                        }
                    }
                } else if (strncmp(a, "<revision>", 10) == 0) {
                    firstid = true;
                    id = "";
                    timestamp = "";
                    contributor = "";
                } else if (strncmp(a, "</revision>", 11) == 0) {
                    if (outfile != NULL) {
                        fwrite("", 1, 1, outfile);
                    }
                } else if (strncmp(a, "<title>", 7) == 0) {
                    textsize = 0;
                    textcap = true;
                } else if (strncmp(a, "</title>", 8) == 0) {
                    textcap = false;
                    fwrite(text, 1, textsize, stdout);
                    puts("");
                    text[textsize] = 0;
                    if (outdir != NULL) {
                        assert(outfile == NULL);
                        if (strchr(text, ':') == NULL) {
                            assert(textsize*2 < sizeof(pagefn));
                            {
                                char *p = pagefn;
                                for (size_t i = 0; i < textsize; i++) {
                                    if (text[i] == '"' || text[i] == '`') {
                                        *p++ = '\\';
                                    }
                                    *p++ = text[i];
                                }
                                *p = 0;
                            }
                            size_t i = 0;
                            while (true) {
                                i += strcspn(pagefn+i, " /\"");
                                if (pagefn[i] == 0) {
                                    break;
                                }
                                pagefn[i] = '_';
                            }
                            char cmd[2000];
                            snprintf(cmd, sizeof(cmd), "xz >\"%s/%s.xz\"", outdir, pagefn);
                            outfile = popen(cmd, "w");
                            if (outfile == NULL) {
                                perror("outfile");
                            }
                        }
                    }
                } else if (strncmp(a, "<id>", 4) == 0 && firstid) {
                    textsize = 0;
                    textcap = true;
                } else if (strncmp(a, "</id>", 5) == 0 && textcap) {
                    textcap = false;
                    if (outfile != NULL && firstid) {
                        text[textsize] = 0;
                        //fwrite(text, 1, textsize+1, outfile);
                        id = text;
                    }
                    firstid = false;
                } else if (strncmp(a, "<timestamp>", 11) == 0) {
                    textsize = 0;
                    textcap = true;
                } else if (strncmp(a, "</timestamp>", 12) == 0) {
                    textcap = false;
                    if (outfile != NULL) {
                        text[textsize] = 0;
                        //fwrite(text, 1, textsize+1, outfile);
                        timestamp = text;
                    }
                } else if (strncmp(a, "<contributor>", 13) == 0) {
                    textsize = 0;
                    textcap = true;
                } else if (strncmp(a, "</contributor>", 14) == 0) {
                    textcap = false;
                    if (outfile != NULL) {
                        text[textsize] = 0;
                        //fwrite(text, 1, textsize+1, outfile);
                        contributor = text;
                    }
                } else if (strncmp(a, "<text", 5) == 0) {
                    if (outfile != NULL) {
                        assert(not id.empty());
                        fwrite(id.c_str(), 1, id.length()+1, outfile);
                        assert(not timestamp.empty());
                        fwrite(timestamp.c_str(), 1, timestamp.length()+1, outfile);
                        if (not contributor.empty()) {
                            fwrite(contributor.c_str(), 1, contributor.length()+1, outfile);
                        } else {
                            fwrite("unknown", 1, 7+1, outfile);
                        }
                    }
                    if (e[-1] != '/') {
                        textcopy = true;
                    }
                    textfirstchar = 0;
                } else if (strncmp(a, "</text>", 7) == 0) {
                    assert(textcopy);
                    textcopy = false;
                }
                //fwrite(a, 1, e-a+1, stdout);
                //puts("");
                p = e + 1 - buf;
            } else {
                if (textcap) {
                    size_t n = size - p;
                    assert(textsize + n <= sizeof(text));
                    memcpy(text+textsize, buf+p, n);
                    textsize += n;
                } else if (textcopy) {
                    if (textfirstchar == 0) {
                        textfirstchar = buf[p];
                    }
                    if (outfile) {
                        fwrite(buf+p, 1, size - p, outfile);
                    }
                }
                p = size;
            }
        }
        p = 0;
    }
    pclose(f);
}
