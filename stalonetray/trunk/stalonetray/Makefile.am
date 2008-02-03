SUBDIRS=src

man_MANS=stalonetray.1

# debian_DIST=debian/control debian/rules debian/changelog\
#             debian/compat debian/copyright debian/docs\
#             debian/dirs debian/menu debian/manpages

doc_DIST=stalonetrayrc.sample stalonetray.1 stalonetray.xml stalonetray.html

EXTRA_DIST=$(doc_DIST) stalonetray.spec stalonetray.xml.in stalonetrayrc.sample.in # $(debian_DIST)

MAINTAINERCLEANFILES=stalonetray.1 stalonetray.html stalonetray.spec stalonetray.xml stalonetrayrc.sample ChangeLog

dist_hook: stalonetray.1 stalonetray.html

stalonetray.xml: stalonetray.xml.in configure.in
stalonetrayrc.sample: stalonetrayrc.sample.in configure.in
stalonetray.spec: stalonetray.spec.in configure.in

stalonetray.xml \
stalonetrayrc.sample \
stalonetray.spec:
	cat $< | sed 's/@VERSION_STR@/@VERSION@/g' > $@

stalonetray.1: stalonetray.xml
	xsltproc /usr/share/xml/docbook/stylesheet/nwalsh/manpages/docbook.xsl stalonetray.xml

stalonetray.html: stalonetray.xml
	xsltproc /usr/share/xml/docbook/stylesheet/nwalsh/xhtml/docbook.xsl stalonetray.xml > stalonetray.html

ChangeLog: .svn/entries src/.svn/entries
	svn update
	svn2cl --linelen=90 --break-before-msg=2

#docdir=$(datadir)/doc/$(PACKAGE)/
#install-data-local:
#	mkdir -p $(DESTDIR)/$(docdir)
#	cp AUTHORS BUGS COPYING NEWS README stalonetrayrc.sample $(DESTDIR)/$(docdir)

