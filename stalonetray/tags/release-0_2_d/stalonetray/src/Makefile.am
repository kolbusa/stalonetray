bin_PROGRAMS = stalonetray
stalonetray_SOURCES = main.c icons.c xembed.c layout.c embed.c settings.c
EXTRA_stalonetray_SOURCES = kde_tray.c
stalonetray_DEPENDENCIES = @kde_tray_support@
INCLUDES = @x_includes@
stalonetray_LDADD = @x_libraries@ @kde_tray_support@
