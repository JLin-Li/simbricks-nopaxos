d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
	sequencer.cc)

$(d)sequencer: $(o)sequencer.o $(LIB-message) $(LIB-configuration) $(LIB-udptransport) \
		$(OBJS-nopaxos-sequencer)

BINS += $(d)sequencer
