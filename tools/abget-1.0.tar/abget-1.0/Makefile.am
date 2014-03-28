AM_CPPFLAGS = -D_GNU_SOURCE -D_THREAD_SAFE @PCAPINC@

confdir = $(sysconfdir)/abget

bin_PROGRAMS = abget
conf_DATA = abget.conf

abget_LDADD = @PCAPLIB@ 
abget_SOURCES = inet.c inet.h capture.c capture.h parseconf.c parseconf.h abget.c

# override the default conf install to check for abget.conf overwrite
install-confDATA: $(conf_DATA)
	@$(NORMAL_INSTALL)
	@test -z "$(confdir)" || mkdir -p "$(DESTDIR)$(confdir)"
	@list='$(conf_DATA)'; for p in $$list; do \
		if test -f $$p; then \
			if test -e "$(DESTDIR)$(confdir)/$$p"; then \
				echo -e "\nConfig file $$p already exists in $(DESTDIR)$(confdir) - not overwriting...\n"; \
			else \
				echo " $(INSTALL) -m 644 $$p $(DESTDIR)$(confdir)/$$f"; \
				$(INSTALL) -m 644 $$p $(DESTDIR)$(confdir)/$$f; \
			fi \
		fi \
	done

EXTRA_DIST = COPYING README abget.conf crawler.py findfiles.pl gsearch

