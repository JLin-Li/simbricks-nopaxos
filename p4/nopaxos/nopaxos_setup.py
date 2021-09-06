from netaddr import EUI, IPAddress

sys.path.insert(0, os.path.join(os.environ['HOME'], 'inho/dsnet/p4'))
from setup_header import *

p4 = bfrt.nopaxos.pipe
clear_all(p4, verbose=True)
num_groups = 1
num_sequencers = 1

set_mcast(num_groups, num_sequencers)

### Program IPv4 address lookup
ipv4_host = p4.Ingress.ipv4_host
set_ipv4_host(ipv4_host);

### Register initialization
p4.Ingress.reg_cnt.mod(0, 0)

bfrt.complete_operations()

### Register print out
print("""******************* SETUP RESULTS *****************""")
print ("\n reg_cnt:")
p4.Ingress.reg_cnt.get(REGISTER_INDEX=0, from_hw=True)