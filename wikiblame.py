# wikiblame - edit attribution for mediawiki
# Copyright (c) 2013  Greg Hewgill <greg@hewgill.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

import difflib
import hashlib
import os
import pprint
import re
import shutil
import subprocess
import sys

class Info(object):
    def __init__(self, revid, timestamp, user):
        self.revid = revid
        self.timestamp = timestamp
        m = re.match(r"\s*(.*?)\s+(\d+)$", user)
        if m:
            self.user = m.group(1) #+ " " + m.group(2)
        else:
            self.user = user.strip()
    def __repr__(self):
        return (self.revid, self.timestamp, self.user).__repr__()

def group(f, a):
    r = []
    if a:
        r = [a[0]]
        for i in a[1:]:
            if f(i) == f(r[-1]):
                r.append(i)
            else:
                yield r
                r = [i]
    yield r

wikiname = sys.argv[1]
srcdir = sys.argv[2]
pagename = sys.argv[3]
destdir = sys.argv[4]
wordre = re.compile(r"(&\w+;|[\w\d]+)")

print "---"
print pagename
print "Reading page history"
fn = os.path.join(srcdir, "%s.xz" % pagename)
if not os.access(fn, os.F_OK):
    fn = os.path.join(srcdir, pagename)
p = subprocess.Popen(["xz", "-d", "-c", fn], stdout=subprocess.PIPE)
data = p.stdout.read()

z = [0]
i = 0
while True:
    i = data.find("\0", i)
    if i < 0:
        break
    i += 1
    z.append(i)
assert (len(z) - 1) % 4 == 0
revisions = []
current = None
for i in xrange(0, len(z)-1, 4):
    current = data[z[i+3]:z[i+4]-1]
    revisions.append((Info(data[z[i]:z[i+1]-1], data[z[i+1]:z[i+2]-1], data[z[i+2]:z[i+3]-1]), (z[i+3], z[i+4]-1)))
print len(revisions), "raw revisions"

print "Creating linear history"
revisions.sort(key = lambda x: x[0].timestamp)
hashes = [hashlib.sha1(re.sub(r"\s+", " ", data[x[1][0]:x[1][1]])).hexdigest() for x in revisions]
earliest = {}
for i, h in reversed(list(enumerate(hashes))):
    earliest[h] = i
linrevs = []
i = len(revisions) - 1
while True:
    e = earliest[hashes[i]]
    assert e <= i
    if i == e:
        linrevs.append(revisions[i])
        i -= 1
        if i < 0:
            break
    else:
        i = e
linrevs.reverse()
#pprint.pprint([x[0] for x in linrevs])
print len(linrevs), "linear revisions"

print "Building differences"
lasttext = []
words = []
count = 0
for info, page in linrevs:
    text = wordre.findall(data[page[0]:page[1]])
    m = difflib.SequenceMatcher(lambda x: False, lasttext, text)
    blocks = m.get_matching_blocks()
    newwords = []
    lastpos = 0
    for i in xrange(len(blocks)):
        newwords.extend([(x, info) for x in text[lastpos:blocks[i][1]]])
        newwords.extend(words[blocks[i][0]:blocks[i][0]+blocks[i][2]])
        lastpos = blocks[i][1] + blocks[i][2]
    words = newwords
    lasttext = text
    count += 1
    sys.stdout.write(" %d/%d\r" % (count, len(linrevs)))
    sys.stdout.flush()

print "Writing output"
#pprint.pprint(words)
f = open(os.path.join(destdir, "%s.html" % pagename), "w")
print >>f, "<html>"
print >>f, "<head>"
print >>f, "<title>%s</title>" % pagename
print >>f, """<meta http-equiv="Content-type" content="text/html; charset=utf-8">"""
print >>f, """<script type="text/javascript" src="http://ajax.googleapis.com/ajax/libs/jquery/1.2.6/jquery.js"></script>"""
print >>f, """
    <script type="text/javascript">
        function opendiff(e) {
            var a = $(e).attr("className");
            var s = a.split(" ");
            for (var i in s) {
                if (s[i].substr(0, 2) == "r_") {
                    window.open("http://%(wikiname)s.wikipedia.org/w/index.php?title=%(pagename)s&diff=prev&oldid="+s[i].substr(2));
                    break;
                }
            }
        }
        function setbackground(e, value) {
            var a = $(e).attr("className");
            var s = a.split(" ");
            for (var i in s) {
                if (s[i].substr(0, 2) == "r_") {
                    $("."+s[i]).css("background", value);
                    break;
                }
            }
        }
        $(document).ready(function() {
            $(".m").each(function(i) {
                this.onclick = function() { opendiff(this); };
                this.onmouseover = function() { setbackground(this, "yellow"); };
                this.onmouseout = function() { setbackground(this, ""); };
            });
        });
    </script>
""" % {
    'wikiname': wikiname,
    'pagename': pagename,
}
print >>f, "</head>"
print >>f, "<body>"
#print >>f, "<pre>"
p = 0
for a in group(lambda x: x[1][1].revid, zip(wordre.finditer(current), words)):
    f.write(re.sub("\n", "<br>\n", current[p:a[0][0].start()]))
    p = a[0][0].end()
    f.write("""<span class="r_%s m" title="%s %s">""" % (re.sub(r"[-:]", "", a[0][1][1].revid), a[0][1][1].timestamp, a[0][1][1].user))
    first = True
    for word, info in a:
        if not first:
            f.write(re.sub("\n", "<br>\n", current[p:word.start()]))
            p = word.end()
        f.write(word.group(0))
        first = False
    f.write("""</span>""")
f.write(current[p:])
#print >>f, "</pre>"
print >>f, "</body>"
print >>f, "</html>"
f.close()
