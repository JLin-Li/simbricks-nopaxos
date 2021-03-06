d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
	client.cc benchmark.cc replica.cc)

OBJS-benchmark := $(o)benchmark.o \
                  $(LIB-message) $(LIB-latency)

$(d)client: $(o)client.o $(OBJS-benchmark) $(LIB-udptransport)
$(d)client:	$(OBJS-vr-client) $(OBJS-fastpaxos-client) $(OBJS-unreplicated-client) $(OBJS-nopaxos-client)
$(d)client: $(OBJS-spec-client)

$(d)replica: $(o)replica.o $(LIB-udptransport)
$(d)replica: $(OBJS-vr-replica) $(OBJS-fastpaxos-replica) $(OBJS-unreplicated-replica) $(OBJS-nopaxos-replica)
$(d)replica: $(OBJS-spec-replica)

BINS += $(d)client $(d)replica
