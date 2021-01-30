VERSION = 0.8.3
PAGES_IN = index news wmhints manpage
PAGES_OUT = $(PAGES_IN:=.html)

all: generate

clean:
	rm -f $(PAGES_OUT)

superclean: clean
	rm -f manpage_data stalonetray.html stalonetray.xml stalonetray.xml.in

generate: $(PAGES_OUT)

dist:
	tar cfz htdocs.tar.gz *.html images *.txt *.css

manpage: manpage_data

manpage_data: stalonetray.xml
	xsltproc /usr/share/xml/docbook/stylesheet/docbook-xsl/xhtml/docbook.xsl $< \
		| sed -e '1d;2s|^.*<body>||;$$s|</body>.*$$||' > manpage_data

stalonetray.xml: stalonetray.xml.in
	cat $< | sed "s/@VERSION_STR@/$(VERSION)/g" > $@

.PHONY: stalonetray.xml.in
stalonetray.xml.in:
	git show master:stalonetray.xml.in > $@

%.html : %
	cat template.html.in \
		| sed -e "s/@VERSION@/$(VERSION)/g;s/@PAGENAME@/$</g" \
		| cpp -C -P -nostdinc -w -traditional-cpp \
		| sed '1,2d' > $@

