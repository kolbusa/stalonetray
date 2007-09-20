bin_PROGRAMS = stalonetray
stalonetray_SOURCES = main.c icons.c icons.h xembed.c xembed.h layout.c layout.h embed.c embed.h \
                      settings.c settings.h xutils.c xutils.h tray.c tray.h debug.h list.h
EXTRA_stalonetray_SOURCES = kde_tray.c kde_tray.h
stalonetray_DEPENDENCIES = @kde_tray_support@
INCLUDES = @x_includes@
stalonetray_LDADD = @x_libraries@ @kde_tray_support@
