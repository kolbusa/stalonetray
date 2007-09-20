SUBDIRS=src

man_MANS=stalonetray.1

# debian_DIST=debian/control debian/rules debian/changelog\
#             debian/compat debian/copyright debian/docs\
#             debian/dirs debian/menu debian/manpages

doc_DIST=stalonetrayrc.sample stalonetray.1 stalonetray.xml stalonetray.html

EXTRA_DIST=$(doc_DIST) stalonetray.spec $(debian_DIST)

MAINTAINERCLEANFILES=stalonetray.1 stalonetray.html ChangeLog

dist_hook: stalonetray.1 stalonetray.html

stalonetray.1: stalonetray.xml
	xsltproc /usr/share/xml/docbook/stylesheet/nwalsh/manpages/docbook.xsl stalonetray.xml

stalonetray.html: stalonetray.xml
	xsltproc /usr/share/xml/docbook/stylesheet/nwalsh/xhtml/docbook.xsl stalonetray.xml > stalonetray.html

ChangeLog: CVS/Entries src/CVS/Entries
	cvs2cl -P --fsf -S

#docdir=$(datadir)/doc/$(PACKAGE)/
#install-data-local:
#	mkdir -p $(DESTDIR)/$(docdir)
#	cp AUTHORS BUGS COPYING NEWS README stalonetrayrc.sample $(DESTDIR)/$(docdir)

