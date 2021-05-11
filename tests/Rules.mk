d := $(dir $(lastword $(MAKEFILE_LIST)))

include $(d)replication/Rules.mk
