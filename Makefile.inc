#
# Put user-defined variables here
#

BUNDLE_NAME?=				DirectHW
BUNDLE_MACHO?=				$(BUNDLE_NAME)
BUNDLE_SERVICE=				$(BUNDLE_NAME)Service
BUNDLE_CLIENT=				$(BUNDLE_NAME)UserClient
BUNDLE_VERSION?=			$(strip $(shell cat $(dir $(lastword $(MAKEFILE_LIST)))VERSION 2>/dev/null))
BUNDLE_BUILD?=				1
BUNDLE_DOMAIN?=				com.beako.utils
BUNDLE_ID?=					$(BUNDLE_DOMAIN).$(KEXTNAME)
COPYRIGHT?= 				""
SIGNCERT?= -
