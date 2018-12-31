import quac

quac.initialize()
q = quac.Instance()
print(q)
print("node_id: {0:d}".format(q.node_id))
print("num_nodes: {0:d}".format(q.num_nodes))

c = quac.Circuit()
print(c)

quac.finalize()
