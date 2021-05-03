d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
	sequencer.cc ucs.cc)

$(d)sequencer: $(o)sequencer.o $(LIB-message)
$(d)ucs: $(o)ucs.o $(LIB-message) $(LIB-configuration) $(LIB-udptransport)

BINS += $(d)sequencer $(d)ucs
