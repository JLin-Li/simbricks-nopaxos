d := $(dir $(lastword $(MAKEFILE_LIST)))

include $(d)unreplicated/Rules.mk
