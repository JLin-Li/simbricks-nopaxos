d := $(dir $(lastword $(MAKEFILE_LIST)))

include $(d)vr/Rules.mk $(d)fastpaxos/Rules.mk $(d)unreplicated/Rules.mk $(d)spec/Rules.mk
