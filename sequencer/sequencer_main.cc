#include <cstring>
#include <fstream>

#include "lib/udptransport.h"
#include "replication/nopaxos/sequencer.h"
#include "sequencer/sequencer.h"

static void Usage(const char *name) {
  fprintf(stderr, "usage: %s -c conf-file -m nopaxos\n", name);
  exit(1);
}

int main(int argc, char *argv[]) {
  const char *config_path = nullptr;
  dsnet::Sequencer *sequencer = nullptr;
  int opt;

  enum {
    PROTO_UNKNOWN,
    PROTO_NOPAXOS,
  } proto = PROTO_UNKNOWN;

  while ((opt = getopt(argc, argv, "c:m:")) != -1) {
    switch (opt) {
      case 'c':
        config_path = optarg;
        break;

      case 'm':
        if (strcasecmp(optarg, "nopaxos") == 0) {
          proto = PROTO_NOPAXOS;
        } else {
          Panic("Unknown sequencer mode '%s'", optarg);
        }
        break;

      default:
        fprintf(stderr, "Unknown argument %s\n", argv[optind]);
        break;
    }
  }

  if (config_path == nullptr) {
    fprintf(stderr, "option -c is required\n");
    Usage(argv[0]);
  }

  if (proto == PROTO_UNKNOWN) {
    fprintf(stderr, "option -m is required\n");
    Usage(argv[0]);
  }

  std::ifstream config_stream(config_path);
  if (config_stream.fail()) {
    Panic("unable to read configuration file: %s\n", config_path);
  }

  dsnet::Configuration config(config_stream);
  dsnet::UDPTransport transport;
  switch (proto) {
    case PROTO_NOPAXOS:
      sequencer = new dsnet::nopaxos::NOPaxosSequencer(config, &transport, 0);
      break;
    default:
      NOT_REACHABLE();
  }
  transport.Run();
  delete sequencer;

  return 0;
}
