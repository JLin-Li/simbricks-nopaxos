d := $(dir $(lastword $(MAKEFILE_LIST)))

include $(d)vr/Rules.mk $(d)unreplicated/Rules.mk
