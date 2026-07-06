#!/bin/sh
#
# directhwd - boot-time loader for the DirectHW kernel extension.
#
# On Apple Silicon a third-party kext normally loads from the Auxiliary Kernel
# Collection at boot, but when that collection cannot be rebuilt the kext must be
# live-loaded with `kmutil load`. This script (run by the LaunchDaemon
# com.beako.utils.directhwd at boot, as root) performs that load, with a few
# retries in case IOKit is not fully up early in boot.
#
# It is idempotent: if the kext is already loaded it exits successfully.
#

BUNDLE_ID="com.beako.utils.DirectHW"

log() {
	echo "$(date '+%Y-%m-%dT%H:%M:%S%z') directhwd: $*"
}

if [ "$(id -u)" -ne 0 ]; then
	log "must run as root"
	exit 1
fi

# Already loaded?
if kmutil showloaded 2>/dev/null | grep -q "$BUNDLE_ID"; then
	log "$BUNDLE_ID already loaded"
	exit 0
fi

attempt=1
max=5
while [ "$attempt" -le "$max" ]; do
	if kmutil load -b "$BUNDLE_ID" 2>&1; then
		log "loaded $BUNDLE_ID (attempt $attempt)"
		exit 0
	fi
	log "load attempt $attempt/$max failed, retrying in 2s..."
	attempt=$((attempt + 1))
	sleep 2
done

log "failed to load $BUNDLE_ID after $max attempts"
exit 1
