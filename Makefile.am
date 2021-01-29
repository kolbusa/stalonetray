SUBDIRS=src

man_MANS=stalonetray.1

if have_xsltproc
doc_DIST=stalonetrayrc.sample stalonetray.1 stalonetray.xml stalonetray.html
dist_hook: stalonetray.1 stalonetray.html
endif
EXTRA_DIST=$(doc_DIST) stalonetray.spec stalonetray.xml.in stalonetrayrc.sample.in # $(debian_DIST)
MAINTAINERCLEANFILES=stalonetray.1 stalonetray.html stalonetray.spec stalonetray.xml stalonetrayrc.sample

stalonetray.xml: stalonetray.xml.in configure.ac
stalonetrayrc.sample: stalonetrayrc.sample.in configure.ac
stalonetray.spec: stalonetray.spec.in configure.ac

stalonetray.xml \
stalonetrayrc.sample \
stalonetray.spec:
	VERSION_STR=`echo @VERSION@ | sed 's/\([^\.]*\.[^\.]*\)\..*/\1/'`; \
	cat $< | sed "s/@VERSION_STR@/$$VERSION_STR/g" > $@

if have_xsltproc
stalonetray.1: stalonetray.xml
	$(XSLTPROC) $(XSLTPROC_FLAGS) $(DOCBOOK_ROOT)/manpages/docbook.xsl stalonetray.xml

stalonetray.html: stalonetray.xml
	$(XSLTPROC) $(XSLTPROC_FLAGS) $(DOCBOOK_ROOT)/xhtml/docbook.xsl stalonetray.xml >& $@
endif
