SUBDIRS = src

.PHONY: ChangeLog INSTALL
INSTALL:
        $(INSTALL_CMD)
ChangeLog:
        $(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
