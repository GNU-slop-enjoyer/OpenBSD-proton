# OpenBSD Proton.  On OpenBSD 8.0 amd64, run "make" as your normal user:
# it checks the system, builds the ports in ports/ and installs them
# (privileged steps go through doas and are shown first).
# See docs/USING.md and scripts/openbsd-proton-setup.

# SETUP_FLAGS=-y answers yes to the root steps; see the script's usage.
SETUP =	./scripts/openbsd-proton-setup ${SETUP_FLAGS}

all install:
	@${SETUP} install

update:
	@git pull --ff-only && ${SETUP} install

check:
	@${SETUP} check

dry-run:
	@${SETUP} -n install

uninstall:
	@${SETUP} uninstall

.PHONY: all install update check dry-run uninstall
