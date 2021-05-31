d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
	message.cc client.cc server.cc fcor.cc)

PROTOS += $(addprefix $(d), eris-proto.proto)

OBJS-eris-client := $(o)client.o $(o)message.o $(o)eris-proto.o \
    $(OBJS-client) $(LIB-pbmessage)

OBJS-eris-server := $(o)server.o $(o)message.o $(o)eris-proto.o \
    $(OBJS-replica) $(LIB-message) \
    $(LIB-configuration) $(LIB-latency) \
    $(OBJS-vr-client)

OBJS-eris-fcor := $(o)fcor.o $(o)message.o $(o)eris-proto.o \
    $(LIB-message) $(LIB-configuration) $(OBJS-replica)
