#!/bin/sh
# Prepare the OpenBSD VM for building ports.  Run on the host after
# scripts/vm/run.sh start; executes privileged steps as root inside the guest
# only (never on the host).
#
#  - installs the build dependencies of emulators/wine from the release's
#    binary packages
#  - copies upstream/ports (without .git) to /usr/ports in the guest
#  - points WRKOBJDIR/PACKAGE_REPOSITORY/DISTDIR at /home (largest partition)
#  - raises the builder user's login class limits for clang/LTO builds
set -eu

TOP=$(cd "$(dirname "$0")/../.." && pwd)
RUN=$TOP/scripts/vm/run.sh
root() { VM_USER=root "$RUN" ssh "$@"; }

root 'cat > /etc/installurl' <<'EOF'
https://cdn.openbsd.org/pub/OpenBSD
EOF

# Build and library dependencies of emulators/wine (see docs/DEPENDENCIES.md).
root pkg_add -I \
	autoconf-2.73 automake-1.18.1 bison nasm gmake git rsync-- \
	llvm-22.1.8p10 \
	pulseaudio-17.0p4 sdl2-2.32.10p1 ffmpeg-9.0.2v1 vulkan-loader vulkan-headers \
	gstreamer1 gst-plugins-base gst-plugins-good \
	gst-plugins-bad gst-plugins-ugly gst-libav \
	gettext-tools libinotify libgphoto sane-backends-- libpcap \
	pcsc-lite gnutls cups-libs dbus-1.16.2p1v0 libxkbcommon gtk+4-- samba-- \
	2>&1 | tee "$TOP/logs/vm-pkg_add.log"

# Ports tree: the working copy, minus git metadata.
root 'mkdir -p /usr/ports && chown builder /usr/ports'
tar -C "$TOP/upstream/ports" --exclude=.git -cf - . |
	"$RUN" ssh 'tar -C /usr/ports -xf -'

root 'mkdir -p /home/pobj /home/packages /home/distfiles /home/plist &&
	chown builder /home/pobj /home/packages /home/distfiles /home/plist'
root 'cat > /etc/mk.conf' <<'EOF'
WRKOBJDIR=/home/pobj
PACKAGE_REPOSITORY=/home/packages
DISTDIR=/home/distfiles
PLIST_REPOSITORY=/home/plist
SUDO=/usr/bin/doas
EOF
root 'echo "permit nopass builder" > /etc/doas.conf'

# Large C++ translation units need more than the default 1.5G datasize.
root 'grep -q "^builder:" /etc/login.conf ||
	printf "builder:\\\\\n\t:datasize=infinity:maxproc=512:openfiles=4096:tc=default:\n" >> /etc/login.conf
	usermod -L builder builder'
root 'df -h; sysctl -n kern.version hw.ncpu hw.physmem'

# USE_WXNEEDED ports (emulators/wine) must build on a wxallowed filesystem;
# WRKOBJDIR lives on /home.
root 'grep -q " /home ffs .*wxallowed" /etc/fstab ||
	{ sed -i "/ \/home ffs /s/rw,/rw,wxallowed,/" /etc/fstab &&
	  mount -u -o wxallowed /home; }; mount | grep " /home "'
